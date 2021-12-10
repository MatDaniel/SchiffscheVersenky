// This module implements the network io manager (pseudo singleton),
// it provides an io bride to several clients, 
// and manages them in a multiplexed independent asynchronous manner
module;

#include <SharedLegacy.h>
#include <memory>
#include <vector>

export module NetworkIoControl;
import ShipSock;
using namespace std;



export spdlogger ServerLog;
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
	ClientController(
		size_t InformationSize
	) 
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
		// Magic fuckery cause make_unique cannot normally access a private constructor
		struct EnableMakeUnique : public NetWorkIoControl {
			inline EnableMakeUnique(
				const char* ServerPort
			) : NetWorkIoControl(
				ServerPort) {}
		};

		InstanceObject = make_unique<EnableMakeUnique>(ServerPort);
		ServerLog->info("Factory created NetworkIoCtl object at {}",
			(void*)InstanceObject.get());
		return InstanceObject.get();
	}
	static NetWorkIoControl* GetInstance() {
		return InstanceObject.get();
	}

	~NetWorkIoControl() {
		ServerLog->info("Network manager destroyed, cleaning up sockets");
	}
	NetWorkIoControl(
		const NetWorkIoControl&) = delete;
	NetWorkIoControl& operator=(
		const NetWorkIoControl&) = delete;



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
		if (Result <= 0)
			return ServerLog->error("WSAPoll failed to query socket states with: {}",
				WSAGetLastError()), Result;
		
		for (auto i = 0; i < SocketDescriptorTable.size(); ++i) {

			// Network request factory helper
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
			
			if (ReturnEvent & POLLERR)
				return ServerLog->error("WSA error on socket [{:04x}]",
					SocketDescriptorTable[i].fd), -4;

			if (ReturnEvent & POLLHUP) {

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
						find(
							ConnectedClients.begin(),
							ConnectedClients.end(),
							Associate));
				SocketDescriptorTable.erase(
					SocketDescriptorTable.begin() + i);

				ServerLog->warn("Socket [{:04x}] was disconnected from the server by request",
					SocketDescriptorTable[i].fd);
				return IoRequestPacket::STATUS_LIST_MODIFIED;
			}
			if (ReturnEvent & POLLNVAL)
				return ServerLog->error("An invalid socket was specified that was not removed from the descriptor list: [{:04x}]",
					SocketDescriptorTable[i].fd), -3;
				
			if (ReturnEvent & POLLRDNORM) {

				ServerLog->info("Socket [{:04x}] is requesting to receive a packet",
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
					return ServerLog->warn("Client list has been modified, iterators destroyed"),
						NetworkRequest.IoRequestStatus;
				}

				if (NetworkRequest.IoRequestStatus < 0)
					return ServerLog->error("Callback failed to handle critical request [{}]", 
						(void*)&NetworkRequest), NetworkRequest.IoRequestStatus;
			}
			if (ReturnEvent & POLLWRNORM) {

				ServerLog->info("Client [{:04x}] is ready to receive data",
					SocketDescriptorTable[i].fd);

				auto NetworkRequest = BuildNetworkRequest(
					SocketDescriptorTable[i].fd,
					IoRequestPacket::OUTGOING_PACKET_COMPLETE);
				IoCompleteRequestRoutine(
					*this,
					NetworkRequest,
					UserContext);

				if (NetworkRequest.IoRequestStatus < 0)
					return ServerLog->error("Callback failed to handle critical request [{}]",
						(void*)&NetworkRequest), NetworkRequest.IoRequestStatus;
			}
		}

		return ServerLog->info("All incoming requests have been handled successfully"),
			IoRequestPacket::STATUS_REQUEST_COMPLETED;
	}

	void AcceptIncomingConnection(
		IoRequestPacket& NetworkRequest
	) {
		// Accepting incoming connection and allocating controller
		ClientController AcceptingClient(sizeof(ClientController::ClientInformation));
		AcceptingClient.AssociatedClient = accept(
			NetworkRequest.RequestingSocket,
			&AcceptingClient.ClientInformation,
			&AcceptingClient.ClientInfoSize);
		if (AcceptingClient.AssociatedClient == INVALID_SOCKET) {

			NetworkRequest.IoRequestStatus = IoRequestPacket::STATUS_REQUEST_ERROR;
			ServerLog->error("WSA accept failed to connect with {}",
				WSAGetLastError());
			return;
		}

		// Adding client to local client list and socket descriptor table
		ConnectedClients.emplace_back(AcceptingClient);
		SocketDescriptorTable.emplace_back(WSAPOLLFD{
			.fd = AcceptingClient.AssociatedClient,
			.events = POLLIN });
		NetworkRequest.IoRequestStatus = IoRequestPacket::STATUS_LIST_MODIFIED;
		ServerLog->info("Client on socket [{:04x}] was sucessfully connected",
			AcceptingClient.AssociatedClient);
	}
	SOCKET GetServerSocketHandle() {
		return LocalServerSocket;
	}

private:
	NetWorkIoControl(
		const char* PortNumber
	) {
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
		if (Result)
			throw (ServerLog->error("Failed to retreive port information for {} with {}",
				PortNumber, Result), -1);
		ServerLog->info("Retrieved port information for {}", PortNumber);

		auto ServerInformationIterator = ServerInformation;
		do {
			LocalServerSocket = socket(
				ServerInformationIterator->ai_family,
				ServerInformationIterator->ai_socktype,
				ServerInformationIterator->ai_protocol);
		} while (LocalServerSocket == INVALID_SOCKET &&
			(ServerInformationIterator = ServerInformationIterator->ai_next));
		if (LocalServerSocket == INVALID_SOCKET)
			throw (freeaddrinfo(ServerInformation),
			ServerLog->error("failed to create socket for client with {}",
				WSAGetLastError()), -2);
		ServerLog->info("Created socket [{:04x}] for network manager", LocalServerSocket);

		Result = ::bind(
			LocalServerSocket,
			ServerInformationIterator->ai_addr,
			ServerInformationIterator->ai_addrlen);
		freeaddrinfo(ServerInformation);
		ServerInformation = nullptr;
		if (Result == SOCKET_ERROR)
			throw (closesocket(LocalServerSocket),
			ServerLog->error("failed to bind port {} on socket [{:04x}] with {}",
				PortNumber, LocalServerSocket, WSAGetLastError()), -3);
		ServerLog->info("Bound port {} to socket [{:04x}]",
			PortNumber, LocalServerSocket);

		Result = listen(
			LocalServerSocket,
			SOMAXCONN);
		if (Result == SOCKET_ERROR)
			throw (closesocket(LocalServerSocket),
				ServerLog->error("Failed to switch socket [{:04x}] to listening mode with {}",
					LocalServerSocket, WSAGetLastError()), -4);

		SocketDescriptorTable.emplace_back(WSAPOLLFD{
			.fd = LocalServerSocket,
			.events = POLLIN });
		ServerLog->info("Created network manager object successfully on socket [{:04x}:{}]",
			LocalServerSocket, PortNumber);
	}

	SOCKET                   LocalServerSocket = INVALID_SOCKET;
	vector<ClientController> ConnectedClients;
	vector<WSAPOLLFD>        SocketDescriptorTable;

	static inline unique_ptr<NetWorkIoControl> InstanceObject;
};
