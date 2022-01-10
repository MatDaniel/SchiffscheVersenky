// DispatchRoutine defines the 
module;

#include "BattleShip.h"
#include <functional>

export module DispatchRoutine;
using namespace std;
import Network.Server;
import Network.Client;
using namespace Network;
import GameManagement;
using namespace GameManagementEx;
using namespace GameManagement;

// Layer manager logging object
export SpdLogger LayerLog;

namespace Server {
	using namespace Network::Server;

	void CheckCurrentPhaseAndRaiseClientStatus( // Checks if the request matches the game phase supplied
												// on mismatch the function will raise a status on the client
												// and complete the NRP returning to the manager immediately
		NetworkManager2*        NetworkDevice,  
		NwRequestPacket&        NetworkRequest, // The NRP to check against
		GameManager2::GamePhase WantedPhase,    // The game phase that should be checked for
		size_t                  UniqueIdentifier
	) {
		TRACE_FUNTION_PROTO;

		auto& GameManager = GameManager2::GetInstance();
		if (GameManager.GetCurrentGamePhase() == WantedPhase)
			return;

		// Currently not in the correct game phase, notify caller by posting status and complete NRP
		auto AssociatedClient = NetworkDevice->GetClientBySocket(
			NetworkRequest.RequestingSocket);
		AssociatedClient->RaiseStatusMessageOrComplete(NetworkRequest,
			ShipSockControl::STATUS_NOT_IN_PHASE,
			UniqueIdentifier);
		SPDLOG_LOGGER_WARN(LayerLog, "Cannot execute handler, not in the correct gamestate");
		NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED);
	}

	GmPlayerField& CheckPlayerAndGetFieldOrRaiseClientStatus( // Check if the nrp is dispatched through a registered player
		                                                      // and get player's field, or complete nrp on failure
		NetworkManager2* NetworkDevice,                       // A network manager pointer to the instance holding the player
		NwRequestPacket& NetworkRequest,                      // The NRP to complete on failure
		size_t           UniqueIdentifier
	) {
		TRACE_FUNTION_PROTO;

		// Get Playerfield for SocketAsId stored in NRP if it exists,
		auto& GameManager = GameManager2::GetInstance();
		auto RequestingPlayerField = GameManager.GetPlayerFieldByOperation(
			GameManager2::GET_PLAYER_BY_ID,
			NetworkRequest.RequestingSocket);
		if (RequestingPlayerField)
			return *RequestingPlayerField;

		// otherwise raise a status and complete the NRP
		auto AssociatedClient = NetworkDevice->GetClientBySocket(
			NetworkRequest.RequestingSocket);
		AssociatedClient->RaiseStatusMessageOrComplete(NetworkRequest,
			ShipSockControl::STATUS_NOT_A_PLAYER,
			UniqueIdentifier);
		SPDLOG_LOGGER_WARN(LayerLog, "Requesting client {}, is not a player", 
			NetworkRequest.RequestingSocket);
		NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED);
	}

	void CheckPlayerHasTurnOrRaiseStatus( // Checks if the requesting player has his turn
		NetworkManager2* NetworkDevice,   // A network manager pointer to the instance holding the player
		NwRequestPacket& NetworkRequest,  // The NRP to complete on failure and get the requesting player from
		size_t           UniqueIdentifier
	) {
		TRACE_FUNTION_PROTO;

		auto& GameManager = GameManager2::GetInstance();
		if (NetworkRequest.RequestingSocket == GameManager.GetCurrentSelectedPlayer())
			return;

		auto AssociatedClient = NetworkDevice->GetClientBySocket(
			NetworkRequest.RequestingSocket);
		AssociatedClient->RaiseStatusMessageOrComplete(NetworkRequest,
			ShipSockControl::STATUS_NOT_YOUR_TURN,
			UniqueIdentifier);
		SPDLOG_LOGGER_WARN(LayerLog, "Requesting client {}, does not have his turn", 
			NetworkRequest.RequestingSocket);
		NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED);
	}
}

// Internal server controller and server related utilities/parts of the layer manager
export namespace Server {
	using namespace Network::Server;

