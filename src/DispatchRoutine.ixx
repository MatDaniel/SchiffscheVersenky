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
		void*            UserContext,
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
					auto OpponenntId = GameManager.GetSocketAsIdForPlayerField(*OpponentPlayerField);
					auto OpponentClient = NetworkDevice->GetClientBySocket(OpponenntId);
					OpponentClient->DirectSendPackageOutboundOrComplete(NetworkRequest,
						ShipSockControl{
							.ControlCode = ShipSockControl::SHOOT_CELL_SERVER,
							.SocketAsSelectedPlayerId = OpponenntId,
							.ShootCellLocation = RequestPackage->ShootCellLocation
						});
					SPDLOG_LOGGER_INFO(LayerLog, "Send opponent {} state change notification",
						OpponenntId);

					// - broadcast the changes made to except to the opponents client
					NetworkDevice->BroadcastShipSockControlExcept(NetworkRequest,
						OpponenntId,
						ShipSockControl{
							.ControlCode = ShipSockControl::CELL_STATE_SERVER,
							.SocketAsSelectedPlayerId = OpponenntId,
							.CellStateUpdate = { RequestPackage->ShootCellLocation,
							CellUpdated } });

					SPDLOG_LOGGER_INFO(LayerLog, "Broadcasted cell state change to other clients");
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
					auto AssociatedClient = NetworkDevice->GetClientBySocket(
						NetworkRequest.RequestingSocket);
					if (Result != GmPlayerField::STATUS_SHIP_PLACED) {

						SPDLOG_LOGGER_ERROR(LayerLog, "Could not place ship, client {} seemingly didnt check correctly or attempted to cheat",
							NetworkRequest.RequestingSocket);
						AssociatedClient->RaiseStatusMessageOrComplete(NetworkRequest,
							ShipSockControl::STATUS_INVALID_PLACEMENT,
							RequestPackage->SpecialRequestIdentifier);
						NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED);
					}

					// Complete request, notify player of successful placement
					AssociatedClient->RaiseStatusMessageOrComplete(NetworkRequest,
						ShipSockControl::STATUS_NEUTRAL,
						RequestPackage->SpecialRequestIdentifier);
					SPDLOG_LOGGER_INFO(LayerLog, "Successfully received and placed ship on board");
					NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
				}

				default:
					SPDLOG_LOGGER_WARN(LayerLog, "Untreated ShipSockControl command encountered, this is fine for now");
					return;
				}
			
			
			
			
			
			
			
				
				
				
				
				
				
				
			// ALlocate buffer for incoming package and recv()
			auto PacketBuffer = make_unique<char[]>(PACKET_BUFFER_SIZE);
			auto Result = recv(NetworkRequest.RequestingSocket,
				PacketBuffer.get(),
				PACKET_BUFFER_SIZE,
				0);
			SPDLOG_LOGGER_INFO(LayerLog, "Incoming packet request, read input: {}", Result);

			// Evaluate recv() result and dispatch
			switch (Result) {
			case SOCKET_ERROR:
				SPDLOG_LOGGER_ERROR(LayerLog, "fatal on receive on requesting socket {}, {}",
					NetworkRequest.RequestingSocket, WSAGetLastError());
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR);
								
			default:
				// Handle Incoming network request packet
				// this needs to be treated in context of game setup and during playing

				/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				auto ShipSockControlPack = (ShipSockControl*)PacketBuffer.get();
			}
		}
		break;

		case NwRequestPacket::SOCKET_DISCONNECTED:
			// Further handling is required can be ignored for now

			NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			SPDLOG_LOGGER_WARN(LayerLog, "Socket {} was gracefully disconnected",
				NetworkRequest.OptionalClient->GetSocket());
			break;

		default:
			SPDLOG_LOGGER_CRITICAL(LayerLog, "Io request left untreated, fatal");
		}
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
		auto RequestingPlayersId = GameManager.GetSocketAsIdForPlayerField(*OptionalField);
		auto OurPlayer = GameManager.GetPlayerFieldByOperation(GameManager2::GET_MY_PLAYER,
			INVALID_SOCKET);

		// Check if current player is our own
		if (OurPlayer != OptionalField) {
			// if opponent player: 
			// Ship placement will only be updated by the server, 
			// there has to be no async request therefore the exeution goes directly through
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

	}

	struct ManagementDispatchState {
		PointComponent InternalFieldDimensions;
		ShipCount      NumberOFShipsPerType;
		bool           StateReady;
		SOCKET         ClientIdByServer;
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

			case ShipSockControl::CELL_STATE_SERVER: {
				auto& GameManager = GameManager2::GetInstance();


				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}


			// Callback injection extension handler:
			// This takes care of async responses from the server 
			case ShipSockControl::RAISE_STATUS_MESSAGE: {

				SPDLOG_LOGGER_INFO(GameLogEx, "Received status message with id {}",
					IoControlPacketData->SpecialRequestIdentifier);

				// Check if we can find a queued status for our request 
				// (at this point nothing in my brain makes sense anymore)
				auto Iterator = QueuedForStatus.find(IoControlPacketData->SpecialRequestIdentifier);
				if (Iterator == QueuedForStatus.end()) {

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

					// our request was confirmed now asynchronously do what the client expected us to actually do
					auto& DownPlacement = dynamic_cast<ShipPlacementEx&>(*Iterator->second);
					auto OurPlayerField = GameManager2::GetInstance()
						.GetPlayerFieldByOperation(GameManager2::GET_MY_PLAYER,
						INVALID_SOCKET);
					auto Result = OurPlayerField->PlaceShipBypassSecurityChecks(DownPlacement.TypeOfShip,
						DownPlacement.ShipOrientation,
						DownPlacement.ShipCoordinates);
					if (!Result) {

						SPDLOG_LOGGER_CRITICAL(GameLogEx, "GameEx injected controlflow received success notification but failed to place ship");
						NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR);
					}

					SPDLOG_LOGGER_INFO(GameLogEx, "Server notified us of ok state");
					NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
				}

				case SRITECELL_ICALLBACK_INDEX: {

					NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_NOT_HANDLED);
				}

				case REMOVESHI_ICALLBACK_INDEX: {

					NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_NOT_HANDLED);
				}}

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


