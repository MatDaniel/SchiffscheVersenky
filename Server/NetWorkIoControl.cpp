// This implements the ClientContoller that manages the clients and provides an IO bridge
#include "ShipServer.h"

ClientController::ClientController(
	size_t InformationSize
) : ClientInfoSize(InformationSize) {}

SOCKET ClientController::GetSocket() const {
	return AssociatedClient;
}

bool ClientController::operator==(
	const ClientController* Rhs
	) const {
	return this == Rhs;
}



NetWorkIoControl* NetWorkIoControl::CreateSingletonOverride(
	const char* ServerPort
) {
	SsLog("NetworkIoCtl Factory called, creating NetworkObject");

	// Magic fuckery cause make_unique cannot normally access a private constructor
	struct EnableMakeUnique : public NetWorkIoControl { 
		inline EnableMakeUnique(
			const char* ServerPort
		) : NetWorkIoControl(
			ServerPort) {}
	};
	return (InstanceObject = std::make_unique<EnableMakeUnique>(ServerPort)).get();
}
NetWorkIoControl* NetWorkIoControl::GetInstance() {
	return InstanceObject.get();
}


NetWorkIoControl::NetWorkIoControl(
	const char* PortNumber
) {
	SsLog("Preparing internal server structures for async IO operation\n"
		"Retrieving portnumber information for localhost\n");
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
		throw -1;

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
		WSAGetLastError())) {

		freeaddrinfo(ServerInformation);
		throw -2;
	}

	Result = bind(
		LocalServerSocket,
		ServerInformationIterator->ai_addr,
		ServerInformationIterator->ai_addrlen);
	freeaddrinfo(ServerInformation);
	ServerInformation = nullptr;
	if (SsAssert(Result,
		"failed to bind socket to port \"%s\", with: %d\n",
		PortNumber,
		WSAGetLastError())) {

		closesocket(LocalServerSocket);
		throw -3;
	}
		
	SsLog("Starting to listen to incoming requests, passively\n");
	Result = listen(
		LocalServerSocket,
		SOMAXCONN);
	if (SsAssert(Result,
		"failed to initilize listening queue with: %d\n",
		WSAGetLastError())) {

		closesocket(LocalServerSocket);
		throw -4;
	}
		
	SocketDescriptorTable.emplace_back(WSAPOLLFD{ 
		.fd = LocalServerSocket,
		.events = POLLIN });
}
NetWorkIoControl::~NetWorkIoControl() {
	SsLog("Destructor of NetworkIoControl called!\n"
		"Cleaning up internal server structures\n");
	

}

SOCKET NetWorkIoControl::GetServerSocketHandle() {
	return LocalServerSocket;
}

void NetWorkIoControl::AcceptIncomingConnection(
	IoRequestPacket& NetworkRequest
) {
	SsLog("Passing through incoming connection\n");
	ClientController AcceptingClient(sizeof(ClientController::ClientInformation));
	AcceptingClient.AssociatedClient = accept(
		NetworkRequest.RequestingSocket,
		&AcceptingClient.ClientInformation,
		&AcceptingClient.ClientInfoSize);
	if (SsAssert(AcceptingClient.AssociatedClient == INVALID_SOCKET,
		"failed to open connection, wtf happened i have no idea: %d",
		WSAGetLastError())) {

		NetworkRequest.IoRequestStatus = IoRequestPacket::STATUS_REQUEST_ERROR;
		return;
	}

	ConnectedClients.emplace_back(AcceptingClient);
	SocketDescriptorTable.emplace_back(WSAPOLLFD{
		.fd = AcceptingClient.AssociatedClient,
		.events = POLLIN });
	NetworkRequest.IoRequestStatus = IoRequestPacket::STATUS_LIST_MODIFIED;
}



