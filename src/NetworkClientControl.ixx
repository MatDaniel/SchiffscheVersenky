// This file implements the client side of the NetworkIoControl manager,
// it provides the necessary interfaces to communicate back to the server
// and receive multiplexed nonblocking io to run in parallel with the engine
module;

#define FD_SETSIZE 1
#include "BattleShip.h"
#include <memory>
#include <queue>

export module Network.Client;
export import LayerBase;
using namespace std;



// Client sided networking code
export namespace Network::Client {
	using namespace Network;

	// A control structure passed to the clients network managers callback allocated by the dispatcher,
	// it describes the request and io information associated.
	// The request must be handled, several utility functions are provided for interfacing.
	struct NwRequestPacket 
		: public NwRequestBase {
		
		ShipSockControl* IoControlPacketData;
	};


	class NetworkManager2
		: public MagicInstanceManagerBase<NetworkManager2>,
		  private WsaNetworkBase {
		friend class MagicInstanceManagerBase<NetworkManager2>;
	public:
		typedef void(*MajorFunction)(        // this has to handle the networking requests being both capable of reading and sending requests
			NetworkManager2* NetworkDevice,  // a pointer to the NetWorkIoController responsible of said request packet
			NwRequestPacket& NetworkRequest, // a reference to a network request packet describing the current request
			void*            UserContext     // A pointer to caller defined data thats forwarded to the callback in every call, could be the GameManager class or whatever
			);
		struct PacketQueueEntry {          // A Entry in the packet queue of data to be send later
			unique_ptr<char[]> DataPacket; // The actual data or rest of the data to send
			size_t SizeOfDatapack;         // The length of data to be send stored
		};


		~NetworkManager2() {
			TRACE_FUNTION_PROTO;

			// SPDLOG_LOGGER_INFO(NetworkLog, "Client network manager destroyed, cleaning up sockets");
		}

		
		static constexpr auto READ_MASK = 1;
		static constexpr auto WRITE_MASK = 2;
		static constexpr auto EXCEPT_MASK = 4;
		static constexpr auto CONNECTED_MASK = 8;
		int32_t CheckoutServerSocket() { // Checks out the servers associated socket for updates
			                             // and notifies the caller
			TRACE_FUNTION_PROTO;

			// FD_SET list (in order: read, write, except) and initialize
			fd_set DescriptorTable[3];
			for (auto i = 0; i < 3; ++i) {
				FD_ZERO(&DescriptorTable[i]); FD_SET(GameServer, &DescriptorTable[i]);
			}
			timeval Timeout{};
			auto Result = select(0,
				&DescriptorTable[0],
				&DescriptorTable[1],
				&DescriptorTable[2],
				&Timeout);

			// Evaluate result of select
			auto MergeOutput = 0;
			switch (Result) {
			case SOCKET_ERROR:
				SPDLOG_LOGGER_ERROR(NetworkLog, "Socket query failed with {}",
					WSAGetLastError());
				return MergeOutput |= EXCEPT_MASK;

			case 0:
				SPDLOG_LOGGER_TRACE(NetworkLog, "Socket query, no status information received, pending");
				return NwRequestPacket::STATUS_WORK_PENDING;

			default:
				if (!SocketAttached) {
					if (DescriptorTable[2].fd_count)
						return SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to connect to sever, refused connection"),
							MergeOutput |= EXCEPT_MASK;

					if (DescriptorTable[1].fd_count) {
						freeaddrinfo(ServerInformation);
						SocketAttached = true;
						SPDLOG_LOGGER_INFO(NetworkLog, "Server connected on socket, switched controller to io mode");
						return MergeOutput |= CONNECTED_MASK;
					}
				}

				if (DescriptorTable[2].fd_count)
					return SocketAttached = false,
						SPDLOG_LOGGER_ERROR(NetworkLog, "An error occured on the socket, while probing for socket state"),
						MergeOutput |= EXCEPT_MASK;

				if (DescriptorTable[0].fd_count)
					MergeOutput |= READ_MASK | CONNECTED_MASK;
				if (DescriptorTable[1].fd_count)
					MergeOutput |= WRITE_MASK | CONNECTED_MASK;
			}

			return MergeOutput;
		}

		NwRequestBase::NwRequestStatus
		ExecuteNetworkRequestHandlerWithCallback( // Probes requests and prepares IORP's for asynchronous networking
		                                          // notifies the caller over a callback and provides completion routines
		                                          // returns the last tracked request issue or status complete
			MajorFunction NetworkServiceRoutine,  // a user provided callback responsible for handling/completing requests
			void*         UserContext             // some user provided polymorphic value forwarded to the handler routine
		) {
			TRACE_FUNTION_PROTO;

			// Check if Multiplexer has attached to server
			auto Result = CheckoutServerSocket();
			if (!(Result & CONNECTED_MASK)) // !SocketAttached
				return NwRequestPacket::STATUS_WORK_PENDING;

			// Check if we can and need to push new data to the socket
			if (Result & WRITE_MASK &&
				PacketQueue.size()) {

				// The socket is ready to handle and send data
				// Evil variable shadowing btw.
				auto& InternalPacketEntry = PacketQueue.front();
				auto Result = send(GameServer,
					InternalPacketEntry.DataPacket.get(),
					InternalPacketEntry.SizeOfDatapack, 0);

				switch (Result) {
				case SOCKET_ERROR:
					switch (auto LastError = WSAGetLastError()) {
					case WSAEWOULDBLOCK:
						SPDLOG_LOGGER_WARN(NetworkLog, "Could not send delayed packet");
						break;

					default:
						SPDLOG_LOGGER_ERROR(NetworkLog, "send failed to properly transmit delayed package");
						return NwRequestPacket::STATUS_SOCKET_ERROR;
					}
					break;

				default:
					SPDLOG_LOGGER_INFO(NetworkLog, "Queued package was transmited");
					if (Result < InternalPacketEntry.SizeOfDatapack)
						SPDLOG_LOGGER_WARN(NetworkLog, "The message was not fully send {}<{}",
							Result, InternalPacketEntry.SizeOfDatapack);
					PacketQueue.pop();
				}
			}

			// Check if we can receive data and exit if not
			if (!(Result & READ_MASK))
				return NwRequestPacket::STATUS_REQUEST_COMPLETED;

			// Continue execution normally (when server is attached)
			SPDLOG_LOGGER_TRACE(NetworkLog, "Checking socker for messages to process");
			auto PacketBuffer = make_unique<char[]>(PACKET_BUFFER_SIZE);
			do {
				Result = recv(GameServer,
					PacketBuffer.get(),
					PACKET_BUFFER_SIZE,
					0);

				// recv result control
				switch (Result) {
				case SOCKET_ERROR:

					// recv error handler (this may lead to the desired path of WSAWOULDBLOCK)
					switch (auto SocketErrorCode = WSAGetLastError()) {
					case WSAEWOULDBLOCK:
						Result = 0;
						SPDLOG_LOGGER_TRACE(NetworkLog, "No (more) request to handle");
						break;

					case WSAECONNRESET:
						if (closesocket(GameServer) == SOCKET_ERROR)
							SPDLOG_LOGGER_ERROR(NetworkLog, "Server socket failed to close properly with {}",
								WSAGetLastError());
						SPDLOG_LOGGER_ERROR(NetworkLog, "Connection reset, closed socket and aborting");
						return NwRequestPacket::STATUS_REQUEST_ERROR;

					default:
						SPDLOG_LOGGER_CRITICAL(NetworkLog, "unhandled recv status {}",
							SocketErrorCode);
						return NwRequestPacket::STATUS_REQUEST_ERROR;
					}
					break;

				// graceful server shutdown and disconnect handling
				case 0: {
					NwRequestPacket RequestDispatchPacket{};
					RequestDispatchPacket.IoControlCode = NwRequestPacket::SOCKET_DISCONNECTED;
					RequestDispatchPacket.IoRequestStatus = NwRequestPacket::STATUS_REQUEST_NOT_HANDLED;
					NetworkServiceRoutine(this,
						RequestDispatchPacket,
						UserContext);
					if (RequestDispatchPacket.IoRequestStatus < 0)
						SPDLOG_LOGGER_ERROR(NetworkLog, "Callback did not handle disconnect");
					SPDLOG_LOGGER_INFO(NetworkLog, "Server disconnected from client gracefully");
					return RequestDispatchPacket.IoRequestStatus;
				}

				// Network packet handler dispatch
				default: {
					auto ShipPacket = (ShipSockControl*)PacketBuffer.get();
					SPDLOG_LOGGER_INFO(NetworkLog, "Received shipsock control command package with ioctl: {}",
						ShipPacket->ControlCode);
					
					NwRequestPacket RequestDispatchPacket{};
					RequestDispatchPacket.IoControlCode = NwRequestPacket::INCOMING_PACKET;
					RequestDispatchPacket.IoControlPacketData = ShipPacket;
					RequestDispatchPacket.IoRequestStatus = NwRequestPacket::STATUS_REQUEST_NOT_HANDLED;
					NetworkServiceRoutine(this,
						RequestDispatchPacket,
						UserContext);
					if (RequestDispatchPacket.IoRequestStatus < 0)
						return SPDLOG_LOGGER_ERROR(NetworkLog, "Callback failed to handle ship sock control command"),
							RequestDispatchPacket.IoRequestStatus;
				}
				}
			} while (Result > 0);

			return NwRequestPacket::STATUS_REQUEST_COMPLETED;
		}

		SOCKET GetSocket() const {
			TRACE_FUNTION_PROTO; return GameServer;
		}

		int32_t SendShipControlPackageDynamic(    // Sends a package to the connected server 
			const ShipSockControl& RequestPackage // Package to be send
		) {
			TRACE_FUNTION_PROTO;

			// get size of package dynamically
			// send package

			auto PackageSize = sizeof(RequestPackage); // this has to be done properly, skipping it for now


			auto Result = send(GameServer,
				(char*)&RequestPackage,
				PackageSize, 0);

			switch (Result) {
			case SOCKET_ERROR:
				switch (auto LastError = WSAGetLastError()) {
				case WSAEWOULDBLOCK:
				
					// Queue the request for delayed handling
					PacketQueue.emplace(make_unique<char[]>(PackageSize), PackageSize);
					memcpy(PacketQueue.front().DataPacket.get(),
						&RequestPackage, PackageSize);
					SPDLOG_LOGGER_INFO(NetworkLog, "send() wasn't able to complete immediately, request queued for later handling");
					return 0;

				default:
					SPDLOG_LOGGER_ERROR(NetworkLog, "untreated socket error from send()");
					return SOCKET_ERROR;
				}
				break;

			default:
				SPDLOG_LOGGER_INFO(NetworkLog, "package was transmited");
				if (Result < PackageSize)
					SPDLOG_LOGGER_WARN(NetworkLog, "The message was not fully send {}<{}",
						Result, PackageSize);
				return Result;
			}
		}


	private:
		NetworkManager2(
			const char* ServerName, // aka ipv4/6 address
			const char* PortNumber  // the Port Number the server runs on
		) {
			TRACE_FUNTION_PROTO;

			auto Result = getaddrinfo(ServerName,
				PortNumber,
				&(const addrinfo&)addrinfo{
					.ai_family = AF_INET,
					.ai_socktype = SOCK_STREAM,
					.ai_protocol = IPPROTO_TCP
				},
				&ServerInformation);
			if (Result)
				throw (SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to query server address port information with {}",
					Result),
					runtime_error("Failed to query server address port information"));
			SPDLOG_LOGGER_INFO(NetworkLog, "Found address information for {}:{}",
				ServerName, PortNumber);

			auto ServerInformationIterator = ServerInformation;
			do {
				GameServer = socket(
					ServerInformationIterator->ai_family,
					ServerInformationIterator->ai_socktype,
					ServerInformationIterator->ai_protocol);
			} while (GameServer == INVALID_SOCKET &&
				(ServerInformationIterator = ServerInformationIterator->ai_next));
			if (GameServer == INVALID_SOCKET)
				throw (freeaddrinfo(ServerInformation),
					SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to allocate socket for target server {}",
						WSAGetLastError()),
					runtime_error("Failed to allocate socket for target server"));
			SPDLOG_LOGGER_INFO(NetworkLog, "Allocated socket [{:04x}] for remote",
				GameServer);

