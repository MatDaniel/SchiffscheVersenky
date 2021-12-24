// Entrypoint of the game server, this implements the connection between the game manager and the network manager
import NetworkIoControl;
import GameManagment;

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <SharedLegacy.h>
#include <memory>

extern spdlogger GameLog;
extern spdlogger ServerLog;
spdlogger LayerLog;
using namespace std;



struct IoDispatchUserContext {
	GameManagmentController* GameManagerQuickRef;
};
void ServerManagmentDispatchRoutine(
	NetWorkIoControl& NetworkDevice,
	IoRequestPacket&  NetworkRequest,
	void*             UserContext
) {
	TRACE_FUNTION_PROTO;

	switch (NetworkRequest.IoControlCode) {
	case IoRequestPacket::INCOMING_CONNECTION:
	{
		SPDLOG_LOGGER_INFO(LayerLog, "Incoming connection requested, adding client");
		auto NewClient = NetworkDevice.AcceptIncomingConnection(NetworkRequest);
		if (!NewClient)			
			return SPDLOG_LOGGER_ERROR(LayerLog, "Socket failed to accept remote client"); // Fatal dispatch exit
		SPDLOG_LOGGER_INFO(LayerLog, "Successfully connected client to server");

		// we now need to notify the game manager of the connecting player
		auto GameManager = GameManagmentController::GetInstance();
		auto NewPlayer = GameManager->AllocattePlayerWithId(NewClient->GetSocket());
		if (!NewPlayer) {
			
			// we would need to inform the connecting client of all the current states etc 
			SPDLOG_LOGGER_INFO(LayerLog, "There are already 2 connected players, may only accept viewers now (to be implemented)");
			
			// TODO: code goes here



			// Skip further handling
			return (void)NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_COMPLETED);
		}
		
		// Send game dimensions information
		auto Result = NewClient->SendShipControlPackageDynamic(ShipSockControl{
				.ControlCode = ShipSockControl::STARTUP_FIELDSIZE,
				.GameFieldSizes = GameManager->GetFieldDimensions()
			});
		if (Result < 0)
			return NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_ERROR),
				SPDLOG_LOGGER_ERROR(LayerLog, "Failed to transmit field size data to client");
		NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_COMPLETED);
		SPDLOG_LOGGER_INFO(LayerLog, "Successfully allocated and associated client to player");
	}
		break;

	case IoRequestPacket::INCOMING_PACKET:
	{
		auto PacketBuffer = make_unique<char[]>(PACKET_BUFFER_SIZE);
		auto Result = recv(NetworkRequest.RequestingSocket,
			PacketBuffer.get(),
			PACKET_BUFFER_SIZE,
			0);
		SPDLOG_LOGGER_INFO(LayerLog, "Incoming packet request, read input: {}", Result);
		
		switch (Result) {
		case SOCKET_ERROR:
			SPDLOG_LOGGER_ERROR(LayerLog, "fatal on receive on requesting socket [{:04x}], {}",
				NetworkRequest.RequestingSocket, WSAGetLastError());
			NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_ERROR);
			break;

		case 0:
			SPDLOG_LOGGER_WARN(LayerLog, "Socket [{:04x}] was gracefully disconnected",
				NetworkRequest.RequestingSocket);
			NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_IGNORED);
			break;

		default:
			auto IoPacket = (ShipSockControl*)PacketBuffer.get();
			switch (IoPacket->ControlCode) {
			case ShipSockControl::NO_COMMAND_CLIENT:
				SPDLOG_LOGGER_WARN(LayerLog, "Debug NO_COMMAND received");
				break;

			case ShipSockControl::SHOOT_CELL_CLIENT:
			{
				auto GameManager = GameManagmentController::GetInstance();
				auto RequestingClientId = NetworkRequest.RequestingSocket;

				// Get players by id (socket handle)
				PlayerField* RemotePlayer;
				auto RequestingPlayer = GameManager->GetPlayerFieldControllerById(RequestingClientId,
					&RemotePlayer);
				if (!RequestingPlayer)
					return NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_IGNORED),
						SPDLOG_LOGGER_WARN(LayerLog, "Shoot request not by player");
				SPDLOG_LOGGER_INFO(LayerLog, "Found players for SHOOT request");

				// Check if requesting player has its turn now
				if (RequestingPlayer != GameManager->GetPlayerByTurn()) {
					auto Client = NetworkDevice.GetClientBySocket(RequestingClientId);
					if (!Client)
						return NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_ERROR),
						SPDLOG_LOGGER_CRITICAL(LayerLog, "Could NOT find Client for registered socket [{:04x}]",
							RequestingClientId); // Abort server

					// Post status report
					if (Client->RaiseStatusMessage(ShipControlStatus::STATUS_NOT_YOUR_TURN) < 0)
						return NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_ERROR),
						SPDLOG_LOGGER_ERROR(LayerLog, "Failed to post status message code on client [{:04x}]",
							RequestingClientId);

					// Complete request and exit dispatch
					return NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_COMPLETED),
						SPDLOG_LOGGER_WARN(LayerLog, "Requesting player [{:04x}], did not have his turn for current round",
							RequestingClientId);
				}

				// Update internal server state
				auto NewCellState = RemotePlayer->StrikeCellAndUpdateShipList(
					IoPacket->ShootCellLocation);
				SPDLOG_LOGGER_INFO(LayerLog, "New Cellstate for {{{}:{}}} is {}",
					IoPacket->ShootCellLocation.XCord,
					IoPacket->ShootCellLocation.YCord,
					NewCellState);

				// Send update notification to all connected clients
				ShipSockControl UpdateNotification{
					.SizeOfThisStruct = sizeof(UpdateNotification),
					.ControlCode = ShipSockControl::CELL_STATE_SERVER,
					.CellStateUpdate = {
						.Coordinates = IoPacket->ShootCellLocation,
						.State = NewCellState
					}
				};
				auto ControllerList = NetworkDevice.GetClientList();
				for (auto& Client : ControllerList)
					if (auto Result = Client.SendShipControlPackageDynamic(UpdateNotification) < 0)
						SPDLOG_LOGGER_WARN(LayerLog, "failed to send client [{:04x}] cellstate update command",
							Client.GetSocket());
				SPDLOG_LOGGER_INFO(LayerLog, "Updated all clients");
				NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_COMPLETED);
			}
				break;

			case ShipSockControl::SET_SHIP_LOC_CLIENT:
			{
				// Meta setup handle data
				auto GameManager = GameManagmentController::GetInstance();
				auto RequestingClientId = NetworkRequest.RequestingSocket;
				
				// Check if type of request is currently accepted in gamestate
				if (GameManager->GetCurrentGamePhase() != GamePhase::SETUP_PHASE)
					return NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_IGNORED),
						SPDLOG_LOGGER_WARN(LayerLog, "Cannot place ships in non setup phases of the game");

				// Get Player for requesting client or exit
				auto RequestingPlayer = GameManager->GetPlayerFieldControllerById(RequestingClientId);
				if (!RequestingPlayer)
					return NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_IGNORED),
						SPDLOG_LOGGER_WARN(LayerLog, "requesting client [{:04x}] is not a player, unable to allocate ship",
							RequestingClientId);
				SPDLOG_LOGGER_INFO(LayerLog, "Found player {} for ship placement request request",
					(void*)RequestingPlayer);

				// Place ship into grid
				auto Result = RequestingPlayer->PlaceShipSecureCheckInterference(
					IoPacket->SetShipLocation.ShipType,
					IoPacket->SetShipLocation.Rotation,
					IoPacket->SetShipLocation.ShipsPosition);
				if (Result < 0) {
					
					// the ship presumably collided with another ship or the current slot is unavailable,
					// client has to be notified of the invalid placement
					auto Client = NetworkDevice.GetClientBySocket(RequestingClientId);
					if (!Client)
						return NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_ERROR),
							SPDLOG_LOGGER_CRITICAL(LayerLog, "Critical failure, no client associated to requesting socket, impossible state");

					// Notify player of invalid placement
					if (Client->RaiseStatusMessage(ShipControlStatus::STATUS_INVALID_PLACEMENT) < 0)
						return NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_ERROR),
						SPDLOG_LOGGER_ERROR(LayerLog, "Failed to post status message code on client [{:04x}]",
							RequestingClientId);
					return (void)NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_COMPLETED);
				}

				// Complete request
				SPDLOG_LOGGER_INFO(LayerLog, "Successfully received and placed ship on board");
				NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_COMPLETED);
			}
				break;




			default:
				SPDLOG_LOGGER_WARN(LayerLog, "Untreated ShipSockControl command encountered, this is fine for now");
			}
		}
	}
		break;

	case IoRequestPacket::SOCKET_DISCONNECTED:
		NetworkRequest.CompleteIoRequest(IoRequestPacket::STATUS_REQUEST_COMPLETED);
		SPDLOG_LOGGER_WARN(LayerLog, "Socket [{:04x}] was gracefully disconnected", 
			NetworkRequest.RequestingSocket);
		break;

	default:
		SPDLOG_LOGGER_CRITICAL(LayerLog, "Io request left untreated, fatal");
	}
}

