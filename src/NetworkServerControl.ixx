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
			TRACE_FUNTION_PROTO; SPDLOG_LOGGER_INFO(NetworkLog, "Client controller has been closed");
		}
		bool operator==(
			const NwClientControl* Rhs
			) const {
			TRACE_FUNTION_PROTO; return this == Rhs;
		}

		// TODO: rewrite
		int32_t SendShipControlPackageDynamic(    // Sends a package to the connected remote,
		                                          // returns the size of the send data or SOCKET_ERROR on error
			const ShipSockControl& RequestPackage // The Package to be send, the data behind the struct may actually be bigger
		) {
			TRACE_FUNTION_PROTO;
			
			// get size of package dynamically
			// send package

			auto PackageSize = sizeof(RequestPackage); // this has to be done properly, skipping it for now



			auto Result = send(SocketHandle,
				(char*)&RequestPackage,
				PackageSize, 0);

			if (Result != SOCKET_ERROR)
				return SPDLOG_LOGGER_INFO(NetworkLog, "Send package to client [{:04x}]",
					SocketHandle), Result;

			switch (auto WSAError = WSAGetLastError()) {
			case WSAEWOULDBLOCK:
				SPDLOG_LOGGER_WARN(NetworkLog, "Socket would block, this should never appear as the server doesn't use nonblocking sockets");
				break;

			default:
				SPDLOG_LOGGER_ERROR(NetworkLog, "Internal server error, failed to send package {}", WSAError);
			}

			return Result;
		}

		int32_t RaiseStatusMessage(
			ShipSockControl::ShipControlStatus StatusMessage
		) {
			TRACE_FUNTION_PROTO;

			return SendShipControlPackageDynamic(
				(const ShipSockControl&)ShipSockControl {
					.ControlCode = ShipSockControl::RAISE_STATUS_MESSAGE,
					.ShipControlRaisedStatus = StatusMessage
			});
		}

		SOCKET GetSocket() const {
			TRACE_FUNTION_PROTO; return SocketHandle;
		}

	private:
		SOCKET   SocketHandle;
		sockaddr ClientInformation;
		int32_t  ClientInfoSize = sizeof(ClientInformation);
	};



	struct NwRequestPacket
		: public NwRequestBase {
	
		SOCKET           RequestingSocket; // the socket which was responsible for the incoming request
		NwClientControl* OptionalClient;   // an optional pointer set by the NetworkManager to the client responsible for the request,
											// on accept this will contain the information about the connecting client the callback can decide to accept the connection

		bool StatusListModified = false;    // Internal flag relevant to the socket descriptor list, as it logs modification and restarts the evaluation loop

		[[noreturn]] NwRequestStatus CompleteIoRequest(
			NwRequestStatus RequestStatus
		) override {
			TRACE_FUNTION_PROTO;

			switch (RequestStatus) {
			case STATUS_LIST_MODIFIED:
				StatusListModified |= true;
				[[fallthrough]];

			default:
				NwRequestBase::CompleteIoRequest(RequestStatus);

				// we now raise the package in order to immediately return to the manager
				// The package is thrown as a pointer to the original passed request
				throw this;
			}
		}
	};

	class NetworkManager2
		: public MagicInstanceManagerBase<NetworkManager2>,
		  private WsaNetworkBase {
		friend class MagicInstanceManagerBase<NetworkManager2>;
	public:
		typedef void (*MajorFunction)(       // this has to handle the networking requests being both capable of reading and sending requests
			NetworkManager2* NetworkDevice,  // a pointer to the NetWorkIoController responsible of said request packet
			NwRequestPacket& NetworkRequest, // a reference to a network request packet describing the current request
			void*            UserContext     // A pointer to caller defined data thats forwarded to the callback in every call, could be the GameManager class or whatever
			);

		~NetworkManager2() {
			TRACE_FUNTION_PROTO; SPDLOG_LOGGER_INFO(NetworkLog, "Network manager destroyed, cleaning up sockets");
		}
	

		NwRequestPacket::NwRequestStatus
		BuildRequestDispatchAndReturn(                      // Builds, dispatches and catches a network request
			MajorFunction                 IoServiceRoutine, // The passed routine responsible for handling
			void*                         UserContext,      // The passed user context
			SOCKET                        SocketAsId,       // Used to locate a client to associate to
			NwRequestPacket::NwServiceCtl IoControlCode     // Describes the request to handle
		) {
			// Build basic request packet
			NwRequestPacket RequestPacket{};
			RequestPacket.IoControlCode = IoControlCode;
			RequestPacket.OptionalClient = GetClientBySocket(SocketAsId);
			RequestPacket.RequestingSocket = SocketAsId;
			RequestPacket.IoRequestStatus = NwRequestPacket::STATUS_REQUEST_NOT_HANDLED;

			// Dispatch request and checkout
			try {
				IoServiceRoutine(this,
					RequestPacket,
					UserContext);

				// If execution resumes here the packet was not completed, server has to terminate,
				// as the state of several components could now be indeterminate 
				SPDLOG_LOGGER_CRITICAL(NetworkLog, "The network request returned, it was not completed");
				return NwRequestPacket::STATUS_REQUEST_NOT_HANDLED;
			}
			catch (const NwRequestPacket* CompletedPacket) {

				// Check if allocated packet is the same as returned
				if (CompletedPacket != &RequestPacket)
					return SPDLOG_LOGGER_ERROR(NetworkLog, "The package completed doesnt match the send package signature"),
						NwRequestPacket::STATUS_REQUEST_ERROR;
				return CompletedPacket->StatusListModified ? 
					NwRequestPacket::STATUS_LIST_MODIFIED : CompletedPacket->IoRequestStatus;
			}
		}

		NwRequestPacket::NwRequestStatus
		PollNetworkConnectionsAndDispatchCallbacks( // Starts internally handling accept requests passively,
													// notifies caller over callbacks and handles io requests
													// @Sherlock will have to call this with his game manager ready,
													// the callback is responsible for handling incoming io and connections.
			MajorFunction IoCompleteRequestRoutine, // a function pointer to a callback that handles socket operations
			void*         UserContext,              // a user supplied buffer containing arbitrary data forwarded to the callback
			int32_t       Timeout = -1              // specifies the timeout count the functions waits on poll till it returns
		) {
			TRACE_FUNTION_PROTO;

		EvaluationLoopReset:
			auto Result = WSAPoll(
				SocketDescriptorTable.data(),
				SocketDescriptorTable.size(),
				Timeout);
			if (Result <= 0)
				return SPDLOG_LOGGER_ERROR(NetworkLog, "WSAPoll() failed to query socket states with: {}",
					WSAGetLastError()),
					NwRequestPacket::STATUS_SOCKET_ERROR;

			for (auto i = 0; i < SocketDescriptorTable.size(); ++i) {
				
				// Skip current round if there are no return events listed
				auto ReturnEvent = SocketDescriptorTable[i].revents;
				if (!ReturnEvent)
					continue;

				// Query for soft and hard disconnects + general faults
				if (ReturnEvent & (POLLERR | POLLHUP | POLLNVAL)) {

					// Notify endpoint of disconnect
					auto IoStatus = BuildRequestDispatchAndReturn(IoCompleteRequestRoutine,
						UserContext,
						SocketDescriptorTable[i].fd,
						NwRequestPacket::SOCKET_DISCONNECTED);
					if (IoStatus < 0)
						return SPDLOG_LOGGER_CRITICAL(NetworkLog, "Callback failed to handle client disconnect, aborting server"),
							IoStatus;

					// Removing client from entry/descriptor lists
					if (auto Associate = GetClientBySocket(SocketDescriptorTable[i].fd))
						ConnectedClients.erase(
							Associate - ConnectedClients.data() + ConnectedClients.begin());
					SPDLOG_LOGGER_INFO(NetworkLog, "Socket [{:04x}] was disconnected from the server {}",
						SocketDescriptorTable[i].fd,
						GetLastWSAErrorOfSocket(
							SocketDescriptorTable[i].fd));
					SocketDescriptorTable.erase(
						SocketDescriptorTable.begin() + i);
					SPDLOG_LOGGER_WARN(NetworkLog, "Client list has been modified, iterators destroyed");

					// Reset current state of the loop and reiterate
					goto EvaluationLoopReset;
				}

				// Input/Output related handling
				if (ReturnEvent & POLLRDNORM) {

					// Dispatch read command
					SPDLOG_LOGGER_INFO(NetworkLog, "Socket [{:04x}] is requesting to receive a packet",
						SocketDescriptorTable[i].fd);
					auto IoStatus = BuildRequestDispatchAndReturn(IoCompleteRequestRoutine,
						UserContext,
						SocketDescriptorTable[i].fd,
						i ? NwRequestPacket::INCOMING_PACKET : NwRequestPacket::INCOMING_CONNECTION);

					// Check if the list was modified and redo function
					if (IoStatus == NwRequestPacket::STATUS_LIST_MODIFIED) {
						SPDLOG_LOGGER_WARN(NetworkLog, "Client list has been modified, iterators destroyed"); goto EvaluationLoopReset;
					}
					if (IoStatus < 0)
						return SPDLOG_LOGGER_ERROR(NetworkLog, "Callback failed to handle critical request"),
							IoStatus;
				}
				if (ReturnEvent & POLLWRNORM) {

					SPDLOG_LOGGER_INFO(NetworkLog, "Client [{:04x}] is ready to receive data",
						SocketDescriptorTable[i].fd);

					auto IoStatus = BuildRequestDispatchAndReturn(IoCompleteRequestRoutine,
						UserContext,
						SocketDescriptorTable[i].fd,
						NwRequestPacket::OUTGOING_PACKET_COMPLETE);
					if (IoStatus < 0)
						return SPDLOG_LOGGER_ERROR(NetworkLog, "Callback failed to handle critical request"),
							IoStatus;
				}
			}

			return SPDLOG_LOGGER_INFO(NetworkLog, "All incoming requests have been handled successfully"),
				NwRequestPacket::STATUS_REQUEST_COMPLETED;
		}

		NwClientControl& AcceptIncomingConnection( // Accepts an incoming connection and allocates the client,
		                                           // if successful invalidates all references to existing clients
		                                           // This provides default request handling, RequestStatus modified
			NwRequestPacket& NetworkRequest        // The NetworkRequest to apply accept handling on
		) {
			TRACE_FUNTION_PROTO;

			// Accepting incoming connection and allocating controller
			NwClientControl ConnectingClient;
			ConnectingClient.SocketHandle = accept(
				NetworkRequest.RequestingSocket,
				&ConnectingClient.ClientInformation,
				&ConnectingClient.ClientInfoSize);
			if (ConnectingClient.SocketHandle == INVALID_SOCKET) {

				SPDLOG_LOGGER_ERROR(NetworkLog, "accept() failed to connect to with {}",
					WSAGetLastError());
				NetworkRequest.CompleteIoRequest(NwRequestBase::STATUS_SOCKET_ERROR);
			}
			SPDLOG_LOGGER_INFO(NetworkLog, "Accepted client connection [{:04x}]",
				ConnectingClient.SocketHandle);

			// Adding client to local client list and socket descriptor table
			auto& RemoteClient = ConnectedClients.emplace_back(ConnectingClient);
			SocketDescriptorTable.emplace_back(ConnectingClient.SocketHandle, POLLIN);
			SPDLOG_LOGGER_INFO(NetworkLog, "Client on socket [{:04x}] was sucessfully connected",
				ConnectingClient.SocketHandle);
			NetworkRequest.StatusListModified = true;
			return RemoteClient;
		}

		void BroadcastShipSockControlExcept(       // Broadcasts the message details to all connected clients,
		                                           // except for the one passed in SocketAsId
			const ShipSockControl& PackageDetails, // The package to be send, the data behind the struct may be bigger
			      SOCKET           SocketAsId      // The id of the client to be excluded
		) {
			TRACE_FUNTION_PROTO;

			for (auto& ClientNode : ConnectedClients)
				if (ClientNode.GetSocket() != SocketAsId) {
					
					// Send data over client controller
					auto Result = ClientNode.SendShipControlPackageDynamic(PackageDetails);
					if (Result == SOCKET_ERROR)
						throw runtime_error("Bad handling have to implement somehting here at some point, cant be assed to do it now");
				}
		}


		// Helper and utility functions
		SOCKET GetServerSocketHandle() {
			TRACE_FUNTION_PROTO; return LocalServerSocket;
		}
		uint32_t GetNumberOfConnectedClients() {
			TRACE_FUNTION_PROTO; return ConnectedClients.size();
		}
		span<NwClientControl> GetClientList() {
			TRACE_FUNTION_PROTO; return ConnectedClients;
		}
		NwClientControl* GetClientBySocket(
			SOCKET SocketAsId
		) {
			TRACE_FUNTION_PROTO;

			for (auto& Client : ConnectedClients)
				if (Client.SocketHandle == SocketAsId)
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



		SOCKET                  LocalServerSocket = INVALID_SOCKET;
		vector<NwClientControl> ConnectedClients;
		vector<WSAPOLLFD>       SocketDescriptorTable;
	};
}