			u_long BlockingMode = 1;
			Result = ioctlsocket(
				GameServer,
				FIONBIO,
				&BlockingMode);
			if (Result == SOCKET_ERROR)
				throw (freeaddrinfo(ServerInformation),
					closesocket(GameServer),
					SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to switch socket [{:04x}] to nonblocking mode {}",
						GameServer, WSAGetLastError()),
					runtime_error("Failed to switch server socket to nonblocking mode"));
			SPDLOG_LOGGER_INFO(NetworkLog, "Switched socket [{:04x}] to nonblocking mode",
				GameServer);

			Result = connect(
				GameServer,
				ServerInformation->ai_addr,
				ServerInformation->ai_addrlen);
			if (auto Response = WSAGetLastError(); Result == SOCKET_ERROR)
				if (Response != WSAEWOULDBLOCK)
					throw (freeaddrinfo(ServerInformation),
						closesocket(GameServer),
						SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to queue connection attempt on socket [{:04x}] {}",
							GameServer, WSAGetLastError()),
						runtime_error("Failed to queue nonblocking connection attempt"));
			SPDLOG_LOGGER_INFO(NetworkLog, "Queed connect atempt on socket [{:04x}] in async mode",
				GameServer);
		}

		queue<PacketQueueEntry> PacketQueue;
		SOCKET    GameServer = INVALID_SOCKET;
		bool      SocketAttached = false;
		addrinfo* ServerInformation = nullptr;
	};
}
