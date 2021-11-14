// This file implements the Network Io for the client, aka @Lima's zone
#include "NetworkQueue.h"

ServerIoController::ServerIoController(
	const char* ServerName,
	const char* PortNumber,
	long* OutResponse
) {
	SsLog("Preparing internal server structures for async IO operation\n"
		"Retrieving remote server information\n");
	addrinfo* ServerInformation,
		AddressHints{};
	AddressHints.ai_family = AF_INET;
	AddressHints.ai_socktype = SOCK_STREAM;
	AddressHints.ai_protocol = IPPROTO_TCP;
	auto Result = getaddrinfo(
		NULL,
		PortNumber,
		&AddressHints,
		&ServerInformation);
	*OutResponse = -1;
	if (SsAssert(Result,
		"failes to retieve portinfo on \"%s\" with: %d\n",
		PortNumber,
		Result))
		return;

	SsLog("Preparing gameserver socket\n");
	auto ServerInformationIterator = ServerInformation;
	do {
		GameServer = socket(
			ServerInformationIterator->ai_family,
			ServerInformationIterator->ai_socktype,
			ServerInformationIterator->ai_protocol);
	} while (GameServer == INVALID_SOCKET &&
		(ServerInformationIterator = ServerInformationIterator->ai_next));
	*OutResponse = -2;
	if (SsAssert(GameServer == INVALID_SOCKET,
		"failed to create socked with: %d\n",
		WSAGetLastError()))
		goto Cleanup;

	Result = connect(
		GameServer,
		ServerInformation->ai_addr,
		ServerInformation->ai_addrlen);
	freeaddrinfo(ServerInformation);
	ServerInformation = nullptr;
	*OutResponse = -3;
	if (SsAssert(Result,
		"failed to connect to server with: %d\n",
		WSAGetLastError()))
		goto Cleanup;

	*OutResponse = 0;
	return;

Cleanup:
	if (ServerInformation)
		freeaddrinfo(ServerInformation);
	if (GameServer != INVALID_SOCKET)
		SsAssert(closesocket(GameServer),
			"failed to close server socket, critical?: %d\n",
			WSAGetLastError());
	GameServer = INVALID_SOCKET;
}

ServerIoController::~ServerIoController() {
	SsLog("Closing server connection and cleaning up socket");
}

