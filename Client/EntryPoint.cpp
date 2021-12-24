// EntryPoint of the game client, Implements the start and layer manager betweem all client components
import ShipSock;
import NetworkControl;
import GameManagment;

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <SharedLegacy.h>
#include <memory>

extern spdlogger GameLog;
extern spdlogger NetworkLog;
spdlogger LayerLog;
using namespace std;

// Drawing
import Draw.Window;
import Draw.Engine;
import Draw.Resources;
import Draw.Scene;
import Draw.DearImGUI;
import Scenes.FieldSetup;

#include <cstdlib>



void NetworkDispatchTest(
	ServerIoController* NetworkDevice,
	ServerIoController::IoRequestPacketIndirect* NetworkRequest,
	void* UserContext
) {
	switch (NetworkRequest->IoControlCode) {
	case ServerIoController::IoRequestPacketIndirect::INCOMING_PACKET:

		SsLog("Pseudohandling request here\n");


		NetworkRequest->IoRequestStatus = ServerIoController::IoRequestPacketIndirect::STATUS_REQUEST_COMPLETED;
		break;

	}
}

int main(
	      int   argc,
	const char* argv[]
) {
	auto ServerAddress = DefaultServerAddress;
	auto PortNumber = DefaultPortNumber;
	if (argc >= 2)
		ServerAddress = argv[1];
	if (argc >= 3)
		PortNumber = argv[2];

	// WaitForDebugger();

	// Creating Loggers, we need multiple for each components (Game, Network, Layer, Renderer)
	try {
		LayerLog = spdlog::stdout_color_st("Layer");
		GameLog = spdlog::stdout_color_st("Game");
		NetworkLog = spdlog::stdout_color_st("Network");
		spdlog::set_pattern(SPDLOG_SMALL_PATTERN);
		spdlog::set_level(spdlog::level::debug);
	
		TRACE_FUNTION_PROTO;
		SPDLOG_LOGGER_INFO(LayerLog, "Initilized Spdlog loggers");
	}
	catch (const spdlog::spdlog_ex& ExceptionInformation) {

		SPDLOG_WARN("Failed to initilize Spdlog with: \"{}\"",
			ExceptionInformation.what());
		return EXIT_FAILURE;
	}



	// Create and start basic server (unit test)
	try {
		auto& Server = *ServerIoController::CreateSingletonOverride(
			ServerAddress,
			PortNumber);

		for (;;) {

			auto ResponseOption = Server.ExecuteNetworkRequestHandlerWithCallback(
				NetworkDispatchTest,
				nullptr
			);

			if (ResponseOption < 0)
				break;

			Sleep(1000);
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

	SPDLOG_LOGGER_INFO(LayerLog, "Exiting CLient io unit test");



	// Initialize the window
	if (!Window::init())
		return EXIT_FAILURE;

	// Initialize the engine
	if (!Engine::init())
	{
		Window::cleanUp(); // Clean up the window on failure
		return EXIT_FAILURE;
	}

	// Initialize the gui
	DearImGUI::init();

	// Initialize the resources
	Resources::init();

	// Set entry scene
	Scene::load(std::make_unique<FieldSetupScene>());

	// Run
	int code = Engine::run();

	// Clean up
	Scene::cleanUp();
	Resources::cleanUp();
	DearImGUI::cleanUp();
	Window::cleanUp();

	// Exit
	return code;
	
}