// Entrypoint of client and server, this implements the layer manager,
// responsible for the connection between the game, network manager and the engine,
// as well as implementing the handling for client and server operation modes
// module;

#include "BattleShip.h"
#include <memory>
#include <string_view>

// export module LayerBase;

// Ugly but do i care ?
using namespace std;

// Networking and game state management (Lima's district)
import DispatchRoutine;
import Network.Server;
import Network.Client;
using namespace Network;
import GameManagement;
using namespace GameManagement;


// Drawing and Rendering (Daniel's job)
import Draw.Window;
import Draw.Engine;
import Draw.Resources;
import Draw.Scene;
import Draw.DearImGUI;
import Draw.NetEngine;
import Scenes.Menu;
using namespace Draw;

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
		PointComponent{ 7, 5 });
	PlaceShipStatus = MyField->PlaceShipSecureCheckInterference(
		ShipClass::SUBMARINE_3x1,
		ShipRotation::FACING_WEST,
		PointComponent{ 7, 8 });


	Debug_PrintGmPlayerField(*MyField);
	MyField->RemoveShipFromField(PointComponent{ 3, 4 });
	Debug_PrintGmPlayerField(*MyField);


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

export int main(
	int   argc,
	const char* argv[]
) {
	// Creating Loggers, we need multiple for each components (Game, Network, Layer, Renderer and etc.)
	// Add loggers here as needed
	try {
		LayerLog = spdlog::stdout_color_st("Layer");
		GameLog = spdlog::stdout_color_st("Game");
		GameLogEx = spdlog::stdout_color_st("GameEx");
		NetworkLog = spdlog::stdout_color_st("Network");
		EngineLog = spdlog::stdout_color_st("Engine");
		WindowLog = spdlog::stdout_color_st("Window");
		DearImGUILog = spdlog::stdout_color_st("ImGUI");
		ResourceLog = spdlog::stdout_color_st("Resource");
		
		spdlog::set_pattern(SPDLOG_SMALL_PATTERN);
#ifdef NDEBUG
		spdlog::set_level(spdlog::level::info);
#else
		spdlog::set_level(spdlog::level::debug);
#endif
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


			// Creating and initializing managers managers
			auto& ShipSocketObject = Network::Server::NetworkManager2::CreateObject(PortNumber);
			auto& ShipGameObject = GameManager2::CreateObject(
				PointComponent{ 8, 8 },
				ShipCount{ 2,1,1,1,1 });

			// Run main server handler loop
			SPDLOG_LOGGER_INFO(LayerLog, "Entering server management mode, ready for IO");
			for (;;) {
				auto Result = ShipSocketObject.PollNetworkConnectionsAndDispatchCallbacks(
					::Server::ManagementDispatchRoutine,
					nullptr);
				if (Result < 0)
					return SPDLOG_LOGGER_ERROR(LayerLog, "An error with id: {} ocured on in the callback dispatch routine",
						Result), EXIT_FAILURE;
			}

			GameManager2::ManualReset();
			SPDLOG_LOGGER_INFO(LayerLog, "Server successfully terminated, shutting down and good night :D");
		}
		break;

		case APPLICATION_IS_CLIENT: {

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
				Scene::Load(make_unique<Scenes::MenuScene>());

				// Run
				int code = Engine::Run();

				// Clean up
				Scene::CleanUp();
				Resources::CleanUp();
				DearImGUI::CleanUp();
				Window::CleanUp();
				NetEngine::CleanUp();

				// Exit
				SPDLOG_LOGGER_INFO(LayerLog, "Client successfully terminated");
				GameManager2::ManualReset();
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

		SPDLOG_ERROR("Spdlog threw an exception: \"{}\"",
			ExceptionInformation.what());
		return EXIT_FAILURE;
	}
	catch (const std::exception& ExceptionInformation) {

		SPDLOG_LOGGER_CRITICAL(LayerLog, "StandardException thrown: {}",
			ExceptionInformation.what());
		return EXIT_FAILURE;
	}
	catch (...) {

		SPDLOG_LOGGER_CRITICAL(LayerLog, "An unknown exception was thrown, unable to handle");
		__debugbreak();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