	void ManagementDispatchRoutine(
		NetworkManager2* NetworkDevice,
		NwRequestPacket& NetworkRequest,
		void* UserContext,
		ShipSockControl* RequestPackage
	) {
		TRACE_FUNTION_PROTO;

		switch (NetworkRequest.IoControlCode) {
		case NwRequestPacket::INCOMING_CONNECTION: {

			// Accept incoming connection and checkout
			auto& NewClient = NetworkDevice->AllocateConnectionAndAccept(NetworkRequest);
			SPDLOG_LOGGER_INFO(LayerLog, "Successfully connected client to server");

			// Send game dimensions and ship count information, this is send by default always
			auto& GameManager = GameManager2::GetInstance();
			NewClient.DirectSendPackageOutboundOrComplete(NetworkRequest,
				ShipSockControl{
					.ControlCode = ShipSockControl::STARTUP_FIELDSIZE,
					.GameFieldSizes = GameManager.InternalFieldDimensions
				});
			NewClient.DirectSendPackageOutboundOrComplete(NetworkRequest,
				ShipSockControl{
					.ControlCode = ShipSockControl::STARTUP_SHIPCOUNTS,
					.GameShipNumbers = GameManager.InternalShipCount
				});



			// we now need to notify the game manager of the connecting player
			auto NewPlayer = GameManager.TryAllocatePlayerWithId(
				NewClient.GetSocket());
			if (!NewPlayer) {

				// we would need to inform the connecting client of all the current states etc 
				SPDLOG_LOGGER_INFO(LayerLog, "There are already 2 connected players, may only accept viewers now (to be implemented)");

				// TODO: code goes here



				// Skip further handling
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
				return;
			}



			// continue here if registered as play: the server now sends the player an id to associate with
			NewClient.DirectSendPackageOutboundOrComplete(NetworkRequest,
				ShipSockControl{
					.ControlCode = ShipSockControl::STARTUP_YOUR_ID,
					.SocketAsSelectedPlayerId = NewClient.GetSocket()
				});

			SPDLOG_LOGGER_INFO(LayerLog, "Successfully allocated and associated client to player");
			NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
		}

												 // Dispatch ship control handling
		case NwRequestPacket::INCOMING_PACKET: {
			
			switch (RequestPackage->ControlCode) {
			case ShipSockControl::NO_COMMAND_CLIENT:
				SPDLOG_LOGGER_WARN(LayerLog, "Debug NO_COMMAND received");
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED);

			case ShipSockControl::SHOOT_CELL_CLIENT: {

				// Get basic meta data in order to interface with the game etc
				auto& GameManager = GameManager2::GetInstance();
				auto RequestingClientId = NetworkRequest.RequestingSocket;

				// Check if we are in game phase, does not return on error
				CheckCurrentPhaseAndRaiseClientStatus(NetworkDevice,
					NetworkRequest,
					GameManager2::GAME_PHASE,
					RequestPackage->SpecialRequestIdentifier);
				CheckPlayerHasTurnOrRaiseStatus(NetworkDevice,
					NetworkRequest,
					RequestPackage->SpecialRequestIdentifier);

				// Get requesting client and filter out non players, does not return on error
				auto& RequestingPlayerField = CheckPlayerAndGetFieldOrRaiseClientStatus(NetworkDevice,
					NetworkRequest,
					RequestPackage->SpecialRequestIdentifier);
				SPDLOG_LOGGER_INFO(LayerLog, "Applied checks and retrived requesting player {}",
					RequestingClientId);
				auto OpponentPlayerField = GameManager.GetPlayerFieldByOperation(
					GameManager2::GET_OPPONENT_PLAYER,
					RequestingClientId);
				if (!OpponentPlayerField)
					throw runtime_error("Impossible state, there must be a opponent player during GAME_PHASE");
				SPDLOG_LOGGER_INFO(LayerLog, "Retrived requesting players opponent {}",
					GameManager.GetSocketAsIdForPlayerField(*OpponentPlayerField));

				// we have check everything we can now care for the game logic, what we have to do:
				// - update internal player state of opponent
				auto CellUpdated = OpponentPlayerField->StrikeCellAndUpdateShipList(
					RequestPackage->ShootCellLocation);

				// - inform the opponents client of the changes, aka sending him a shoot request
				auto OpponentId = GameManager.GetSocketAsIdForPlayerField(*OpponentPlayerField);
				auto OpponentClient = NetworkDevice->GetClientBySocket(OpponentId);
				OpponentClient->DirectSendPackageOutboundOrComplete(NetworkRequest,
					ShipSockControl{
						.ControlCode = ShipSockControl::SHOOT_CELL_SERVER,
						.SocketAsSelectedPlayerId = OpponentId,
						.ShootCellLocation = RequestPackage->ShootCellLocation
					});
				SPDLOG_LOGGER_INFO(LayerLog, "Send opponent {} state change notification",
					OpponentId);

				// - broadcast the changes made except to the opponents client
				NetworkDevice->BroadcastShipSockControlExcept(NetworkRequest,
					OpponentId,
					ShipSockControl{
						.ControlCode = ShipSockControl::CELL_STATE_SERVER,
						.SocketAsSelectedPlayerId = OpponentId,
						.CellStateUpdate = {
							RequestPackage->ShootCellLocation,
							CellUpdated.value()
					} });
				SPDLOG_LOGGER_INFO(LayerLog, "Broadcasted cell state change to other clients");

				// Notify player of successful strike request
				NetworkRequest.OptionalClient->RaiseStatusMessageOrComplete(NetworkRequest,
					ShipSockControl::STATUS_NEUTRAL,
					RequestPackage->SpecialRequestIdentifier);
				SPDLOG_LOGGER_INFO(LayerLog, "Reported back successful strike notification");
				

				// -> Special case in case strike cell returns sunken we have to check if the party shot lost
				//    and inform all clients about the game over state,
				//    then switch our own state to game over
				if (CellUpdated.value() & STATUS_WAS_DESTRUCTOR) {

					// Broadcast dead shiplocation to all remotes execpt target
					auto ShipStateForShot = OpponentPlayerField->GetShipEntryForCordinate(
						RequestPackage->ShootCellLocation);
					if (!ShipStateForShot)
						throw runtime_error("could not find ship for destroyed shit, impossible state");
					NetworkDevice->BroadcastShipSockControlExcept(NetworkRequest,
						OpponentId,
						ShipSockControl{
							.ControlCode = ShipSockControl::DEAD_SHIP_SERVER,
							.ShipLocation = {
								ShipStateForShot->Cordinates,
								ShipStateForShot->Rotation,
								ShipStateForShot->ShipType
						} });

					// Check if all ships are down
					bool AllDown = true;
					for (auto& Shipstate : OpponentPlayerField->GetShipStateList())
						if (!Shipstate.Destroyed) {
							AllDown = false; break;
						}
					if (AllDown) {

						// All ships have been destroyed, notify clients of game over
						if (!GameManager.EndCurrentGame()) {

							SPDLOG_LOGGER_ERROR(LayerLog, "Failed to terminate game state");
							NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR);
						}

						NetworkDevice->BroadcastShipSockControlExcept(NetworkRequest,
							INVALID_SOCKET,
							ShipSockControl{
								.ControlCode = ShipSockControl::RAISE_STATUS_MESSAGE,
								.SocketAsSelectedPlayerId = RequestingClientId,
								.ShipControlRaisedStatus = ShipSockControl::STATUS_GAME_OVER 
							});
						SPDLOG_LOGGER_INFO(LayerLog, "Reported sucken ship to all clients");
					}
				}

				// Notifiy opponent of turn and complete
				auto NewPlayersTurnId = GameManager.SwitchPlayersTurnState();
				OpponentClient->RaiseStatusMessageOrComplete(NetworkRequest,
					ShipSockControl::STATUS_YOUR_TURN, 0);
				SPDLOG_LOGGER_INFO(LayerLog, "Informed opponent of having turn now");
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}
			case ShipSockControl::SET_SHIP_LOC_CLIENT: {

				// Check game phase state and if requesting client is valid player
				CheckCurrentPhaseAndRaiseClientStatus(NetworkDevice,
					NetworkRequest,
					GameManager2::SETUP_PHASE,
					RequestPackage->SpecialRequestIdentifier);
				auto& RequestingPlayer = CheckPlayerAndGetFieldOrRaiseClientStatus(NetworkDevice,
					NetworkRequest,
					RequestPackage->SpecialRequestIdentifier);
				SPDLOG_LOGGER_INFO(LayerLog, "Applied checks and retrived requesting player {}",
					NetworkRequest.RequestingSocket);

				// - add the ship to the players field
				auto Result = RequestingPlayer.PlaceShipSecureCheckInterference(
					RequestPackage->ShipLocation.TypeOfShip,
					RequestPackage->ShipLocation.Rotation,
					RequestPackage->ShipLocation.ShipsPosition);
				if (Result != GmPlayerField::STATUS_SHIP_PLACED) {

					SPDLOG_LOGGER_ERROR(LayerLog, "Could not place ship, client {} seemingly didnt check correctly or attempted to cheat",
						NetworkRequest.RequestingSocket);
					NetworkRequest.OptionalClient->RaiseStatusMessageOrComplete(NetworkRequest,
						ShipSockControl::STATUS_INVALID_PLACEMENT,
						RequestPackage->SpecialRequestIdentifier);
					NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED);
				}

				// Complete request, notify player of successful placement
				NetworkRequest.OptionalClient->RaiseStatusMessageOrComplete(NetworkRequest,
					ShipSockControl::STATUS_NEUTRAL,
					RequestPackage->SpecialRequestIdentifier);
				SPDLOG_LOGGER_INFO(LayerLog, "Successfully received and placed ship on board");
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}
			case ShipSockControl::REMOVE_SHIP_CLIENT: {

				// Check game phase state and if requesting client is valid player
				CheckCurrentPhaseAndRaiseClientStatus(NetworkDevice,
					NetworkRequest,
					GameManager2::SETUP_PHASE,
					RequestPackage->SpecialRequestIdentifier);
				auto& RequestingPlayer = CheckPlayerAndGetFieldOrRaiseClientStatus(NetworkDevice,
					NetworkRequest,
					RequestPackage->SpecialRequestIdentifier);
				SPDLOG_LOGGER_INFO(LayerLog, "Applied checks and retrived requesting player {}",
					NetworkRequest.RequestingSocket);

				// Remove the ship from the field
				auto Result = RequestingPlayer.RemoveShipFromField(
					RequestPackage->RemoveShipLocation);
				if (Result.value().Cordinates.ZComponent) {

					// Failed to remove ship from field, either no ship existed there,
					// or something else went terribly wrong
					SPDLOG_LOGGER_WARN(LayerLog, "Failed to remove ship from field, location not found");
					NetworkRequest.OptionalClient->RaiseStatusMessageOrComplete(NetworkRequest,
						ShipSockControl::STATUS_ENTRY_NOT_FOUND,
						RequestPackage->SpecialRequestIdentifier);
					NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED);
				}

				// Complete request, notify player of successful removal
				NetworkRequest.OptionalClient->RaiseStatusMessageOrComplete(NetworkRequest,
					ShipSockControl::STATUS_NEUTRAL,
					RequestPackage->SpecialRequestIdentifier);
				SPDLOG_LOGGER_INFO(LayerLog, "Successfully received and removed ship form field");
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}
			case ShipSockControl::READY_UP_CLIENT: {

				// Set the client to ready
				CheckCurrentPhaseAndRaiseClientStatus(NetworkDevice,
					NetworkRequest,
					GameManager2::SETUP_PHASE,
					RequestPackage->SpecialRequestIdentifier);
				auto& RequestingPlayer = CheckPlayerAndGetFieldOrRaiseClientStatus(NetworkDevice,
					NetworkRequest,
					RequestPackage->SpecialRequestIdentifier);
				SPDLOG_LOGGER_INFO(LayerLog, "Applied checks and retrived requesting player {}",
					NetworkRequest.RequestingSocket);

				auto& GameManager = GameManager2::GetInstance();
				auto Result = GameManager.ReadyUpPlayerByPlayer(RequestingPlayer);

				if (Result < 0) {

					NetworkRequest.OptionalClient->RaiseStatusMessageOrComplete(NetworkRequest,
						ShipSockControl::STATUS_NO_READY_STATE,
						RequestPackage->SpecialRequestIdentifier);
					SPDLOG_LOGGER_WARN(LayerLog, "Client wanted to ready up with incomplete state");
					NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED);
				}

