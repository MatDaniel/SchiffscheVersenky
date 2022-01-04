// DispatchRoutine defines the 
module;

#include "BattleShip.h"
#include <functional>

export module DispatchRoutine;
using namespace std;
import Network.Server;
import Network.Client;
using namespace Network;
import GameManagment;
using namespace GameManagment;

// Layer manager logging object
export SpdLogger LayerLog;

namespace Server {
	using namespace Network::Server;

	void CheckCurrentPhaseAndRaiseClientStatus( // Checks if the request matches the game phase supplied
												// on mismatch the function will raise a status on the client
												// and complete the NRP returning to the manager immediately
		NetworkManager2*        NetworkDevice,  
		NwRequestPacket&        NetworkRequest, // The NRP to check against
		GameManager2::GamePhase WantedPhase     // The game phase that should be checked for
	) {
		TRACE_FUNTION_PROTO;

		auto& GameManager = GameManager2::GetInstance();
		if (GameManager.GetCurrentGamePhase() == WantedPhase)
			return;

		// Currently not in the correct game phase, notify caller by posting status and complete NRP
		auto AssociatedClient = NetworkDevice->GetClientBySocket(
			NetworkRequest.RequestingSocket);
		AssociatedClient->RaiseStatusMessageOrComplete(NetworkRequest,
			ShipSockControl::STATUS_NOT_IN_PHASE);
		SPDLOG_LOGGER_WARN(LayerLog, "Cannot execute handler, not in the correct gamestate");
		NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED);
	}

	GmPlayerField& CheckPlayerAndGetFieldOrRaiseClientStatus( // Check if the nrp is dispatched through a registered player
		                                                      // and get player's field, or complete nrp on failure
		NetworkManager2* NetworkDevice,                       // A network manager pointer to the instance holding the player
		NwRequestPacket& NetworkRequest                       // The NRP to complete on failure
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
			ShipSockControl::STATUS_NOT_A_PLAYER);
		SPDLOG_LOGGER_WARN(LayerLog, "Requesting client {}, is not a player", 
			NetworkRequest.RequestingSocket);
		NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED);
	}

	void CheckPlayerHasTurnOrRaiseStatus( // Checks if the requesting player has his turn
		NetworkManager2* NetworkDevice,   // A network manager pointer to the instance holding the player
		NwRequestPacket& NetworkRequest   // The NRP to complete on failure and get the requesting player from
	) {
		TRACE_FUNTION_PROTO;

		auto& GameManager = GameManager2::GetInstance();
		if (NetworkRequest.RequestingSocket == GameManager.GetCurrentSelectedPlayer())
			return;

		auto AssociatedClient = NetworkDevice->GetClientBySocket(
			NetworkRequest.RequestingSocket);
		AssociatedClient->RaiseStatusMessageOrComplete(NetworkRequest,
			ShipSockControl::STATUS_NOT_YOUR_TURN);
		SPDLOG_LOGGER_WARN(LayerLog, "Requesting client {}, does not have his turn", 
			NetworkRequest.RequestingSocket);
		NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED);
	}
}

// Internal server controller and server related utilities/parts of the layer manager
export namespace Server {
	using namespace Network::Server;

	void ManagmentDispatchRoutine(
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
						GameManager2::GAME_PHASE);
					CheckPlayerHasTurnOrRaiseStatus(NetworkDevice,
						NetworkRequest);
					
					// Get requesting client and filter out non players, does not return on error
					auto& RequestingPlayerField = CheckPlayerAndGetFieldOrRaiseClientStatus(NetworkDevice,
						NetworkRequest);
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
						GameManager2::SETUP_PHASE);
					auto& RequestingPlayer = CheckPlayerAndGetFieldOrRaiseClientStatus(NetworkDevice,
						NetworkRequest);
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
						auto AssociatedClient = NetworkDevice->GetClientBySocket(
							NetworkRequest.RequestingSocket);
						AssociatedClient->RaiseStatusMessageOrComplete(NetworkRequest,
							ShipSockControl::STATUS_INVALID_PLACEMENT);
						NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED);
					}

					// Complete request
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

// Internal client controller and client related utilities/parts of the layer manager
export namespace Client {
	using namespace Network::Client;

	SOCKET ServerRemoteIdForMyPlayer = INVALID_SOCKET;
	struct ManagmentDispatchState {
		PointComponent InternalFieldDimensions;
		ShipCount      NumberOFShipsPerType;
		bool           StateReady;
		SOCKET         ClientIdByServer;
	};



	void ManagementDispatchRoutine(
		NetworkManager2* NetworkDevice,
		NwRequestPacket& NetworkRequest,
		void* UserContext
	) {
		TRACE_FUNTION_PROTO;

		switch (NetworkRequest.IoControlCode) {
		case NwRequestPacket::INCOMING_PACKET:

			switch (NetworkRequest.IoControlPacketData->ControlCode) {
			case ShipSockControl::NO_COMMAND_SERVER:
				SPDLOG_LOGGER_INFO(LayerLog, "Debug command received, invalid handling");
				__debugbreak();
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
				
			case ShipSockControl::STARTUP_FIELDSIZE: {

				// Get Engine passed init state and set coords
				auto DispatchState = (ManagmentDispatchState*)UserContext;
				DispatchState->InternalFieldDimensions = NetworkRequest.IoControlPacketData->GameFieldSizes;
				SPDLOG_LOGGER_INFO(LayerLog, "Recorded and stored server defined playfield size");
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);				
			}
			case ShipSockControl::STARTUP_SHIPCOUNTS: {

				// Get Engine passed init state and set shipnumbers
				auto DispatchState = (ManagmentDispatchState*)UserContext;
				DispatchState->NumberOFShipsPerType = NetworkRequest.IoControlPacketData->GameShipNumbers;
				DispatchState->StateReady = true; // hacky, will refine later
				SPDLOG_LOGGER_INFO(LayerLog, "Recorded and stored server defined shipcounts");
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}
			case ShipSockControl::STARTUP_YOUR_ID: {

				// At this point the game manager should already be valid
				ServerRemoteIdForMyPlayer = NetworkRequest.IoControlPacketData->SocketAsSelectedPlayerId;
				auto DispatchState = (ManagmentDispatchState*)UserContext;
					DispatchState->ClientIdByServer = ServerRemoteIdForMyPlayer;
				SPDLOG_LOGGER_INFO(LayerLog, "Server associated us with {}",
					ServerRemoteIdForMyPlayer);
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}





			case ShipSockControl::CELL_STATE_SERVER: {
				auto& GameManager = GameManager2::GetInstance();



			}

			default:
				SPDLOG_LOGGER_CRITICAL(LayerLog, "Received untreated ioctl: {}",
					NetworkRequest.IoControlPacketData->ControlCode);

				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_NOT_HANDLED);
			}
			break;

		default:
			SPDLOG_LOGGER_CRITICAL(LayerLog, "Received untreated request with ioctl: {}",
				NetworkRequest.IoControlCode);

			NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_NOT_HANDLED);
		}
	}
}


