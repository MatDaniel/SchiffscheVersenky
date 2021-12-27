// Entrypoint of client and server, this implements the layer manager,
// responsible for the connection between the game, network manager and the engine,
// as well as implementing the handling for client and server operation modes
#include "BattleShip.h"
#include <memory>
#include <string_view>

// Ugly but do i care ?
using namespace std;

// Networking and game state management (Lima's district)
import Network.Server;
import Network.Client;
using namespace Network;
import GameManagment;
using namespace GameManagment;

// Drawing and Rendering (Daniel's job)
import Draw.Window;
import Draw.Engine;
import Draw.Resources;
import Draw.Scene;
import Draw.DearImGUI;
import Scenes.FieldSetup;

// Layer manager logging object
SpdLogger LayerLog;

void GmManagerUnitTests() {
	// Unit tests
	auto& Manager = GameManager2::CreateObject(
		PointComponent{ 12, 12 },
		ShipCount{ 2, 2, 2, 2, 2 });
	auto MyField = Manager.TryAllocatePlayerWithId(32);
	auto OpponentField = Manager.TryAllocatePlayerWithId(48);

	auto PlaceShipStatus = MyField->PlaceShipSecureCheckInterference(
		ShipClass::CARRIER_5x1,
		ShipRotation::FACING_NORTH,
		PointComponent{ 3, 3 });
	PlaceShipStatus = MyField->PlaceShipSecureCheckInterference(
		ShipClass::CARRIER_5x1,
		ShipRotation::FACING_WEST,
		PointComponent{ 1, 5 });
	PlaceShipStatus = MyField->PlaceShipSecureCheckInterference(
		ShipClass::CARRIER_5x1,
		ShipRotation::FACING_WEST,
		PointComponent{ 8, 8 });

	MyField->StrikeCellAndUpdateShipList(PointComponent{ 4, 6 });
	MyField->StrikeCellAndUpdateShipList(PointComponent{ 3, 7 });
	MyField->StrikeCellAndUpdateShipList(PointComponent{ 2, 8 });
	MyField->StrikeCellAndUpdateShipList(PointComponent{ 2, 4 });
	MyField->StrikeCellAndUpdateShipList(PointComponent{ 8, 8 });
	MyField->StrikeCellAndUpdateShipList(PointComponent{ 1, 5 });

	Debug_PrintGmPlayerField(*MyField, OpponentField);

	MyField->StrikeCellAndUpdateShipList(PointComponent{ 2, 3 });
	MyField->StrikeCellAndUpdateShipList(PointComponent{ 2, 4 });
	MyField->StrikeCellAndUpdateShipList(PointComponent{ 2, 5 });
	MyField->StrikeCellAndUpdateShipList(PointComponent{ 2, 6 });

	Debug_PrintGmPlayerField(*MyField, OpponentField);

	MyField->StrikeCellAndUpdateShipList(PointComponent{ 2, 6 });
	Debug_PrintGmPlayerField(*MyField, OpponentField);

	__debugbreak();
}

#if 0

// Internal server controller and server related utilities/parts of the layer manager
namespace Server {
	using namespace Network::Server;

	void ManagmentDispatchRoutine(
		NetworkManager2& NetworkDevice,
		NwRequestPacket& NetworkRequest,
		void* UserContext
	) {
		TRACE_FUNTION_PROTO;

		switch (NetworkRequest.IoControlCode) {
		case NwRequestPacket::INCOMING_CONNECTION: {

			// Accept incoming connection and checkout
			auto NewClient = NetworkDevice.AcceptIncomingConnection(NetworkRequest);
			if (!NewClient)
				return SPDLOG_LOGGER_ERROR(LayerLog, "Socket failed to accept remote client"); // Fatal dispatch exit
			SPDLOG_LOGGER_INFO(LayerLog, "Successfully connected client to server");

			// we now need to notify the game manager of the connecting player
			auto& GameManager = GameManager2::GetInstance();
			auto NewPlayer = GameManager.TryAllocatePlayerWithId(
				NewClient->GetSocket());
			if (!NewPlayer) {

				// we would need to inform the connecting client of all the current states etc 
				SPDLOG_LOGGER_INFO(LayerLog, "There are already 2 connected players, may only accept viewers now (to be implemented)");

				// TODO: code goes here



				// Skip further handling
				return (void)NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			}

			// Send game dimensions and ship information
			auto Result = NewClient->SendShipControlPackageDynamic(ShipSockControl{
					.ControlCode = ShipSockControl::STARTUP_FIELDSIZE,
					.GameFieldSizes = GameManager.InternalFieldDimensions
				});
			if (Result < 0)
				return NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR),
				SPDLOG_LOGGER_ERROR(LayerLog, "Failed to transmit field size data to client");
			auto Result = NewClient->SendShipControlPackageDynamic(ShipSockControl{
					.ControlCode = ShipSockControl::STARTUP_SHIPCOUNTS,
					.GameShipNumbers = GameManager.InternalShipCount
				});
			if (Result < 0)
				return NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR),
				SPDLOG_LOGGER_ERROR(LayerLog, "Failed to transmit ship number infromation");

			NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			SPDLOG_LOGGER_INFO(LayerLog, "Successfully allocated and associated client to player");
		}
												 break;