				NetworkRequest.OptionalClient->RaiseStatusMessageOrComplete(NetworkRequest,
						ShipSockControl::STATUS_YOURE_READY_NOW,
						RequestPackage->SpecialRequestIdentifier);
				SPDLOG_LOGGER_INFO(LayerLog, "Player was informed of being ready on server");

				if (Result == GameManager2::STATUS_PLAYERS_READY) {
					
					// Broadcast game has started message
					NetworkDevice->BroadcastShipSockControlExcept(NetworkRequest,
						INVALID_SOCKET,
						ShipSockControl{
							.ControlCode = ShipSockControl::GAME_READY_SERVER
						});
					SPDLOG_LOGGER_INFO(LayerLog, "Game hast started, all players informed");

					// Inform random selected player of being ready
					auto RandomPlayer = GameManager.SelectRandomCurrentPlayer();
					auto ClientControllerOfSelect = NetworkDevice->GetClientBySocket(RandomPlayer);
					ClientControllerOfSelect->RaiseStatusMessageOrComplete(NetworkRequest,
							ShipSockControl::STATUS_YOUR_TURN, 0);
					SPDLOG_LOGGER_INFO(LayerLog, "Selected player as {} as starter",
						RandomPlayer);
				}

				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}

			default:
				SPDLOG_LOGGER_WARN(LayerLog, "Untreated ShipSockControl command encountered, this is fine for now");
				return;
			}
			break;

		case NwRequestPacket::SOCKET_DISCONNECTED: {

			// Further handling is required can be ignored for now
			SPDLOG_LOGGER_WARN(LayerLog, "Socket {} was gracefully disconnected",
				NetworkRequest.OptionalClient->GetSocket());
			NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_SOCKET_DISCONNECTED);
		}

		default:
			SPDLOG_LOGGER_CRITICAL(LayerLog, "Io request left untreated, fatal");
		}}
	}
}