long NetWorkIoControl::PollNetworkConnectionsAndDispatchCallbacks(
	MajorFunction IoCompleteRequestRoutine,
	void*         UserContext,
	int32_t       Timeout
) {
	auto Result = WSAPoll(
		SocketDescriptorTable.data(),
		SocketDescriptorTable.size(),
		Timeout);
	if (SsAssert(Result <= 0,
		"WSAPoll failed to query socket states with: %d\n",
		WSAGetLastError()))
		return Result;

	SsLog("Dispatching to notified socket commands\n");
	for (auto i = 0; i < SocketDescriptorTable.size(); ++i) {

		auto BuildNetworkRequest = [this](
			SOCKET                               SocketDescriptor,
			IoRequestPacket::IoServiceRoutineCtl IoControlCode
			) -> IoRequestPacket {

				// Lookup client list for matching client entry and create association
				ClientController* Associate = nullptr;
				for (auto& Client : ConnectedClients)
					if (Client.GetSocket() == SocketDescriptor) {
						Associate = &Client; break;
					}

				return { .IoControlCode = IoControlCode,
						 .OptionalClient = Associate,
						 .IoRequestStatus = IoRequestPacket::STATUS_REQUEST_NOT_HANDLED };
		};

		auto ReturnEvent = SocketDescriptorTable[i].revents;
		if (!ReturnEvent)
			continue;
		if (ReturnEvent & POLLERR) {
			
			SsAssert(1,
				"an error occured on socket [%p:%p] with: %d\n",
				SocketDescriptorTable[i].fd,
				&SocketDescriptorTable[i],
				WSAGetLastError());
			return -4;
		}
		if (ReturnEvent & POLLHUP) {
			
			SsLog("Socket [%p] disconnected from server\n",
				SocketDescriptorTable[i].fd);
			
			// Notify endpoint of disconnect
			auto NetworkRequest = BuildNetworkRequest(SocketDescriptorTable[i].fd,
				IoRequestPacket::SOCKET_DISCONNECTED);
			IoCompleteRequestRoutine(
				*this,
				NetworkRequest,
				UserContext);

			// Removing client from entry list
			if (auto Associate = NetworkRequest.OptionalClient)
				ConnectedClients.erase(
					std::find(
						ConnectedClients.begin(),
						ConnectedClients.end(),
						Associate));
			SocketDescriptorTable.erase(
				SocketDescriptorTable.begin() + i);

			return IoRequestPacket::STATUS_LIST_MODIFIED;
		}
		if (ReturnEvent & POLLNVAL) {
			
			SsAssert(1,
				"an invalid socket was specified that was not previously closed and removed from the descriptor list,\n"
				"this indicates a server crash: [%p]\n",
				&SocketDescriptorTable[i]);
			return -3;
		}
		if (ReturnEvent & POLLRDNORM) {
			
			SsLog("Socket [%p] is requesting to receive a packet\n",
				SocketDescriptorTable[i].fd);

			auto NetworkRequest = BuildNetworkRequest(
				SocketDescriptorTable[i].fd,
				i ? IoRequestPacket::INCOMING_PACKET : IoRequestPacket::INCOMING_CONNECTION);
			if (i == 0)
				NetworkRequest.RequestingSocket = LocalServerSocket;
			IoCompleteRequestRoutine(
				*this,
				NetworkRequest,
				UserContext);

			switch (NetworkRequest.IoRequestStatus) {
			case IoRequestPacket::STATUS_LIST_MODIFIED:
				return NetworkRequest.IoRequestStatus;
			}

			if (NetworkRequest.IoRequestStatus < 0)
				return NetworkRequest.IoRequestStatus;
		}
		if (ReturnEvent & POLLWRNORM) {

			SsLog("Notifying callee of socket [%p] being ready for data input\n",
				SocketDescriptorTable[i].fd);
			auto NetworkRequest = BuildNetworkRequest(
				SocketDescriptorTable[i].fd,
				IoRequestPacket::OUTGOING_PACKET_COMPLETE);
			IoCompleteRequestRoutine(
				*this,
				NetworkRequest,
				UserContext);

			if (NetworkRequest.IoRequestStatus < 0)
				return NetworkRequest.IoRequestStatus;
		}
	}

	return IoRequestPacket::STATUS_REQUEST_COMPLETED;
}