		case NwRequestPacket::INCOMING_PACKET: {

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
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR);
				break;

			case 0:
				SPDLOG_LOGGER_WARN(LayerLog, "Socket [{:04x}] was gracefully disconnected",
					NetworkRequest.RequestingSocket);
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED);
				break;

			default:
				// Handle Incoming network request packet
				// this needs to be treated in context of game setup and during playing

				auto ShipSockControlPack = (ShipSockControl*)PacketBuffer.get();
				switch (ShipSockControlPack->ControlCode) {
				case ShipSockControl::NO_COMMAND_CLIENT:
					SPDLOG_LOGGER_WARN(LayerLog, "Debug NO_COMMAND received");
					break;

				case ShipSockControl::SHOOT_CELL_CLIENT: {
					auto GameManager = GameManager2::GetInstance();

					// Check if we are in game phase
					if (GameManager.GetCurrentGamePhase() != GamePhase::GAME_PHASE) {

						// Currently not in the corerct gamephase, notify caller
						auto Client = NetworkDevice.GetClientBySocket(RequestingClientId);
						if (!Client)
							return NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR),
							SPDLOG_LOGGER_CRITICAL(LayerLog, "Could NOT find Client for registered socket [{:04x}]",
								RequestingClientId); // Abort server

						// Post status report
						if (Client->RaiseStatusMessage(ShipControlStatus::STATUS_NOT_YOUR_TURN) < 0)
							return NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR),
							SPDLOG_LOGGER_ERROR(LayerLog, "Failed to post status message code on client [{:04x}]",
								RequestingClientId);

						// Complete request and exit dispatch
						return NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED),
							SPDLOG_LOGGER_WARN(LayerLog, "Requesting player [{:04x}], did not have his turn for current round",
								RequestingClientId);


					}



					auto RequestingClientId = NetworkRequest.RequestingSocket;

					// Check if the requesting player is
					auto RequestingPlayerField = GameManager.GetPlayerFieldByoperation(
						GameManager2::GET_PLAYER_BY_ID,
						RequestingClientId);
					if (!RequestingPlayerField)

						// Get players by id (socket handle)
						auto PlayerOfCurrentTurn = GameManager.GetPlayerFieldByOperation(
							GameManager2::PLAYER_BY_TURN,
							INVALID_SOCKET);
					auto


						auto RemotePlayers = GameManager.GetFieldControllerPairById(
							RequestingClientId);

					if (!RequestingPlayer)
						return NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED),
						SPDLOG_LOGGER_WARN(LayerLog, "Shoot request not by player");
					SPDLOG_LOGGER_INFO(LayerLog, "Found players for SHOOT request");

					// Check if requesting player has its turn now
					if (RequestingPlayer != GameManager.GetFieldOfCurrentPlayersTurn()) {
						auto Client = NetworkDevice.GetClientBySocket(RequestingClientId);
						if (!Client)
							return NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR),
							SPDLOG_LOGGER_CRITICAL(LayerLog, "Could NOT find Client for registered socket [{:04x}]",
								RequestingClientId); // Abort server

						// Post status report
						if (Client->RaiseStatusMessage(ShipControlStatus::STATUS_NOT_YOUR_TURN) < 0)
							return NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR),
							SPDLOG_LOGGER_ERROR(LayerLog, "Failed to post status message code on client [{:04x}]",
								RequestingClientId);

						// Complete request and exit dispatch
						return NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED),
							SPDLOG_LOGGER_WARN(LayerLog, "Requesting player [{:04x}], did not have his turn for current round",
								RequestingClientId);
					}

					// Update internal server state
					auto NewCellState = RemotePlayer->StrikeCellAndUpdateShipList(
						ShipSockControlPack->ShootCellLocation);
					SPDLOG_LOGGER_INFO(LayerLog, "New Cellstate for {{{}:{}}} is {}",
						ShipSockControlPack->ShootCellLocation.XComponent,
						ShipSockControlPack->ShootCellLocation.YComponent,
						NewCellState);

					// Send update notification to all connected clients
					ShipSockControl UpdateNotification{
						.SizeOfThisStruct = sizeof(UpdateNotification),
						.ControlCode = ShipSockControl::CELL_STATE_SERVER,
						.CellStateUpdate = {
							.Coordinates = ShipSockControlPack->ShootCellLocation,
							.State = NewCellState
						}
					};
					auto ControllerList = NetworkDevice.GetClientList();
					for (auto& Client : ControllerList)
						if (auto Result = Client.SendShipControlPackageDynamic(UpdateNotification) < 0)
							SPDLOG_LOGGER_WARN(LayerLog, "failed to send client [{:04x}] cellstate update command",
								Client.GetSocket());
					SPDLOG_LOGGER_INFO(LayerLog, "Updated all clients");
					NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
				}
													   break;

				case ShipSockControl::SET_SHIP_LOC_CLIENT:
				{
					// Meta setup handle data
					auto& GameManager = GameManager2::GetInstance();
					auto RequestingClientId = NetworkRequest.RequestingSocket;

					// Check if type of request is currently accepted in gamestate
					if (GameManager.GetCurrentGamePhase() != GamePhase::SETUP_PHASE)
						return NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED),
						SPDLOG_LOGGER_WARN(LayerLog, "Cannot place ships in non setup phases of the game");

					// Get Player for requesting client or exit
					auto RequestingPlayer = GameManager.GetPlayerFieldControllerById(RequestingClientId);
					if (!RequestingPlayer)
						return NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_IGNORED),
						SPDLOG_LOGGER_WARN(LayerLog, "requesting client [{:04x}] is not a player, unable to allocate ship",
							RequestingClientId);
					SPDLOG_LOGGER_INFO(LayerLog, "Found player {} for ship placement request request",
						(void*)RequestingPlayer);

					// Place ship into grid
					auto Result = RequestingPlayer->PlaceShipSecureCheckInterference(
						ShipSockControlPack->SetShipLocation.ShipType,
						ShipSockControlPack->SetShipLocation.Rotation,
						ShipSockControlPack->SetShipLocation.ShipsPosition);
					if (Result < 0) {

						// the ship presumably collided with another ship or the current slot is unavailable,
						// client has to be notified of the invalid placement
						auto Client = NetworkDevice.GetClientBySocket(RequestingClientId);
						if (!Client)
							return NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR),
							SPDLOG_LOGGER_CRITICAL(LayerLog, "Critical failure, no client associated to requesting socket, impossible state");

						// Notify player of invalid placement
						if (Client->RaiseStatusMessage(ShipControlStatus::STATUS_INVALID_PLACEMENT) < 0)
							return NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR),
							SPDLOG_LOGGER_ERROR(LayerLog, "Failed to post status message code on client [{:04x}]",
								RequestingClientId);
						return (void)NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
					}

					// Complete request
					SPDLOG_LOGGER_INFO(LayerLog, "Successfully received and placed ship on board");
					NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
				}
				break;




				default:
					SPDLOG_LOGGER_WARN(LayerLog, "Untreated ShipSockControl command encountered, this is fine for now");
				}
			}
		}
											 break;

		case NwRequestPacket::SOCKET_DISCONNECTED:
			NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			SPDLOG_LOGGER_WARN(LayerLog, "Socket [{:04x}] was gracefully disconnected",
				NetworkRequest.OptionalClient->GetSocket());
			break;

		default:
			SPDLOG_LOGGER_CRITICAL(LayerLog, "Io request left untreated, fatal");
		}
	}
}