namespace Client {
	using namespace Network::Client;

	// List of queued requests that require response
	map<size_t, unique_ptr<CallbackInterface>> QueuedForStatus;
	SOCKET ServerRemoteIdForMyPlayer = INVALID_SOCKET;

	bool PlacementCallbackEx(
		      GmPlayerField*     OptionalField,
		const CallbackInterface& PlacementInterface
	) {
		TRACE_FUNTION_PROTO;

		// Get basic requirements and meta data
		auto& NetworkDevice = NetworkManager2::GetInstance();
		auto& GameManager = GameManager2::GetInstance();
		auto OurPlayer = GameManager.GetPlayerFieldByOperation(GameManager2::GET_MY_PLAYER,
			INVALID_SOCKET);

		// Check if current player is our own
		if (OurPlayer != OptionalField) {
			// if opponent player: 
			// Ship placement will only be updated by the server, 
			// there has to be no async request therefore the exeution goes directly through
			SPDLOG_LOGGER_INFO(GameLogEx, "Placed Ship in opponent, no controlflow injection required");
			return false;
		}

		// Check if there is any request that has already been handled for us
		auto& DownPlaecment = dynamic_cast<const ShipPlacementEx&>(PlacementInterface);
		auto Iterator = find_if(QueuedForStatus.begin(),
			QueuedForStatus.end(),
			[&DownPlaecment](
				decltype(QueuedForStatus)::const_reference MapEntry
				) -> bool {
					TRACE_FUNTION_PROTO;

					if (MapEntry.second->TypeTag_Index != PLACEMENT_ICALLBACK_INDEX)
						return false;
					auto& Placement = dynamic_cast<ShipPlacementEx&>(*MapEntry.second);
					return Placement.ShipCoordinates == DownPlaecment.ShipCoordinates
						&& Placement.ShipOrientation == DownPlaecment.ShipOrientation
						&& Placement.TypeOfShip == DownPlaecment.TypeOfShip;
			});

		if (Iterator != QueuedForStatus.end()) {

			// Check if network has received a status for this if not the rquest is still being handled
			SPDLOG_LOGGER_INFO(GameLogEx, "Found queued entry for requested placment");
			if (!Iterator->second->StatusReceived)
				return true;

			// We have received a status, check if this is acceptable, resulting in not altering control flow
			SPDLOG_LOGGER_INFO(GameLogEx, "Request was treated by server");
			auto Acceptability = Iterator->second->StatusAcceptable;
			QueuedForStatus.erase(Iterator);
			return Acceptability ? false : true;
		}

		// if its not received we have to queue the request on the socket through dispatch
		ShipSockControl RequestPackage{
			.SpecialRequestIdentifier = GenerateUniqueIdentifier(),
			.ControlCode = ShipSockControl::SET_SHIP_LOC_CLIENT,
			.SocketAsSelectedPlayerId = ServerRemoteIdForMyPlayer,
			.ShipLocation = {
				DownPlaecment.ShipCoordinates,
				DownPlaecment.ShipOrientation,
				DownPlaecment.TypeOfShip
		}};
		NetworkDevice.QueueShipControlPacket(RequestPackage);
		QueuedForStatus.emplace(RequestPackage.SpecialRequestIdentifier,
			make_unique<ShipPlacementEx>(DownPlaecment));
		SPDLOG_LOGGER_INFO(GameLogEx, "Placed request into async placment que, waiting for server async");
		return true;
	}

