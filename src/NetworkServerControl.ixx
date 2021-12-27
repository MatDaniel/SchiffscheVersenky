// This module implements the network io manager (pseudo singleton),
// it provides an io bride to several clients, 
// and manages them in a multiplexed independent asynchronous manner
module;

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include "BattleShip.h"
#include <memory>
#include <vector>
#include <span>

export module Network.Server;
export import LayerBase;
using namespace std;



// Server sided network management code
export namespace Network::Server {
	using namespace Network;

	// A per client instance of an Io bridge
	class NwClientControl {
		friend class NetworkManager2;
	public:
		~NwClientControl() {
			TRACE_FUNTION_PROTO;

			SPDLOG_LOGGER_INFO(NetworkLog, "Client controller has been closed");
		}
		bool operator==(
			const NwClientControl* Rhs
			) const {
			TRACE_FUNTION_PROTO;

			return this == Rhs;
		}

		int32_t SendShipControlPackageDynamic(    // Sends a package to the 
			const ShipSockControl& RequestPackage
		) {
			TRACE_FUNTION_PROTO;

			// get size of package dynamically
			// send package

			auto PackageSize = sizeof(RequestPackage); // this has to be done properly, skipping it for now



			auto Result = send(AssociatedClient,
				(char*)&RequestPackage,
				PackageSize, 0);

			if (Result != SOCKET_ERROR)
				return SPDLOG_LOGGER_INFO(NetworkLog, "Send package to client [{:04x}]",
					AssociatedClient), Result;

			switch (auto WSAError = WSAGetLastError()) {
			case WSAEWOULDBLOCK:
				SPDLOG_LOGGER_WARN(NetworkLog, "Socket would block, this should never appear as the server doesn't use nonblocking sockets");

			default:
				SPDLOG_LOGGER_ERROR(NetworkLog, "Internal server error, failed to send package {}", WSAError);
			}

			return Result;
		}

		int32_t RaiseStatusMessage(
			ShipControlStatus StatusMessage
		) {
			TRACE_FUNTION_PROTO;

			return SendShipControlPackageDynamic(
				(const ShipSockControl&)ShipSockControl {
				.ControlCode = ShipSockControl::RAISE_STATUS_MESSAGE,
					.ShipControlRaisedStatus = StatusMessage
			});
		}

		SOCKET GetSocket() const {
			TRACE_FUNTION_PROTO;

			return AssociatedClient;
		}

	private:
		SOCKET   AssociatedClient;
		sockaddr ClientInformation;
		int32_t  ClientInfoSize = sizeof(ClientInformation);
	};



	struct NwRequestPacket
		: public NwRequestBase {
	
		SOCKET            RequestingSocket; // the socket which was responsible for the incoming request
		NwClientControl* OptionalClient;   // an optional pointer set by the NetworkManager to the client responsible for the request,
											// on accept this will contain the information about the connecting client the callback can decide to accept the connection

		bool StatusListModified = false;    // Internal flag relevant to the socket descriptor list, as it logs modification and restarts the evaluation loop



		NwRequestStatus CompleteIoRequest(
			NwRequestStatus RequestStatus
		) override {
			TRACE_FUNTION_PROTO;

			switch (RequestStatus) {
			case STATUS_LIST_MODIFIED:
				StatusListModified |= true;
				[[fallthrough]];

			default:
				return NwRequestBase::CompleteIoRequest(RequestStatus);
			}
		}
	};

