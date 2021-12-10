// Entrypoint of the game server, this implements the connection between thethe game manager and the network manager
import NetworkIoControl;
import GameManagment;

#include <SharedLegacy.h>
#include <memory>

extern spdlogger GameLog;
extern spdlogger ServerLog;
spdlogger LayerLog;



void NetworkDispatchTest(
	NetWorkIoControl&                  NetworkDevice,
	NetWorkIoControl::IoRequestPacket& NetworkRequest,
	void*                              UserContext
) {
	switch (NetworkRequest.IoControlCode) {
	case NetWorkIoControl::IoRequestPacket::INCOMING_CONNECTION:
		NetworkDevice.AcceptIncomingConnection(NetworkRequest);
		break;

	case NetWorkIoControl::IoRequestPacket::INCOMING_PACKET:
	{
		auto PacketBuffer = std::make_unique<char[]>(PACKET_BUFFER_SIZE);
		auto Result = recv(NetworkRequest.RequestingSocket,
			PacketBuffer.get(),
			PACKET_BUFFER_SIZE,
			0);
		
		switch (Result) {
		case SOCKET_ERROR:
			NetworkRequest.IoRequestStatus = NetWorkIoControl::IoRequestPacket::STATUS_REQUEST_ERROR;
			ServerLog->error("fatal on receive on requesting socket[{:04x}], {}",
				NetworkRequest.RequestingSocket, WSAGetLastError());
			break;

		case 0:
			NetworkRequest.IoRequestStatus = NetWorkIoControl::IoRequestPacket::STATUS_REQUEST_IGNORED;
			ServerLog->warn("Socket [{:04x}] was gracefully disconnected", NetworkRequest.RequestingSocket);
			break;

		default:
			// Request has to be handled here



			NetworkRequest.IoRequestStatus = NetWorkIoControl::IoRequestPacket::STATUS_REQUEST_COMPLETED;
		}
	}
		break;

	case NetWorkIoControl::IoRequestPacket::SOCKET_DISCONNECTED:
		NetworkRequest.IoRequestStatus = NetWorkIoControl::IoRequestPacket::STATUS_REQUEST_COMPLETED;
		ServerLog->warn("Socket [{:04x}] was gracefully disconnected", NetworkRequest.RequestingSocket);
		break;

	default:
		ServerLog->warn("Io request left untreated, fatal");
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
	


		// Creating network manager
		auto& ShipSocketObject = *NetWorkIoControl::CreateSingletonOverride(PortNumber);
		
		SsLog("Starting to accept arbitrary requests and setting up worker threads for possible clients");
		for (;;) {
			Result = ShipSocketObject.PollNetworkConnectionsAndDispatchCallbacks(
				NetworkDispatchTest,
				nullptr);
			if (SsAssert(Result < 0,
				"NetworkManager failed to execute properly -> shuting down server : %d\n",
				Result))
				return EXIT_FAILURE;
		}
	}
	catch (const spdlog::spdlog_ex& Exception) {

		SsAssert(1,
			"spdlog failed with: \"%s\"",
			Exception.what());
		return EXIT_FAILURE;
	}
	catch (const int& ExceptionCode) {
		
		SsAssert(1,
			"critical failure, invalid socket escaped constructor: %d\n",
			ExceptionCode);
		return EXIT_FAILURE;
	}

	SsLog("Shutting down Server");
	return EXIT_SUCCESS;
}