	bool StrikeFieldUpdateCallbackEx(
		      GmPlayerField*     OptionalField,
		const CallbackInterface& TargetLocation
	) {
		TRACE_FUNTION_PROTO;

		// Get basic requirements and meta data
		auto& NetworkDevice = NetworkManager2::GetInstance();
		auto& GameManager = GameManager2::GetInstance();
		auto OpponentPlayer = GameManager.GetPlayerFieldByOperation(GameManager2::GET_OPPONENT_PLAYER,
			INVALID_SOCKET);

		// Check if current player is our own
		if (OpponentPlayer != OptionalField) {

			// Our own filed will effectively only be ever updated by the remote
			// (our force updated by the client for whatever reason)
			// this means the callback wont have to inject controlflow ever for this case
			SPDLOG_LOGGER_INFO(GameLogEx, "struck own field, no controlflow injection required");
			return false;
		}

		// Check if there is any request that has already been handled for us
		auto& DownTargetLocation = dynamic_cast<const FieldStrikeLocationEx&>(TargetLocation);
		auto Iterator = find_if(QueuedForStatus.begin(),
			QueuedForStatus.end(),
			[&DownTargetLocation](
				decltype(QueuedForStatus)::const_reference MapEntry
				) -> bool {
					TRACE_FUNTION_PROTO;

					if (MapEntry.second->TypeTag_Index != SRITECELL_ICALLBACK_INDEX)
						return false;
					auto& TargetLocation = dynamic_cast<FieldStrikeLocationEx&>(*MapEntry.second);
					return TargetLocation.StrikeLocation == DownTargetLocation.StrikeLocation;
			});

		// 
		if (Iterator != QueuedForStatus.end()) {

			// Check if network has received a status for this if not the rquest is still being handled
			SPDLOG_LOGGER_INFO(GameLogEx, "Found queued entry for requested strike");
			if (!Iterator->second->StatusReceived)
				return true;

			// We have received a status, check if this is acceptable, resulting in not altering control flow
			SPDLOG_LOGGER_INFO(GameLogEx, "Request was treated by server");
			auto Acceptability = Iterator->second->StatusAcceptable;
			QueuedForStatus.erase(Iterator);
			return Acceptability ? false : true;
		}

		// if its not received we have to queue the request on the socket through dispatch
		ShipSockControl RequestPackage{
			.SpecialRequestIdentifier = GenerateUniqueIdentifier(),
			.ControlCode = ShipSockControl::SHOOT_CELL_CLIENT,
			.SocketAsSelectedPlayerId = ServerRemoteIdForMyPlayer,
			.ShootCellLocation = DownTargetLocation.StrikeLocation
		};
		NetworkDevice.QueueShipControlPacket(RequestPackage);
		QueuedForStatus.emplace(RequestPackage.SpecialRequestIdentifier,
			make_unique<FieldStrikeLocationEx>(DownTargetLocation));
		SPDLOG_LOGGER_INFO(GameLogEx, "Placed request into async queue, waiting for server async");
		return true;
	}

	bool RemoveShipFromFieldCallbackEx(
		      GmPlayerField*     OptionalField,
		const CallbackInterface& RemoveLocation
	) {
		TRACE_FUNTION_PROTO;

		// Get basic requirements and meta data
		auto& NetworkDevice = NetworkManager2::GetInstance();
		auto& GameManager = GameManager2::GetInstance();
		auto OurField = GameManager.GetPlayerFieldByOperation(GameManager2::GET_MY_PLAYER,
			INVALID_SOCKET);

		// Check if current player is our own
		if (OurField != OptionalField) {

			// If the field is of the opponents then we know this call is impossible,
			// therefore we permit it as the client forces removal on whatever (idc)
			SPDLOG_LOGGER_INFO(GameLogEx, "removal of opponent ship?, no controlflow injection required");
			return false;
		}

		// Check if there is any request that has already been handled for us
		auto& DownTargetLocation = dynamic_cast<const RemoveShipLocationEx&>(RemoveLocation);
		auto Iterator = find_if(QueuedForStatus.begin(),
			QueuedForStatus.end(),
			[&DownTargetLocation](
				decltype(QueuedForStatus)::const_reference MapEntry
				) -> bool {
					TRACE_FUNTION_PROTO;

					if (MapEntry.second->TypeTag_Index != REMOVESHI_ICALLBACK_INDEX)
						return false;
					auto& TargetLocation = dynamic_cast<RemoveShipLocationEx&>(*MapEntry.second);
					return TargetLocation.OverlapRemoveLoc == DownTargetLocation.OverlapRemoveLoc;
			});

		// 
		if (Iterator != QueuedForStatus.end()) {

			// Check if network has received a status for this if not the request is still being handled
			SPDLOG_LOGGER_INFO(GameLogEx, "Found queued entry for requested removal");
			if (!Iterator->second->StatusReceived)
				return true;

			// We have received a status, check if this is acceptable, resulting in not altering control flow
			SPDLOG_LOGGER_INFO(GameLogEx, "Request was treated by server");
			auto Acceptability = Iterator->second->StatusAcceptable;
			QueuedForStatus.erase(Iterator);
			return Acceptability ? false : true;
		}

		// if its not received we have to queue the request on the socket through dispatch
		ShipSockControl RequestPackage{
			.SpecialRequestIdentifier = GenerateUniqueIdentifier(),
			.ControlCode = ShipSockControl::REMOVE_SHIP_CLIENT,
			.SocketAsSelectedPlayerId = ServerRemoteIdForMyPlayer,
			.ShootCellLocation = DownTargetLocation.OverlapRemoveLoc
		};
		NetworkDevice.QueueShipControlPacket(RequestPackage);
		QueuedForStatus.emplace(RequestPackage.SpecialRequestIdentifier,
			make_unique<RemoveShipLocationEx>(DownTargetLocation));
		SPDLOG_LOGGER_INFO(GameLogEx, "Placed request into async queue, waiting for server async");
		return true;
	}
}


