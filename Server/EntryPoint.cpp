// Entrypoint of the game server, this implements the connection between thethe game manager and the network manager
import NetworkIoControl;
import GameManagment;

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
	switch (NetworkRequest.IoControlCode) {
	case IoRequestPacket::INCOMING_CONNECTION:
	{
		LayerLog->info("Incoming connection requested, adding client");
		auto NewClient = NetworkDevice.AcceptIncomingConnection(NetworkRequest);
		if (!NewClient) {

			LayerLog->error("Socket failed to accept remote client");
			return; // Fatal dispatch exit
		}

		if (NetworkDevice.GetNumberOfConnectedClients() >= 2) {
			
			// we would need to inform the connecting client of all the current states etc 
			LayerLog->info("There are already 2 connected players, may only accept viewers now (to be implemented)");



			NetworkRequest.IoRequestStatus = IoRequestPacket::STATUS_REQUEST_COMPLETED;
			break; // Skip further handling
		}

		// we now need to notify the game manager of the connecting player

		auto GameManager = GameManagmentController::GetInstance();


	}
		break;

	case IoRequestPacket::INCOMING_PACKET:
	{
		auto PacketBuffer = make_unique<char[]>(PACKET_BUFFER_SIZE);
		auto Result = recv(NetworkRequest.RequestingSocket,
			PacketBuffer.get(),
			PACKET_BUFFER_SIZE,
			0);
		LayerLog->info("Incoming packet request, read input: {}", Result);
		
		switch (Result) {
		case SOCKET_ERROR:
			NetworkRequest.IoRequestStatus = IoRequestPacket::STATUS_REQUEST_ERROR;
			LayerLog->error("fatal on receive on requesting socket[{:04x}], {}",
				NetworkRequest.RequestingSocket, WSAGetLastError());
			break;

		case 0:
			NetworkRequest.IoRequestStatus = IoRequestPacket::STATUS_REQUEST_IGNORED;
			LayerLog->warn("Socket [{:04x}] was gracefully disconnected",
				NetworkRequest.RequestingSocket);
			break;

		default:
			// Request has to be handled here

			auto IoPacket = (ShipSockControl*)PacketBuffer.get();

			switch (IoPacket->ControlCode)
			{
			case ShipSockControl::NO_COMMAND:
				LayerLog->warn("Debug NO_COMMAND received");


			default:
				break;
			}



			NetworkRequest.IoRequestStatus = IoRequestPacket::STATUS_REQUEST_COMPLETED;
		}
	}
		break;

	case IoRequestPacket::SOCKET_DISCONNECTED:
		NetworkRequest.IoRequestStatus = IoRequestPacket::STATUS_REQUEST_COMPLETED;
		LayerLog->warn("Socket [{:04x}] was gracefully disconnected", 
			NetworkRequest.RequestingSocket);
		break;

	default:
		LayerLog->warn("Io request left untreated, fatal");
	}
}

int main(
	int         argc,
	const char* argv[]
) {
	// Create Server on passed port number or use default
	const char* PortNumber = DefaultPortNumber;
	if (argc == 2)
		PortNumber = argv[1];
	long Result = 0;
	
	try {
		// Creating Loggers, we need 3 for each component (Game, Server, Layer)
		GameLog = spdlog::stdout_color_st("Game");
		ServerLog = spdlog::stdout_color_st("Socket");
		LayerLog = spdlog::stdout_color_st("Layer"); 
	


		// Creating and initilizing network managers
		auto& ShipSocketObject = *NetWorkIoControl::CreateSingletonOverride(PortNumber);
		ShipCount Ships{ 2,2,2,2,2 };
		auto& ShipGameObject = *GameManagmentController::CreateSingletonOverride(6, 6, Ships);
		
		LayerLog->info("Entering server management mode, ready for IO");
		for (;;) {
			Result = ShipSocketObject.PollNetworkConnectionsAndDispatchCallbacks(
				ServerManagmentDispatchRoutine,
				nullptr);
			if (Result < 0)
				return LayerLog->error("An error with id:{} ocured on in the callback dispatch routine",
					Result), EXIT_FAILURE;
		}
	}
	catch (const spdlog::spdlog_ex& Exception) {

		SsAssert(1,
			"spdlog failed with: \"%s\"",
			Exception.what());
		return EXIT_FAILURE;
	}
	catch (const int& ExceptionCode) {
		
		LayerLog->error("A critical Exception code was thrown, {}", ExceptionCode);
		return EXIT_FAILURE;
	}
	catch (...) {

		LayerLog->error("An unknown exception was thrown, unable to handle");
		return EXIT_FAILURE;
	}

	LayerLog->info("Server successfully terminated, shutting down and good night :D");
	return EXIT_SUCCESS;
}