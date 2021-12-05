// Implements the start of the client
import NetworkControl;

import Draw.Engine;
import Draw.Scene;
import Scenes.FieldSetup;

#include <ShipSock.h>
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

int main(int argc, const char* argv[])
{

	/*
	auto ServerAddress = DefaultServerAddress;
	auto PortNumber = DefaultPortNumber;
	if (argc >= 2)
		ServerAddress = argv[1];
	if (argc >= 3)
		PortNumber = argv[2];

	// WaitForDebugger();

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
	*/


	Scene::load(Scene::getter<FieldSetupScene>());
	return Engine::run();
	
}