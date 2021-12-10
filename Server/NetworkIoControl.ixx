// This module implements the network io manager (pseudo singleton),
// it provides an io bride to several clients, 
// and manages them in a multiplexed independent asynchronous manner
module;

#include <ShipSock.h>
#include <memory>
#include <vector>

export module NetworkIoControl;



export class NetWorkIoControl;

export class ClientController {
	friend NetWorkIoControl;
public:
	SOCKET GetSocket() const {
		return AssociatedClient;
	}

	bool operator==(
		const ClientController* Rhs
		) const {
		return this == Rhs;
	}	

private:
	ClientController(size_t InformationSize) 
		: ClientInfoSize(InformationSize) {}

	SOCKET   AssociatedClient;
	sockaddr ClientInformation;
	int32_t  ClientInfoSize;
};

class NetWorkIoControl
	: private SocketWrap {
public:
	struct IoRequestPacket {
		enum IoServiceRoutineCtl {           // a control code specifying the type of handling required
			NO_COMMAND,                      // reserved for @Lima, dont use
			INCOMING_CONNECTION,
			INCOMING_PACKET,
			INCOMING_PACKET_PRIORITIZED,
			OUTGOING_PACKET_COMPLETE,
			SOCKET_DISCONNECTED,
			SERVICE_ERROR_DETECTED,
		}                 IoControlCode;
		SOCKET            RequestingSocket; // the socket which was responsible for the incoming request
		ClientController* OptionalClient;   // an optional pointer set by the NetworkManager to the client responsible for the request,
											// on accept this will contain the information about the connecting client the callback can decide to accept the connection

		enum IoRequestStatusType {      // describes the current state of this network request packet,
										// the callback has to set it correctly depending on what it did with the request
			STATUS_REQUEST_ERROR = -3,
			STATUS_REQUEST_NOT_HANDLED = -2,
			INVALID_STATUS = -1,
			STATUS_REQUEST_COMPLETED = 0,
			STATUS_REQUEST_IGNORED = 1,
			STATUS_LIST_MODIFIED = 2
		} IoRequestStatus;
	};
	typedef void (*MajorFunction)(        // this has to handle the networking requests being both capable of reading and sending requests
		NetWorkIoControl& NetworkDevice,  // a pointer to the NetWorkIoController responsible of said request packet
		IoRequestPacket& NetworkRequest, // a pointer to a network request packet describing the current request
		void* UserContext     // A pointer to caller defined data thats forwarded to the callback in every call, could be the GameManager class or whatever
		);



	static NetWorkIoControl* CreateSingletonOverride(
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
	static NetWorkIoControl* GetInstance() {
		return InstanceObject.get();
	}

	~NetWorkIoControl() {
		SsLog("Destructor of NetworkIoControl called!\n"
			"Cleaning up internal server structures\n");
	}
	NetWorkIoControl(const NetWorkIoControl&) = delete;
	NetWorkIoControl& operator=(const NetWorkIoControl&) = delete;



	long PollNetworkConnectionsAndDispatchCallbacks( // Starts internally handling accept requests passively,
													 // notifies caller over callbacks and handles io requests
													 // @Sherlock will have to call this with his game manager ready,
													 // the callback is responsible for handling incoming io and connections.
		MajorFunction IoCompleteRequestRoutine,      // a function pointer to a callback that handles socket operations
		void*         UserContext,                   // a user supplied buffer containing arbitrary data forwarded to the callback
		int32_t       Timeout = -1                   // specifies the timeout count the functions waits on poll till it returns
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

	void AcceptIncomingConnection(
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
	SOCKET GetServerSocketHandle() {
		return LocalServerSocket;
	}

private:
	NetWorkIoControl(
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

	SOCKET                        LocalServerSocket = INVALID_SOCKET;
	std::vector<ClientController> ConnectedClients;
	std::vector<WSAPOLLFD>        SocketDescriptorTable;

	static std::unique_ptr<NetWorkIoControl> InstanceObject;
};
std::unique_ptr<NetWorkIoControl> NetWorkIoControl::InstanceObject;
