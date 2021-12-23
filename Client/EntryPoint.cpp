// Implements the start of the client

// Network
import NetworkControl;
import ShipSock;

#define NOMINMAX
#include <SharedLegacy.h>

// Drawing
import Draw.Window;
import Draw.Engine;
import Draw.Resources;
import Draw.Scene;
import Draw.DearImGUI;
import Scenes.FieldSetup;

#include <cstdlib>
#include <memory>

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

int main(int argc, const char* argv[])
{
	auto ServerAddress = DefaultServerAddress;
	auto PortNumber = DefaultPortNumber;
	if (argc >= 2)
		ServerAddress = argv[1];
	if (argc >= 3)
		PortNumber = argv[2];

	// WaitForDebugger();

	try {
		GameLog = spdlog::stdout_color_st("Game");
		// spdlog::set_pattern(SPDLOG_SMALL_PATTERN);
		spdlog::set_level(spdlog::level::debug);
		TRACE_FUNTION_PROTO;

	}
	catch (const spdlog::spdlog_ex& ExceptionInformation) {
		__debugbreak();
	}




	SsLog("Creating networkmanager\n");
	long Result = 0;
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
	catch (const int& ExceptionCode) {

		SsAssert(1,
			"critical failure, invalid socket escaped constructor: %d\n",
			ExceptionCode);
		return EXIT_FAILURE;
	}
	catch (const std::exception& Exception) {
		SsAssert(1,
			Exception.what());
		return EXIT_FAILURE;
	}

	SsLog("exiting handler loop");
	

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