// Internal client controller and client related utilities/parts of the layer manager
namespace Client {
	using namespace Network::Client;

	void ManagementDispatchRoutine(
		NetworkManager2& NetworkDevice,
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
				return (void)NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);

			case ShipSockControl::CELL_STATE_SERVER: {
				auto& GameManager = GameManager2::GetInstance();



			}

			default:
				break;
			}







			NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_COMPLETED);
			break;
		}
	}
}
#endif



int main(
	int   argc,
	const char* argv[]
) {
	// Creating Loggers, we need multiple for each components (Game, Network, Layer, Renderer and etc.)
	// Add loggers here as needed
	try {
		LayerLog = spdlog::stdout_color_st("Layer");
		GameLog = spdlog::stdout_color_st("Game");
		NetworkLog = spdlog::stdout_color_st("Network");
		EngineLog = spdlog::stdout_color_st("Engine");
		WindowLog = spdlog::stdout_color_st("Window");
		DearImGUILog = spdlog::stdout_color_st("ImGUI");
		
		spdlog::set_pattern(SPDLOG_SMALL_PATTERN);
		spdlog::set_level(spdlog::level::debug);
		SPDLOG_LOGGER_INFO(LayerLog, "Initialized Spdlog loggers");
	}
	catch (const spdlog::spdlog_ex& ExceptionInformation) {

		SPDLOG_WARN("Failed to initilize Spdlog with: \"{}\"",
			ExceptionInformation.what());
		return EXIT_FAILURE;
	}

	// GmManagerUnitTests();


	// Parse input arguments (TODO: actually doing it)
	enum CurrentStandaloneMode {
		INVALID_MODE = 0,
		APPLICATION_IS_SERVER,
		APPLICATION_IS_CLIENT
	} CurrentRequestStartupType = APPLICATION_IS_CLIENT;

#define IF_MINIMUM_PASSED_ARGUMENTS(Minimum) if (argc > Minimum)
#define DEREF_ARGUMENT_AT_INDEX(Index) argv[Index+1]
	IF_MINIMUM_PASSED_ARGUMENTS(1) {

		// Check if the passed argument indicates that we are running as server
		CurrentRequestStartupType = !string_view(DEREF_ARGUMENT_AT_INDEX(0)).compare("s") ?
			APPLICATION_IS_SERVER : CurrentRequestStartupType;
	}

	long Result = 0;



	// Primary capture clause
	try {
		switch (CurrentRequestStartupType) {
		case APPLICATION_IS_SERVER: {

			// Parse server specific command line arguments
			auto PortNumber = DefaultPortNumber;
			IF_MINIMUM_PASSED_ARGUMENTS(2)
				PortNumber = DEREF_ARGUMENT_AT_INDEX(1);


#if 0
			// Creating and initializing managers managers
			auto& ShipSocketObject = *Network::Server::NetworkManager::CreateSingletonOverride(PortNumber);
			auto& ShipGameObject = GameManager2::CreateSingletonOverride({ 6, 6 },
				{ 2,2,2,2,2 });

			// Run main server handler loop
			SPDLOG_LOGGER_INFO(LayerLog, "Entering server management mode, ready for IO");
			for (;;) {
				Result = ShipSocketObject.PollNetworkConnectionsAndDispatchCallbacks(
					::Server::ManagmentDispatchRoutine,
					nullptr);
				if (Result < 0)
					return SPDLOG_LOGGER_ERROR(LayerLog, "An error with id: {} ocured on in the callback dispatch routine",
						Result), EXIT_FAILURE;
			}
#endif
			SPDLOG_LOGGER_INFO(LayerLog, "Server successfully terminated, shutting down and good night :D");
		}
								  break;

		case APPLICATION_IS_CLIENT: {

			// Parse server specific command line arguments
			auto ServerAddress = DefaultServerAddress;
			auto PortNumber = DefaultPortNumber;
			IF_MINIMUM_PASSED_ARGUMENTS(1)
				PortNumber = DEREF_ARGUMENT_AT_INDEX(0);
			IF_MINIMUM_PASSED_ARGUMENTS(2)
				ServerAddress = DEREF_ARGUMENT_AT_INDEX(1);

#if 0
			// pretty much all game related code should go here
			auto& ServerObject = *Network::Client::NetworkManager::CreateSingletonOverride(
				ServerAddress,
				PortNumber);

			for (;;) {

				auto ResponseOption = ServerObject.ExecuteNetworkRequestHandlerWithCallback(
					::Client::ManagementDispatchRoutine,
					nullptr);

				if (ResponseOption < 0)
					break;

				Sleep(1000);
			}
#endif
			// Unit test code for client, we will merge this together later (aka the network io into the engine)
			{
				// Initialize the window
				if (!Window::Init())
					return EXIT_FAILURE;

				// Initialize the engine
				if (!Engine::Init())
				{
					Window::CleanUp(); // Clean up the window on failure
					return EXIT_FAILURE;
				}

				// Initialize the gui
				DearImGUI::Init();

				// Initialize the resources
				Resources::Init();

				// Set entry scene
				Scene::Load(std::make_unique<FieldSetupScene>());

				// Run
				int code = Engine::Run();

				// Clean up
				Scene::CleanUp();
				Resources::CleanUp();
				DearImGUI::CleanUp();
				Window::CleanUp();


				// Exit
				SPDLOG_LOGGER_INFO(LayerLog, "Client successfully terminated");
				return code;
			}
		}
			break;

		default:
			SPDLOG_LOGGER_ERROR(LayerLog, "Could not determin if software was started as client or server, fatal?");
			return EXIT_FAILURE;
		}

		// Post Server/Client, possible general non scope based cleanup...

	}

	// Last primary level scope error filter
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
	catch (...) {

		SPDLOG_LOGGER_ERROR(LayerLog, "An unknown exception was thrown, unable to handle");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