int main(
	      int   argc,
	const char* argv[]
) {
	// Create Server on passed port number or use default
	const char* PortNumber = DefaultPortNumber;
	if (argc == 2)
		PortNumber = argv[1];
	long Result = 0;
	
	// Creating Loggers, we need 3 for each component (Game, Server, Layer)
	try {
		LayerLog = spdlog::stdout_color_st("Layer");
		GameLog = spdlog::stdout_color_st("Game");
		ServerLog = spdlog::stdout_color_st("Socket");
		spdlog::set_pattern(SPDLOG_SMALL_PATTERN);
		spdlog::set_level(spdlog::level::debug);
		TRACE_FUNTION_PROTO;
	}
	catch (const spdlog::spdlog_ex& ExceptionInformation) {

		SPDLOG_WARN("Failed to initilize Spdlog with: \"{}\"",
			ExceptionInformation.what());
		return EXIT_FAILURE;
	}



	// Creating network manager and starting server
	try {
		// Creating and initializing managers managers
		auto& ShipSocketObject = *NetWorkIoControl::CreateSingletonOverride(PortNumber);
		auto& ShipGameObject = *GameManagmentController::CreateSingletonOverride({ 6, 6 },
			{ 2,2,2,2,2 });
		
		// Run main server handler loop
		SPDLOG_LOGGER_INFO(LayerLog, "Entering server management mode, ready for IO");
		for (;;) {
			Result = ShipSocketObject.PollNetworkConnectionsAndDispatchCallbacks(
				ServerManagmentDispatchRoutine,
				nullptr);
			if (Result < 0)
				return SPDLOG_LOGGER_ERROR(LayerLog, "An error with id:{} ocured on in the callback dispatch routine",
					Result), EXIT_FAILURE;
		}
	}
	catch (const spdlog::spdlog_ex& ExceptionInformation) {

		SPDLOG_WARN("Spdlog threw an exception: \"{}\"",
			ExceptionInformation.what());
		return EXIT_FAILURE;
	}
	catch (const std::exception& ExceptionInformation) {

		SPDLOG_LOGGER_ERROR(LayerLog, "StandardException of: {}", 
			ExceptionInformation.what());
		return EXIT_FAILURE;
	}
	catch (const int& ExceptionCode) {
		
		SPDLOG_LOGGER_ERROR(LayerLog, "A critical Exception code was thrown, {}", ExceptionCode);
		return EXIT_FAILURE;
	}
	catch (...) {

		SPDLOG_LOGGER_ERROR(LayerLog, "An unknown exception was thrown, unable to handle");
		return EXIT_FAILURE;
	}

	SPDLOG_LOGGER_INFO(LayerLog, "Server successfully terminated, shutting down and good night :D");
	return EXIT_SUCCESS;
}