// This implements the ClientContoller that manages the clients and provides an IO bridge
#include "NetworkIoControl.h"

NetWorkIoControl::NetWorkIoControl( // Creates 
	const char* PortNumber
) {
	SsLog("Preparing internal server structures for async IO operation\n"
		"Retriving portnumber information for localhost\n");
	addrinfo* ServerInformation,
	          AddressHints{};
	AddressHints.ai_flags = AI_PASSIVE;
	AddressHints.ai_family = AF_INET;
	AddressHints.ai_socktype = SOCK_STREAM;
	AddressHints.ai_protocol = IPPROTO_TCP;
	auto Result = getaddrinfo(
		NULL,
		PortNumber,
		&AddressHints,
		&ServerInformation);
	if (SsAssert(Result,
		"failes to retieve portinfo on \"%s\" with: %d\n",
		PortNumber,
		Result))
		return;

	SsLog("Creating gamesever and binding ports\n");
	auto ServerInformationIterator = ServerInformation;
	do {
		LocalServerSocket = socket(
			ServerInformationIterator->ai_family,
			ServerInformationIterator->ai_socktype,
			ServerInformationIterator->ai_protocol);
	} while (LocalServerSocket == INVALID_SOCKET &&
		(ServerInformationIterator = ServerInformationIterator->ai_next));
	if (SsAssert(LocalServerSocket == INVALID_SOCKET,
		"failed to create socked with: %d\n",
		WSAGetLastError()))
		goto Cleanup;

	Result = bind(
		LocalServerSocket,
		ServerInformationIterator->ai_addr,
		ServerInformationIterator->ai_addrlen);
	freeaddrinfo(ServerInformation);
	ServerInformation = nullptr;
	if (SsAssert(Result,
		"failed to bind socket to port \"%s\", with: %d\n",
		PortNumber,
		WSAGetLastError()))
		goto Cleanup;

	SsLog("Starting to listen to incoming requests, passively\n");
	Result = listen(
		LocalServerSocket,
		SOMAXCONN);
	if (SsAssert(Result,
		"failed to initilize listening queue with: %d\n",
		WSAGetLastError()))
		goto Cleanup;
	return;

Cleanup:
	if (ServerInformation)
		freeaddrinfo(ServerInformation),
		ServerInformation = nullptr;
	if (LocalServerSocket)
		SsAssert(closesocket(LocalServerSocket),
			"failed to close server socket, critical?: %d\n",
			WSAGetLastError());
	LocalServerSocket = INVALID_SOCKET;
}
NetWorkIoControl::~NetWorkIoControl() {
	SsLog("Cleaning up internal server structures\n");
	

}

SOCKET NetWorkIoControl::GetServerSocketHandle() {
	return LocalServerSocket;
}


void NetWorkIoControl::LaunchClientManagerConvertThread(           // Starts internally handling accept requests passively, accepted requests get added to a 
	ConnectionHandler AcceptConnectionHandler,
	void*             UserContext
) {
	SsLog("Starting to accept arbitrary requests and setting up worker threads for possible clients");

	do {
		ClientController PossiblyClient;
		PossiblyClient.ClientInfoSize = (long)sizeof(PossiblyClient.ClientInformation);
		PossiblyClient.AssociatedClient = accept(
			PossiblyClient.AssociatedClient,
			&PossiblyClient.ClientInformation,
			&PossiblyClient.ClientInfoSize);
		if (SsAssert(PossiblyClient.AssociatedClient == INVALID_SOCKET,
			"failed to accept request with: %d\n",
			WSAGetLastError()))
			return;








	} while (1);

}
