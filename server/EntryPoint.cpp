#include "NetworkIoControl.h"

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
		SsLog("Incoming packet requested on socket [%p]\n",
			NetworkRequest.RequestingSocket);
		auto PacketBuffer = std::make_unique<char[]>(PACKET_BUFFER_SIZE);
		auto Result = recv(NetworkRequest.RequestingSocket,
			PacketBuffer.get(),
			PACKET_BUFFER_SIZE,
			0);

		switch (Result) {
		case SOCKET_ERROR:
			SsAssert(1,
				"Requesting socket failed to receive data: %d",
				WSAGetLastError());
			NetworkRequest.IoRequestStatus = NetWorkIoControl::IoRequestPacket::STATUS_REQUEST_ERROR;
			break;

		case 0:
			SsLog("Socket [%p:%p] was closed by client: %d",
				NetworkRequest.RequestingSocket,
				&NetworkRequest,
				WSAGetLastError());
			NetworkRequest.IoRequestStatus = NetWorkIoControl::IoRequestPacket::STATUS_REQUEST_IGNORED;
			break;

		default:
			SsLog("Control handler received ship sock control command,\n"
				"This message may never been show but is implemented for completeness\n");

			NetworkRequest.IoRequestStatus = NetWorkIoControl::IoRequestPacket::STATUS_REQUEST_COMPLETED;
		}
	}
		break;

	case NetWorkIoControl::IoRequestPacket::SOCKET_DISCONNECTED:
		SsLog("Client [%p:%p] is disconnecting\n",
			NetworkRequest.RequestingSocket,
			&NetworkRequest);
		NetworkRequest.IoRequestStatus = NetWorkIoControl::IoRequestPacket::STATUS_REQUEST_COMPLETED;
		break;

	default:
		SsLog("IoRequestPacket unhandled [%p]",
			&NetworkRequest);
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
		NetWorkIoControl ShipSocketObject(PortNumber);
		
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
	catch (const int& ExceptionCode) {
		
		SsAssert(1,
			"critical failure, invalid socket escaped constructor: %d\n",
			ExceptionCode);
		return EXIT_FAILURE;
	}

	SsLog("Shutting down Server");
	return EXIT_SUCCESS;
}