// Internal client controller and client related utilities/parts of the layer manager
export namespace Client {
	using namespace Network::Client;

	void InstallGameManagerInstrumentationCallbacks( // Installs network instrumentation callbacks into the gamemanager,
													 // this allows the endpoint to use the game manager as its primary interface
													 // without having to worry about networking which is taken care of 
													 // in the background during execution
		NetworkManager2* NetworkDevice
	) {
		auto& GameManager = GameManager2::GetInstance();

		GameManager.ExCallbacks[PLACEMENT_ICALLBACK_INDEX] = PlacementCallbackEx;
		GameManager.ExCallbacks[SRITECELL_ICALLBACK_INDEX] = StrikeFieldUpdateCallbackEx;
		GameManager.ExCallbacks[REMOVESHI_ICALLBACK_INDEX] = RemoveShipFromFieldCallbackEx;
		SPDLOG_LOGGER_WARN(GameLogEx, "Installed GameEX callbacks into gamemanager");
	}



	enum QueueReadyUpResponse {
		QUE_COULD_NOT_READY_UP = -2000,
		QUE_NOINFO = 0,
		QUE_QUEUED_FOR_READY,
		QUE_READYED_UP,
	};
	QueueReadyUpResponse QueueServerReadyAndPoll() {
		TRACE_FUNTION_PROTO;

		// Get basic requirements and meta data
		auto& NetworkDevice = NetworkManager2::GetInstance();
		auto& GameManager = GameManager2::GetInstance();
		auto OurField = GameManager.GetPlayerFieldByOperation(GameManager2::GET_MY_PLAYER,
			INVALID_SOCKET);

		// Check if there is any request that has already been handled for us
		auto Iterator = find_if(QueuedForStatus.begin(),
			QueuedForStatus.end(),
			[](
				decltype(QueuedForStatus)::const_reference MapEntry
				) -> bool {
					TRACE_FUNTION_PROTO;

					if (MapEntry.second->TypeTag_Index == READYUPPL_ICALLBACK_INDEX)
						return true;
			});

		// 
		if (Iterator != QueuedForStatus.end()) {

			// Check if network has received a status for this if not the request is still being handled
			SPDLOG_LOGGER_INFO(GameLogEx, "Found queued entry for requested removal");
			if (!Iterator->second->StatusReceived)
				return QUE_NOINFO;

			// We have received a status, check if this is acceptable, resulting in not altering control flow
			SPDLOG_LOGGER_INFO(GameLogEx, "Request was treated by server");
			auto Acceptability = Iterator->second->StatusAcceptable;
			QueuedForStatus.erase(Iterator);
			return Acceptability ? QUE_READYED_UP : QUE_COULD_NOT_READY_UP;
		}

		// if its not received we have to queue the request on the socket through dispatch
		ShipSockControl RequestPackage{
			.SpecialRequestIdentifier = GenerateUniqueIdentifier(),
			.ControlCode = ShipSockControl::READY_UP_CLIENT,
			.SocketAsSelectedPlayerId = ServerRemoteIdForMyPlayer
		};
		NetworkDevice.QueueShipControlPacket(RequestPackage);
		QueuedForStatus.emplace(RequestPackage.SpecialRequestIdentifier,
			make_unique<CallbackInterface>(READYUPPL_ICALLBACK_INDEX));
		SPDLOG_LOGGER_INFO(GameLogEx, "Placed request into async queue, waiting for server async");
		return QUE_QUEUED_FOR_READY;
	}


	struct ManagementDispatchState {
		PointComponent InternalFieldDimensions;
		ShipCount      NumberOFShipsPerType;
		bool           StateReady;
		SOCKET         ClientIdByServer;

		enum GameOverState {
			NOT_VALID_YET = 0,
			MY_PLAYER_WON,
			MY_PALYER_LOST,
		} GameOverStateRep;

		bool GameHasStarted;
		bool YouAreReadServerSide;
		bool YourClientHasTurn;
	};

