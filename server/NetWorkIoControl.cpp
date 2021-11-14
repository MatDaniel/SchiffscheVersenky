// This implements the ClientContoller that manages the clients and provides an IO bridge
#include "NetworkIoControl.h"

NetWorkIoControl::NetWorkIoControl( // Creates 
	const char* PortNumber,
	long*       OutResponse
) {
	SsLog("Preparing internal server structures for async IO operation\n"
		"Retriving portnumber information for localhost\n");
	*OutResponse = 0;
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
	*OutResponse = -1;
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

NetWorkIoControl::IoRequestPacket::IoRequestStatusType NetWorkIoControl::AcceptIncomingConnection(
	IoRequestPacket* NetworkRequest
) {
	SsLog("Passing through incoming connection\n");
	NetWorkIoControl::ClientController AcceptingClient;
	AcceptingClient.ClientInfoSize = sizeof(AcceptingClient.ClientInformation);
	AcceptingClient.AssociatedClient = accept(
		NetworkRequest->RequestingSocket,
		&AcceptingClient.ClientInformation,
		&AcceptingClient.ClientInfoSize);
	if (SsAssert(AcceptingClient.AssociatedClient == INVALID_SOCKET,
		"failed to open connection, wtf happened i have no idea: %d",
		WSAGetLastError()))
		return IoRequestPacket::IoRequestStatusType::STATUS_REQUEST_ERROR;
	ConnectedClients.emplace_back(AcceptingClient);
	return IoRequestPacket::IoRequestStatusType::STATUS_REQUEST_COMPLETED;
}



long NetWorkIoControl::LaunchServerIoManagerAndRegisterServiceCallback(
	MajorFunction IoCompleteRequestRoutine,
	void*         UserContext
) {
	SsLog("Starting to accept arbitrary requests and setting up worker threads for possible clients");

	// We dont have to do anything until we get the first client, this will accept the first client and then start the main server loop until we hit an error
	ClientController PossiblyClient;
	PossiblyClient.ClientInfoSize = (long)sizeof(PossiblyClient.ClientInformation);
	
	SsLog("Setting up socket descriptor tables and waiting for first client");
	auto ProcessHeap = GetProcessHeap();
	auto SocketArray = (WSAPOLLFD*)HeapAlloc(
		ProcessHeap,
		0,
		sizeof(WSAPOLLFD));
	if (SsAssert(!SocketArray,
		"failed to allocate Socket descriptor table, critical with: %d",
		GetLastError()))
		return -1;
	size_t CurrentFdArraySize = 1;
	SocketArray[0].fd = LocalServerSocket;
	SocketArray[0].events = POLLIN;

	for (;;) {
		size_t NumberOfSockets = ConnectedClients.size() + 1; // includes the server socket as that has to be polled too
		switch ((NumberOfSockets <=> CurrentFdArraySize)._Value) {
		case -1:
			// Regenerate the FD-Array as client was removed
			if (auto NewSocketArray = (WSAPOLLFD*)HeapReAlloc(
				ProcessHeap,
				NULL,
				SocketArray,
				NumberOfSockets * sizeof(WSAPOLLFD))) {

				for (int i = 0; i < NumberOfSockets - 1; ++i)
					NewSocketArray[i + 1].fd = ConnectedClients[i].AssociatedClient,
					NewSocketArray[i + 1].events = POLLIN;
				SocketArray = NewSocketArray;
				break;
			}
			SsAssert(1,
				"failed to regnerate Socket descriptor table, cirtical failure\n"
				"Array at [%p:%d]\n",
				SocketArray, NumberOfSockets);
			break;

		case 1:
			if (auto NewSocketArray = (WSAPOLLFD*)HeapReAlloc(
				ProcessHeap,
				NULL,
				SocketArray,
				NumberOfSockets * sizeof(WSAPOLLFD))) {
			
				NewSocketArray[NumberOfSockets - 1].fd = ConnectedClients[NumberOfSockets - 1].AssociatedClient;
				NewSocketArray[NumberOfSockets - 1].events = POLLIN;
				SocketArray = NewSocketArray;
				break;
			}
			SsAssert(1,
				"failed to add new client to socket descriptor list, heap corruption indicated with: %d\n",
				GetLastError());
		}

		auto Result = WSAPoll(
			SocketArray,
			NumberOfSockets,
			-1);
		if (SsAssert(Result < 0,
			"WSAPoll failed to query socket states with: %d\n",
			WSAGetLastError()))
			break;

		SsLog("Dispatching to notified socket commands \n");
		for (auto i = 0; i < NumberOfSockets; ++i) {

			// Enmerate said Flags and take response option
			constexpr int FlagSetArray[] = {
				POLLERR,
				POLLHUP,
				POLLNVAL,
				POLLRDNORM,
				POLLWRNORM
			};

			// Skipping sockets with no pending operations
			if (!SocketArray[i].revents)
				goto EndOfFlagLoop;
			for (auto j = 0; j < sizeof(FlagSetArray); ++j) {
				
				IoRequestPacket UniversalRequestPacket{
					.RequestingSocket = SocketArray[i].fd,
					.OptionalClient = &ConnectedClients[i - 1],
					.IoRequestStatus = IoRequestPacket::STATUS_REQUEST_NOT_HANDLED
				};
				switch (SocketArray[i].revents & FlagSetArray[j]) {
				case POLLERR:
					SsAssert(1,
						"an error occured on socket [%p:%p] with: %d\n",
						SocketArray[i].fd,
						&SocketArray[i],
						WSAGetLastError());
					return -4;

				case POLLHUP:
					SsLog("Socket is disconnecting [%p]\n",
						SocketArray[i].fd);
					UniversalRequestPacket.IoControlCode = IoRequestPacket::SOCKET_DISCONNECTED;
					IoCompleteRequestRoutine(
						this,
						&UniversalRequestPacket,
						UserContext);
					switch (UniversalRequestPacket.IoRequestStatus) {
					case IoRequestPacket::STATUS_REQUEST_COMPLETED:
						break;
					case IoRequestPacket::STATUS_REQUEST_IGNORED:;
						// remove socket from descriptor list
					}
					break;

				case POLLNVAL:
					SsAssert(1,
						"an invalid socket was specified that was not previously closed and removed from the descriptor list,\n"
						"this indicates a server crash: [%p]\n",
						&SocketArray[i]);
					return -3;

				case POLLRDNORM:
					SsLog("Socket [%p] is requesting to receive a packet\n",
						SocketArray[i].fd);
					UniversalRequestPacket.IoControlCode = IoRequestPacket::SOCKET_DISCONNECTED;
					IoCompleteRequestRoutine(
						this,
						&UniversalRequestPacket,
						UserContext);


				case POLLWRNORM:;
				}
			}
		EndOfFlagLoop:;
		}



	EndOfCycle:; 
	}
	
	return 0;
}