	class NetworkManager2
		: public MagicInstanceManagerBase<NetworkManager2>,
		  private WsaNetworkBase {
		friend class MagicInstanceManagerBase<NetworkManager2>;
	public:
		typedef void (*MajorFunction)(       // this has to handle the networking requests being both capable of reading and sending requests
			NetworkManager2& NetworkDevice,  // a pointer to the NetWorkIoController responsible of said request packet
			NwRequestPacket& NetworkRequest, // a pointer to a network request packet describing the current request
			void*            UserContext     // A pointer to caller defined data thats forwarded to the callback in every call, could be the GameManager class or whatever
			);

		~NetworkManager2() {
			TRACE_FUNTION_PROTO;

			SPDLOG_LOGGER_INFO(NetworkLog, "Network manager destroyed, cleaning up sockets");
		}
	


		long PollNetworkConnectionsAndDispatchCallbacks( // Starts internally handling accept requests passively,
														 // notifies caller over callbacks and handles io requests
														 // @Sherlock will have to call this with his game manager ready,
														 // the callback is responsible for handling incoming io and connections.
			MajorFunction IoCompleteRequestRoutine,      // a function pointer to a callback that handles socket operations
			void*         UserContext,                   // a user supplied buffer containing arbitrary data forwarded to the callback
			int32_t       Timeout = -1                   // specifies the timeout count the functions waits on poll till it returns
		) {
			TRACE_FUNTION_PROTO;

			auto Result = WSAPoll(
				SocketDescriptorTable.data(),
				SocketDescriptorTable.size(),
				Timeout);
			if (Result <= 0)
				return SPDLOG_LOGGER_ERROR(NetworkLog, "WSAPoll failed to query socket states with: {}",
					WSAGetLastError()), Result;

			for (auto i = 0; i < SocketDescriptorTable.size(); ++i) {

				const auto BuildNetworkRequest = [this](            // Request factory helper, builds a quick and dirty network request
					SOCKET                        SocketDescriptor, // A socket that will be used to locate its associated client
					NwRequestPacket::NwServiceCtl IoControlCode     // The io control code that should be used to build the request
					) -> NwRequestPacket {

						// Lookup client list for matching client entry and create association
						NwClientControl* Associate = nullptr;
						for (auto& Client : ConnectedClients)
							if (Client.GetSocket() == SocketDescriptor) {
								Associate = &Client; break;
							}
						
						NwRequestPacket ReturnValue{};
						ReturnValue.IoControlCode = IoControlCode;
						ReturnValue.OptionalClient = Associate;
						ReturnValue.IoRequestStatus = NwRequestPacket::STATUS_REQUEST_NOT_HANDLED;
						return ReturnValue;
				};

				// Skip current round if there are no return events listed
				auto ReturnEvent = SocketDescriptorTable[i].revents;
				if (!ReturnEvent)
					continue;

				// Query for soft and hard disconnects + general faults
				if (ReturnEvent & (POLLERR | POLLHUP | POLLNVAL)) {

					// Notify endpoint of disconnect
					auto NetworkRequest = BuildNetworkRequest(SocketDescriptorTable[i].fd,
						NwRequestPacket::SOCKET_DISCONNECTED);
					IoCompleteRequestRoutine(
						*this,
						NetworkRequest,
						UserContext);
					if (NetworkRequest.IoRequestStatus < 0)
						return SPDLOG_LOGGER_CRITICAL(NetworkLog, "Callback failed to handle client disconnect, aborting server"),
						NetworkRequest.IoRequestStatus;

					// Removing client from entry/descriptor lists
					if (auto Associate = NetworkRequest.OptionalClient)
						ConnectedClients.erase(
							Associate - ConnectedClients.data() + ConnectedClients.begin());
					SPDLOG_LOGGER_INFO(NetworkLog, "Socket [{:04x}] was disconnected from the server {}",
						SocketDescriptorTable[i].fd,
						GetLastWSAErrorOfSocket(
							SocketDescriptorTable[i].fd));
					SocketDescriptorTable.erase(
						SocketDescriptorTable.begin() + i);

					return NwRequestPacket::STATUS_LIST_MODIFIED;
				}

				// Input/Output related handling
				if (ReturnEvent & POLLRDNORM) {

					// Dispatch read command
					SPDLOG_LOGGER_INFO(NetworkLog, "Socket [{:04x}] is requesting to receive a packet",
						SocketDescriptorTable[i].fd);

					auto NetworkRequest = BuildNetworkRequest(
						SocketDescriptorTable[i].fd,
						i ? NwRequestPacket::INCOMING_PACKET
						: NwRequestPacket::INCOMING_CONNECTION);
					if (i == 0)
						NetworkRequest.RequestingSocket = LocalServerSocket;
					IoCompleteRequestRoutine(
						*this,
						NetworkRequest,
						UserContext);

					if (i == 0 && NetworkRequest.StatusListModified)
						return SPDLOG_LOGGER_WARN(NetworkLog, "Client list has been modified, iterators destroyed"),
						NetworkRequest.IoRequestStatus;
					if (NetworkRequest.IoRequestStatus < 0)
						return SPDLOG_LOGGER_ERROR(NetworkLog, "Callback failed to handle critical request [{}]",
							(void*)&NetworkRequest), NetworkRequest.IoRequestStatus;
				}
				if (ReturnEvent & POLLWRNORM) {

					SPDLOG_LOGGER_INFO(NetworkLog, "Client [{:04x}] is ready to receive data",
						SocketDescriptorTable[i].fd);

					auto NetworkRequest = BuildNetworkRequest(
						SocketDescriptorTable[i].fd,
						NwRequestPacket::OUTGOING_PACKET_COMPLETE);
					IoCompleteRequestRoutine(
						*this,
						NetworkRequest,
						UserContext);

					if (NetworkRequest.IoRequestStatus < 0)
						return SPDLOG_LOGGER_ERROR(NetworkLog, "Callback failed to handle critical request [{}]",
							(void*)&NetworkRequest), NetworkRequest.IoRequestStatus;
				}
			}

			return SPDLOG_LOGGER_INFO(NetworkLog, "All incoming requests have been handled successfully"),
				NwRequestPacket::STATUS_REQUEST_COMPLETED;
		}

		NwClientControl* AcceptIncomingConnection( // Accepts an incoming connection and allocates the client,
													// if successful invalidates all references to existing clients
													// This provides default request handling, RequestStatus modified
			NwRequestPacket& NetworkRequest         // The NetworkRequest to apply accept handling on
		) {
			TRACE_FUNTION_PROTO;

			// Accepting incoming connection and allocating controller
			NwClientControl ConnectingClient;
			ConnectingClient.AssociatedClient = accept(
				NetworkRequest.RequestingSocket,
				&ConnectingClient.ClientInformation,
				&ConnectingClient.ClientInfoSize);
			if (ConnectingClient.AssociatedClient == INVALID_SOCKET)
				return NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_REQUEST_ERROR),
				SPDLOG_LOGGER_ERROR(NetworkLog, "WSA accept failed to connect with {}",
					WSAGetLastError()),
				nullptr;
			SPDLOG_LOGGER_INFO(NetworkLog, "Accepted client connection");

			// Adding client to local client list and socket descriptor table
			auto ClientAddress = &ConnectedClients.emplace_back(ConnectingClient);
			SocketDescriptorTable.emplace_back(WSAPOLLFD{
				.fd = ConnectingClient.AssociatedClient,
				.events = POLLIN });
			NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_LIST_MODIFIED);
			SPDLOG_LOGGER_INFO(NetworkLog, "Client on socket [{:04x}] was sucessfully connected",
				ConnectingClient.AssociatedClient);
			return ClientAddress;
		}
		SOCKET GetServerSocketHandle() {
			TRACE_FUNTION_PROTO;

			return LocalServerSocket;
		}
		uint32_t GetNumberOfConnectedClients() {
			TRACE_FUNTION_PROTO;

			return ConnectedClients.size();
		}
		auto GetClientList() {
			TRACE_FUNTION_PROTO;

			return span(ConnectedClients);
		}
		NwClientControl* GetClientBySocket(
			SOCKET SocketAsId
		) {
			TRACE_FUNTION_PROTO;

			for (auto& Client : ConnectedClients)
				if (Client.AssociatedClient == SocketAsId)
					return &Client;
			return nullptr;
		}



	private:
		NetworkManager2(
			const char* PortNumber
		) {
			TRACE_FUNTION_PROTO;

			addrinfo* ServerInformation;
			auto Result = getaddrinfo(NULL,
				PortNumber,
				&(const addrinfo&)addrinfo{
					.ai_flags = AI_PASSIVE,
					.ai_family = AF_INET,
					.ai_socktype = SOCK_STREAM,
					.ai_protocol = IPPROTO_TCP
				},
				&ServerInformation);
			if (Result)
				throw (SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to retreive port information for {} with {}",
					PortNumber, Result),
					runtime_error("Failed to retrieve port information"));
			SPDLOG_LOGGER_INFO(NetworkLog, "Retrieved port information for {}", PortNumber);

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
					SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to create io handler socket for connect with {}",
						WSAGetLastError()),
					runtime_error("Failed to create io handler socket for connect"));
			SPDLOG_LOGGER_INFO(NetworkLog, "Created socket [{:04x}] for network manager", LocalServerSocket);

			Result = ::bind(
				LocalServerSocket,
				ServerInformationIterator->ai_addr,
				ServerInformationIterator->ai_addrlen);
			freeaddrinfo(ServerInformation);
			ServerInformation = nullptr;
			if (Result == SOCKET_ERROR)
				throw (freeaddrinfo(ServerInformation),
					closesocket(LocalServerSocket),
					SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to bind port {} on socket [{:04x}] with {}",
						PortNumber, LocalServerSocket, WSAGetLastError()),
					runtime_error("Failed to bind io socket"));
			SPDLOG_LOGGER_INFO(NetworkLog, "Bound port {} to socket [{:04x}]",
				PortNumber, LocalServerSocket);

			Result = listen(
				LocalServerSocket,
				SOMAXCONN);
			if (Result == SOCKET_ERROR)
				throw (freeaddrinfo(ServerInformation),
					closesocket(LocalServerSocket),
					SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to switch socket [{:04x}] to listening mode with {}",
						LocalServerSocket, WSAGetLastError()),
					runtime_error("Failed to switch io socket to listening mode"));

			SocketDescriptorTable.emplace_back(WSAPOLLFD{
				.fd = LocalServerSocket,
				.events = POLLIN });
			SPDLOG_LOGGER_INFO(NetworkLog, "Created network manager object successfully on socket [{:04x}:{}]",
				LocalServerSocket, PortNumber);
		}

		int32_t GetLastWSAErrorOfSocket(
			SOCKET SocketToProbe
		) {
			TRACE_FUNTION_PROTO;

			int32_t SocketErrorCode = SOCKET_ERROR;
			int32_t ProbeSize = sizeof(SocketErrorCode);
			if (getsockopt(SocketToProbe,
				SOL_SOCKET,
				SO_ERROR,
				(char*)&SocketErrorCode,
				&ProbeSize) == SOCKET_ERROR)
				return SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to query last error on socket [{:04x}], query error {}",
					SocketToProbe, WSAGetLastError()),
				SOCKET_ERROR;
			return SocketErrorCode;
		}



		SOCKET                   LocalServerSocket = INVALID_SOCKET;
		vector<NwClientControl> ConnectedClients;
		vector<WSAPOLLFD>        SocketDescriptorTable;
	};
}