	void ManagementDispatchRoutine(
		NetworkManager2* NetworkDevice,
		NwRequestPacket& NetworkRequest,
		void*            UserContext,
		ShipSockControl* IoControlPacketData
	) {
		TRACE_FUNTION_PROTO;

		switch (NetworkRequest.IoControlCode) {
		case NwRequestPacket::INCOMING_PACKET:

			switch (IoControlPacketData->ControlCode) {
			case ShipSockControl::NO_COMMAND_SERVER:
				SPDLOG_LOGGER_INFO(LayerLog, "Debug command received, invalid handling");
				__debugbreak();
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);

			case ShipSockControl::STARTUP_FIELDSIZE: {

				// Get Engine passed init state and set coords
				auto DispatchState = (ManagementDispatchState*)UserContext;
				DispatchState->InternalFieldDimensions = IoControlPacketData->GameFieldSizes;
				SPDLOG_LOGGER_INFO(LayerLog, "Recorded and stored server defined playfield size {}",
					IoControlPacketData->GameFieldSizes);
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}
			case ShipSockControl::STARTUP_SHIPCOUNTS: {

				// Get Engine passed init state and set shipnumbers
				auto DispatchState = (ManagementDispatchState*)UserContext;
				DispatchState->NumberOFShipsPerType = IoControlPacketData->GameShipNumbers;
				DispatchState->StateReady = true; // hacky, will refine later
				SPDLOG_LOGGER_INFO(LayerLog, "Recorded and stored server defined shipcounts");
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}
			case ShipSockControl::STARTUP_YOUR_ID: {

				// At this point the game manager should already be valid
				ServerRemoteIdForMyPlayer = IoControlPacketData->SocketAsSelectedPlayerId;
				auto DispatchState = (ManagementDispatchState*)UserContext;
				DispatchState->ClientIdByServer = ServerRemoteIdForMyPlayer;
				SPDLOG_LOGGER_INFO(LayerLog, "Server associated us with {}",
					ServerRemoteIdForMyPlayer);
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}
			case ShipSockControl::SHOOT_CELL_SERVER: {

				// if we get this means the server fucking shot us and will kill us if we dont listen to that gay MF'er,
				// btw we know that if we call Strike our own filed the callback will never intervein
				auto& GameManager = GameManager2::GetInstance();
				auto OurPlayerField = GameManager.GetPlayerFieldByOperation(
					GameManager2::GET_MY_PLAYER,
					INVALID_SOCKET);
				auto Result = OurPlayerField->StrikeCellAndUpdateShipList(
					IoControlPacketData->ShootCellLocation);
				if (!Result)
					throw runtime_error("Fucking dammit this shit cant return an optional state here");
				SPDLOG_LOGGER_INFO(LayerLog, "Players field was successfully shot by server");
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}
			case ShipSockControl::CELL_STATE_SERVER: {
				
				// if we receive this we know that the opponent field gets updated 
				// (for viewers the player will be identified through its id)
				// Update specific cell state
				auto& GameManager = GameManager2::GetInstance();
				auto OpponentPlayer = GameManager.GetPlayerFieldByOperation(
					GameManager2::GET_OPPONENT_PLAYER,
					INVALID_SOCKET);
				auto Result = OpponentPlayer->GetCellStateByCoordinates(
					IoControlPacketData->CellStateUpdate.Coordinates) 
					= IoControlPacketData->CellStateUpdate.State;
				SPDLOG_LOGGER_INFO(LayerLog, "Updated cellstate in {} to {}",
					IoControlPacketData->CellStateUpdate.Coordinates,
					IoControlPacketData->CellStateUpdate.State);
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}
			case ShipSockControl::GAME_READY_SERVER: {

				// server is informing us of all players being ready, can start gaem
				SPDLOG_LOGGER_INFO(LayerLog, "game is ready starting game");
				auto& GameManager = GameManager2::GetInstance();
				auto Result = GameManager.CheckMyPlayerReadyBegin();
				if (Result < 0) {

					SPDLOG_LOGGER_ERROR(LayerLog, "failed to correctly change game state");
					NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR);
				}

				// Starting game rendered derreferd
				auto DispatchState = (ManagementDispatchState*)UserContext;
				DispatchState->GameHasStarted = true;
				SPDLOG_LOGGER_INFO(LayerLog, "Started game completeing request");
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}
			case ShipSockControl::DEAD_SHIP_SERVER: {

				// ForcePlace ship in opponent field and force kill
				auto& GameManager = GameManager2::GetInstance();
				auto OpponentPlayer = GameManager.GetPlayerFieldByOperation(
					GameManager2::GET_OPPONENT_PLAYER,
					INVALID_SOCKET);
				OpponentPlayer->ForcePlaceDeadShip(
					IoControlPacketData->ShipLocation.TypeOfShip,
					IoControlPacketData->ShipLocation.Rotation,
					IoControlPacketData->ShipLocation.ShipsPosition);
				
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}


			case ShipSockControl::RAISE_STATUS_MESSAGE: {

				SPDLOG_LOGGER_INFO(GameLogEx, "Received status message with id {}",
					IoControlPacketData->SpecialRequestIdentifier);
				// Callback injection extension handler:
				// This takes care of async responses from the server 

				// Check if we can find a queued status for our request 
				// (at this point nothing in my brain makes sense anymore)
				auto Iterator = QueuedForStatus.find(IoControlPacketData->SpecialRequestIdentifier);
				if (Iterator == QueuedForStatus.end()) {

					switch (IoControlPacketData->ShipControlRaisedStatus) {
					case ShipSockControl::STATUS_GAME_OVER: {

						// need to find out who won and notify caller of dispatch
						// auto& GameManager = GameManager2::GetInstance();
						auto DispatchState = (ManagementDispatchState*)UserContext;
						DispatchState->GameOverStateRep =
							IoControlPacketData->SocketAsSelectedPlayerId == ServerRemoteIdForMyPlayer
							? ManagementDispatchState::MY_PLAYER_WON : ManagementDispatchState::MY_PALYER_LOST;
						SPDLOG_LOGGER_INFO(LayerLog, "Server dispatched game over message {}",
							DispatchState->GameOverStateRep);
						NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
					}

					case ShipSockControl::STATUS_YOUR_TURN: {

						// Please fucking kill me this is scuffed af
						auto DispatchState = (ManagementDispatchState*)UserContext;
						DispatchState->YourClientHasTurn = true;
						SPDLOG_LOGGER_INFO(LayerLog, "server notified us of having turn");
						NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
					}

					default:
						break;
					}

					// The server send a status for a non queued request ?!
					SPDLOG_LOGGER_CRITICAL(GameLogEx, "Received status message for unrecored record");
					NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR);
				}
				Iterator->second->StatusReceived = true;

				// Check if the server is notifying us with a violation
				if (IoControlPacketData->ShipControlRaisedStatus < 0) {

					SPDLOG_LOGGER_WARN(GameLogEx, "This client violated a gamerule on the server with status {}",
						IoControlPacketData->ShipControlRaisedStatus);
					Iterator->second->StatusAcceptable = false;
					NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
				}

				// The server notified us with a positive status, apply handling
				switch (Iterator->second->TypeTag_Index) {
				case PLACEMENT_ICALLBACK_INDEX: {

					// Our request was confirmed now asynchronously do what the client expected us to actually do
					auto& DownPlacement = dynamic_cast<ShipPlacementEx&>(*Iterator->second);
					auto OurPlayerField = GameManager2::GetInstance()
						.GetPlayerFieldByOperation(GameManager2::GET_MY_PLAYER,
						INVALID_SOCKET);

					// Place ship into field and update grid
					auto Result = OurPlayerField->PlaceShipBypassSecurityChecks(DownPlacement.TypeOfShip,
						DownPlacement.ShipOrientation,
						DownPlacement.ShipCoordinates);
					if (!Result) {

						SPDLOG_LOGGER_CRITICAL(GameLogEx, "GameEx injected controlflow received success notification but failed to place ship");
						NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR);
					}
				}
				break;

				case SRITECELL_ICALLBACK_INDEX: {
					
					// our request was confirmed now asynchronously do what the client expected us to actually do
					auto& DownStrikeLocation = dynamic_cast<FieldStrikeLocationEx&>(*Iterator->second);
					auto OpponentField = GameManager2::GetInstance()
						.GetPlayerFieldByOperation(GameManager2::GET_OPPONENT_PLAYER,
							INVALID_SOCKET);

					// Strike cell and update ship list
					auto Result = OpponentField->StrikeCellAndUpdateShipList(
						DownStrikeLocation.StrikeLocation);
					if (!Result) {

						SPDLOG_LOGGER_CRITICAL(GameLogEx, "GameEx injected controlflow received success notification but failed to update cell");
						NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR);
					}
				}
				break;

