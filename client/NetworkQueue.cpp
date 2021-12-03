// This file implements the Network Io for the client, aka @Lima's zone
#include "NetworkQueue.h"





ServerIoController* ServerIoController::CreateSingletonOverride(
	const char* ServerName,
	const char* ServerPort
) {
	SsLog("ServerIoCtl Factory called, creating NetworkingObject\n");

	// Magic fuckery cause make_unique cannot normally access a private constructor
	struct EnableMakeUnique : public ServerIoController {
		inline EnableMakeUnique(
			const char* ServerName,
			const char* ServerPort
		) : ServerIoController(
			ServerName,
			ServerPort) {}
	};
	return (InstanceObject = std::make_unique<EnableMakeUnique>(
		ServerName,
		ServerPort)).get();
}
ServerIoController* ServerIoController::GetInstance() {
	return InstanceObject.get();
}

ServerIoController::ServerIoController(
	const char* ServerName,
	const char* PortNumber
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
	if (SsAssert(Result,
		"failes to retieve portinfo on \"%s\" with: %d\n",
		PortNumber,
		Result))
		throw - 1;

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

		freeaddrinfo(ServerInformation);
		throw - 2;
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

		freeaddrinfo(ServerInformation);
		SsAssert(closesocket(GameServer),
			"failed to close server socket, critical?: %d\n",
			WSAGetLastError());
		throw - 3;
	}

	SsLog("Queuing socket connect request in async mode");
	Result = connect(
		GameServer,
		ServerInformation->ai_addr,
		ServerInformation->ai_addrlen);
	if (Result == SOCKET_ERROR) {
		auto Response = WSAGetLastError();
		if (SsAssert(Response != WSAEWOULDBLOCK,
			"failed to schedule conneciton request, cause unknown controller invlaid: %d",
			Response))
			throw - 4;
	}
}

ServerIoController::~ServerIoController() {
	SsLog("Closing server connection and cleaning up socket");
}

long ServerIoController::ExecuteNetworkRequestHandlerWithCallback(
	MajorFunction IoCompleteRequestQueue,
	void* UserContext
) {
	// Check if Multiplexer has attached to server 
	if (!SocketAttached) {

		SsLog("Querying connecting socket request");
		fd_set WriteSet;
		FD_ZERO(&WriteSet);
		FD_SET(GameServer, &WriteSet);
		fd_set ExceptionSet;
		FD_ZERO(&ExceptionSet);
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
			if (WriteSet.fd_count) {

				SsLog("Server connected on socket, switching controller IoMode");
				freeaddrinfo(ServerInformation);
				SocketAttached = true;
				break;
			}
			if (ExceptionSet.fd_count)
				return SsLog("Client failed to connect to socket"),
				STATUS_FAILED_TO_CONNECT;
		}
	}

	SsLog("Checking for input and handling messages\n");
	long Result = 0;
	auto PacketBuffer = std::make_unique<char[]>(PACKET_BUFFER_SIZE);
	do {
		Result = recv(GameServer,
			PacketBuffer.get(),
			PACKET_BUFFER_SIZE,
			0);

		switch (Result) {
		case SOCKET_ERROR:
			switch (auto SocketErrorCode = WSAGetLastError()) {
			case WSAEWOULDBLOCK:
				SsLog("No (more) request to handle, we can happily exit now :)\n");
				Result = 0;
				break;

			case WSAECONNRESET:
				SsLog("Connection reset, server aborted, closing socket");
				SsAssert(closesocket(GameServer),
					"Server socket failed to close properly: %d",
					WSAGetLastError());
				return IoRequestPacketIndirect::STATUS_REQUEST_ERROR;

			default:
				SsLog("Ok shit went south here, uh dont know what to do here ngl,\n"
					"time to die ig: %d",
					SocketErrorCode);
				Result = IoRequestPacketIndirect::STATUS_REQUEST_ERROR;
			}
			break;

		case 0: {
			SsLog("Server disconnected from client by remote\n");

			IoRequestPacketIndirect RequestDispatchPacket{
				.IoControlCode = IoRequestPacketIndirect::SOCKET_DISCONNECTED,
				.IoRequestStatus = IoRequestPacketIndirect::STATUS_REQUEST_NOT_HANDLED
			};
			IoCompleteRequestQueue(this,
				&RequestDispatchPacket,
				UserContext);
			if (SsAssert(RequestDispatchPacket.IoRequestStatus < 0,
				"Callback refused or did not handle server disconnect properly,\n"
				"tHE FUck are you doing @Daniel")) {

				Result = RequestDispatchPacket.IoRequestStatus;
			}
		}
			break;

		default: {
			auto ShipPacket = (ShipSockControl*)PacketBuffer.get();
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

				Result = RequestDispatchPacket.IoRequestStatus;
			}
		}
		}
	} while (Result > 0);

	return Result;
}

