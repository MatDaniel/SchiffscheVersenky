// This file implements the Network Io for the client, aka @Lima's zone
#include "NetworkQueue.h"

ServerIoController::ServerIoController(
	const char* ServerName,
	const char* PortNumber,
	long*       OutResponse
) {
	SsLog("Preparing internal server structures for async IO operation\n"
		"Retrieving remote server information\n");
	addrinfo AddressHints{
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = IPPROTO_TCP
	};
	auto Result = getaddrinfo(
		NULL,
		PortNumber,
		&AddressHints,
		&ServerInformation);
	*OutResponse = -1;
	if (SsAssert(Result,
		"failes to retieve portinfo on \"%s\" with: %d\n",
		PortNumber,
		Result)) return;

	SsLog("Preparing gameserver socket\n");
	auto ServerInformationIterator = ServerInformation;
	u_long BlockingMode = 1;
	do {
		GameServer = socket(
			ServerInformationIterator->ai_family,
			ServerInformationIterator->ai_socktype,
			ServerInformationIterator->ai_protocol);
	} while (GameServer == INVALID_SOCKET &&
		(ServerInformationIterator = ServerInformationIterator->ai_next));
	if (SsAssert(GameServer == INVALID_SOCKET,
		"failed to create socked with: %d\n",
		WSAGetLastError())) {

		*OutResponse = -2;
		freeaddrinfo(ServerInformation);
		ServerInformation = nullptr;
	}

	SsLog("Switching to nonblocking mode on socket: [%p]",
		GameServer);
	Result = ioctlsocket(
		GameServer,
		FIONBIO,
		&BlockingMode);
	if (SsAssert(Result,
		"failed to switch socket into non blocking mode: %d",
		WSAGetLastError())) {

		*OutResponse = -3;
		freeaddrinfo(ServerInformation);
		ServerInformation = nullptr;
		SsAssert(closesocket(GameServer),
			"failed to close server socket, critical?: %d\n",
			WSAGetLastError());
		GameServer = INVALID_SOCKET;
	}

	*OutResponse = QueueConnectAttemptTimepoint();
}

long ServerIoController::QueueConnectAttemptTimepoint() {
	
	SsLog("Queuing socket connect rquest in async mode");
	auto Result = connect(
		GameServer,
		ServerInformation->ai_addr,
		ServerInformation->ai_addrlen);
	if (Result == SOCKET_ERROR) {
		auto Response = WSAGetLastError();
		if (SsAssert(Response != WSAEWOULDBLOCK,
			"failed to schedule conneciton request, cause unknown controller invlaid: %d",
			Response))
			return -4;
	}

	return 0;
}
void ServerIoController::ConnectSuccessSwitchToMode() {

	SsLog("Server connected on socket, switching controller IoMode");
	freeaddrinfo(ServerInformation);
	ServerInformation = nullptr;
	SocketAttached = true;
}
long ServerIoController::QueryServerIsConnected() {
	
	SsLog("Querying connecting socket request");
	fd_set WriteSet; FD_ZERO(&WriteSet);
	FD_SET(GameServer, &WriteSet);
	fd_set ExceptionSet; FD_ZERO(&ExceptionSet);
	FD_SET(GameServer, &ExceptionSet);
	timeval Timeout{};
	auto Result = select(0, nullptr,
		&WriteSet,
		&ExceptionSet,
		&Timeout);
	switch (Result) {
	case SOCKET_ERROR:
		SsLog("socket query failed, cause: %d",
			WSAGetLastError());
		return SOCKET_ERROR;

	case 0:
		SsLog("Connect has not finished work yet");
		return STATUS_WORK_PENDING;

	case 1:
		if (WriteSet.fd_count)
			return SsLog("Client successfully connected to gameserver"),
				ConnectSuccessSwitchToMode(),
				STATUS_SUCESSFUL;
		if (ExceptionSet.fd_count)
			return SsLog("Client failed to connect to socket"),
				STATUS_FAILED_TO_CONNECT;
	}
}

ServerIoController::~ServerIoController() {
	SsLog("Closing server connection and cleaning up socket");
}

long ServerIoController::ExecuteNetworkRequestHandlerWithCallback(
	MajorFunction IoCompleteRequestQueue,
	void*         UserContext
) {
	if (!SocketAttached) {

		// Mega Brainfart retarded design
		auto Result = QueryServerIsConnected();
		switch (Result) {
		case STATUS_WORK_PENDING:
			return 0;
		case STATUS_SUCESSFUL:
			return 1;
		default:
			return -1;
		}
	}

	SsLog("Preparing for incoming messages or return\n");
	auto ProcessHeap = GetProcessHeap();
	auto PacketBuffer = (char*)HeapAlloc(ProcessHeap,
		0,
		PACKET_BUFFER_SIZE);
	if (SsAssert(!PacketBuffer,
		"Heapmanager failed to cretae packet buffer, critical: %d",
		GetLastError()))
		return -1;

	// check if socket has input data to recieve
	long Result = 0;
	
	do {
		Result = recv(GameServer,
			PacketBuffer,
			PACKET_BUFFER_SIZE,
			0);

		if (Result > 0) {

			ShipSockControl* ShipPacket = (ShipSockControl*)PacketBuffer;
			SsLog("Recived Gamenotification, IOCTL: &d",
				ShipPacket->ControlCode);
			IoRequestPacketIndirect RequestDispatchPacket{
				.IoControlCode = IoRequestPacketIndirect::INCOMING_PACKET,
				.ControlIoPacketData = ShipPacket,
				.IoRequestStatus = IoRequestPacketIndirect::STATUS_REQUEST_NOT_HANDLED
			};
			IoCompleteRequestQueue(this,
				&RequestDispatchPacket,
				UserContext);
			if (SsAssert(RequestDispatchPacket.IoRequestStatus < 0,
				"Callback failed to handle request")) {

				Result = -2; break;
			}
		} else {

			if (Result == SSOCKET_DISCONNECTED) {
	
				SsLog("Server disconnected from socket,\n"
					"this shouldnt normally happen but its gracefully so it has to be treated specially\n");

				IoRequestPacketIndirect RequestDispatchPacket{
					.IoControlCode = IoRequestPacketIndirect::SOCKET_DISCONNECTED,
					.IoRequestStatus = IoRequestPacketIndirect::STATUS_REQUEST_NOT_HANDLED
				};
				IoCompleteRequestQueue(this,
					&RequestDispatchPacket,
					UserContext);
				if (SsAssert(RequestDispatchPacket.IoRequestStatus != IoRequestPacketIndirect::STATUS_REQUEST_COMPLETED,
					"Callback refused or did not handle server disconnect properly,\n"
					"tHE FUck are you doing @Daniel")) {

					Result = -3; break;
				}

				Result = 2;
			} else 
				switch (WSAGetLastError()) {

				case WSAEWOULDBLOCK:
					SsLog("No (more) request to handle, we can happily exit now :)\n");
					Result = 0;
					break;

				default:
					SsLog("Ok shit went south here, uh dont know what to do here ngl,\n"
						"time to die ig: %d",
						WSAGetLastError());
					Result = -4;
					break;
				}
		}
	} while (Result > 0);

Cleanup:
	HeapFree(ProcessHeap,
		0,
		PacketBuffer);
	return Result;
}