				case REMOVESHI_ICALLBACK_INDEX: {
					
					// our request was confirmed now asynchronously do what the client expected us to actually do
					auto& DownRemoveLocation = dynamic_cast<RemoveShipLocationEx&>(*Iterator->second);
					auto PlayerField = GameManager2::GetInstance()
						.GetPlayerFieldByOperation(GameManager2::GET_MY_PLAYER,
							INVALID_SOCKET);

					// Strike cell and update ship list
					auto Result = PlayerField->RemoveShipFromField(
						DownRemoveLocation.OverlapRemoveLoc);
					if (!Result) {

						SPDLOG_LOGGER_CRITICAL(GameLogEx, "GameEx injected controlflow received success notification but failed to update cell");
						NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR);
					}
				}
				break;

				case READYUPPL_ICALLBACK_INDEX: {

					SPDLOG_LOGGER_INFO(GameLogEx, "Received server set ready up state");
				}
				break;

				default: {
					auto ErrorMessage = "received untreatable request state";
					SPDLOG_LOGGER_CRITICAL(GameLogEx, ErrorMessage);
					throw runtime_error(ErrorMessage);
				}}

				SPDLOG_LOGGER_INFO(GameLogEx, "Server notified us of ok state");
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}
			break;

			default: {

				// No handling supplied for this type of SSCTL, fatal
				SPDLOG_LOGGER_CRITICAL(LayerLog, "Received untreated ioctl: {}",
					IoControlPacketData->ControlCode);
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_NOT_HANDLED);
			}}
			break;

		case NwRequestPacket::SOCKET_DISCONNECTED: {

			// Dispatch is requesting to shutdown the connection
			SPDLOG_LOGGER_WARN(LayerLog, "Disconnect notification, dispatch is about to terminate the connecton");
			NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
		}


		default: {
			
			// No handling supplied for this type of SSCTL, fatal
			SPDLOG_LOGGER_CRITICAL(LayerLog, "Received untreated request with ioctl: {}",
				NetworkRequest.IoControlCode);
			NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_NOT_HANDLED);
		}}
	}
}


