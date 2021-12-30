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
	class NwRequestPacket 
		: public NwRequestBase {
		friend class NetworkManager2;
	public:
		[[noreturn]] NwRequestStatus CompleteIoRequest(
			NwRequestStatus RequestStatus
		) override {
			TRACE_FUNTION_PROTO;

			// Complete this request and raise it to the manager
			NwRequestBase::CompleteIoRequest(RequestStatus);
			throw this;
		}

		// Optional contained package, dispatched with INCOMING_PACKET
		ShipSockControl* IoControlPacketData; 

	private:
		NwRequestPacket() = default; // Make the constructor private so only the dispatcher can allocate NRP's
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
			TRACE_FUNTION_PROTO; SPDLOG_LOGGER_INFO(NetworkLog, "Client network manager destroyed, cleaning up sockets");
		}



		NwRequestPacket::NwRequestStatus
		RequestDispatchAndReturn(              // Dispatches and catches a network request
			MajorFunction    IoServiceRoutine, // The passed routine responsible for handling
			void*            UserContext,      // The passed user context
			NwRequestPacket& NetworkRequest    // A NRP to extend and dispatch
		) {
			TRACE_FUNTION_PROTO;

			// Dispatch request and checkout
			try {
				IoServiceRoutine(this,
					NetworkRequest,
					UserContext);

				// If execution resumes here the packet was not completed, server has to terminate,
				// as the state of several components could now be indeterminate 
				constexpr auto ErrorMessage = "The network request returned, it was not completed";
				SPDLOG_LOGGER_CRITICAL(NetworkLog, ErrorMessage);
				throw runtime_error(ErrorMessage);
			}
			catch (const NwRequestPacket* CompletedPacket) {

				// Check if allocated packet is the same as returned
				if (CompletedPacket != &NetworkRequest)
					return SPDLOG_LOGGER_ERROR(NetworkLog, "The completed packet {} doesnt match the send package signature",
						(void*)CompletedPacket),
					NwRequestPacket::STATUS_REQUEST_ERROR;
				return CompletedPacket->IoRequestStatus;
			}
		}

		NwRequestPacket::NwRequestStatus
		ExecuteNetworkRequestHandlerWithCallback( // Probes requests and prepares IORP's for asynchronous networking
		                                          // notifies the caller over a callback and provides completion routines
		                                          // returns the last tracked request issue or status complete
			MajorFunction NetworkServiceRoutine,  // a user provided callback responsible for handling/completing requests
			void*         UserContext             // some user provided polymorphic value forwarded to the handler routine
		) {
			TRACE_FUNTION_PROTO;

			// FD_SET list (in order: read, write, except) and initialize
			fd_set DescriptorTable[3];
			for (auto i = 0; i < 3; ++i) {
				FD_ZERO(&DescriptorTable[i]); FD_SET(RemoteServer, &DescriptorTable[i]);
			}
			timeval Timeout{};
			auto Result = select(0,
				&DescriptorTable[0],
				&DescriptorTable[1],
				&DescriptorTable[2],
				&Timeout);

			// Evaluate result of select
			switch (Result) {
			case SOCKET_ERROR:
				SPDLOG_LOGGER_ERROR(NetworkLog, "Socket query failed with {}",
					WSAGetLastError());
				return NwRequestPacket::STATUS_SOCKET_ERROR;

			case 0:
				// Check for connect pending or no operations required
				SPDLOG_LOGGER_TRACE(NetworkLog, "Socket query, no status information received, pending");
				return NwRequestPacket::STATUS_WORK_PENDING;

			default:
				// Check except fd_set and report oob data / failed connect attempt
				if (DescriptorTable[2].fd_count) {
					if (!SocketAttached)
						return SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to connect to sever {}, refused connection",
							RemoteServer),
							NwRequestPacket::STATUS_FAILED_TO_CONNECT;

					// This can be happily ignored cause I dont use OOB data :)
					SPDLOG_LOGGER_DEBUG(NetworkLog, "Out of boud, data is ready for read");
				}


				//
				// Now we take care of the main processing
				//


				auto BuildNetworkRequestPacket = [this](         // Helper function for building NRP's
					NwRequestPacket::NwServiceCtl IoControlCode, //
					ShipSockControl*              InternalPacket //
					) -> NwRequestPacket {
						NwRequestPacket NetworkRequest{};
						NetworkRequest.IoControlCode = IoControlCode;
						NetworkRequest.IoRequestStatus = NwRequestPacket::STATUS_REQUEST_NOT_HANDLED;
						NetworkRequest.IoControlPacketData = InternalPacket;
						return NetworkRequest;
				};

				// Check for writeability / connected state
				if (DescriptorTable[1].fd_count) {
					
					// Check if multiplexer is attached
					if (!SocketAttached) {

						// This scope handles successful connects
						SPDLOG_LOGGER_INFO(NetworkLog, "Socket {} succesfully connected to remote",
							RemoteServer);
						SocketAttached = true;

						// we do not need to notify the engine yet, that will happing implicitly,
						// as the server sends the first 2 mandatory data packages
						return NwRequestPacket::STATUS_NO_INPUT_OUTPUT;
					}

					// The remote socket is ready to take data againg, check if we have anything left to send
					if (PacketQueue.size()) {

						// The socket is ready to handle and send data
						// Evil variable shadowing btw.
						auto& InternalPacketEntry = PacketQueue.front();
						auto Result = send(RemoteServer,
							InternalPacketEntry.DataPacket.get(),
							InternalPacketEntry.SizeOfDatapack, 0);

						switch (Result) {
						case SOCKET_ERROR:
							switch (auto LastError = WSAGetLastError()) {
							case WSAEWOULDBLOCK:
								SPDLOG_LOGGER_WARN(NetworkLog, "Could not send delayed packet, will retry next time");
								break;

							default:
								SPDLOG_LOGGER_ERROR(NetworkLog, "send() failed to properly transmit delayed package");
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
				}

				// Check for readability and dispatch appropriate handling
				if (DescriptorTable[0].fd_count) {
					
					// Allocate packet buffer and loop recv() until all packets were read and dispatched
					SPDLOG_LOGGER_TRACE(NetworkLog, "Checking socket {} for messages to process",
						RemoteServer);
					auto PacketBuffer = make_unique<char[]>(PACKET_BUFFER_SIZE);
					for (;;) {
						auto Result = recv(RemoteServer,
							PacketBuffer.get(),
							PACKET_BUFFER_SIZE, 0);

						// recv result control
						switch (Result) {
						case SOCKET_ERROR:

							// recv error handler (this may lead to the desired path of WSAWOULDBLOCK)
							switch (auto SocketErrorCode = WSAGetLastError()) {
							case WSAEWOULDBLOCK:
								SPDLOG_LOGGER_TRACE(NetworkLog, "No (more) request to handle");
								return NwRequestPacket::STATUS_NO_INPUT_OUTPUT;
							
							case WSAECONNRESET:
								SPDLOG_LOGGER_ERROR(NetworkLog, "Connection reset, closed socket {}", 
									RemoteServer);
								closesocket(RemoteServer);
								return NwRequestPacket::STATUS_SOCKET_ERROR;

							default:
								SPDLOG_LOGGER_CRITICAL(NetworkLog, "unhandled recv status {}",
									SocketErrorCode);
								return NwRequestPacket::STATUS_REQUEST_ERROR;
							}
							break;

						// graceful server shutdown and disconnect handling
						case 0: {
							
							// Dispatch disconnect notification
							auto NetworkRequest = BuildNetworkRequestPacket(
								NwRequestPacket::SOCKET_DISCONNECTED, nullptr);
							auto IoStatus = RequestDispatchAndReturn(NetworkServiceRoutine,
								UserContext,
								NetworkRequest);
							if (IoStatus < 0)
								SPDLOG_LOGGER_ERROR(NetworkLog, "Callback did not handle disconnect"),
							SPDLOG_LOGGER_INFO(NetworkLog, "Server disconnected from client");
							return IoStatus;
						}

						// Network packet handler dispatch
						default: {
							
							// Get internal packet data and dispatch to callback
							auto ShipPacket = (ShipSockControl*)PacketBuffer.get();
							SPDLOG_LOGGER_INFO(NetworkLog, "Received shipsock control command package with ioctl: {}",
								ShipPacket->ControlCode);

							auto NetworkRequest = BuildNetworkRequestPacket(
								NwRequestPacket::SOCKET_DISCONNECTED, ShipPacket);
							auto IoStatus = RequestDispatchAndReturn(NetworkServiceRoutine,
								UserContext,
								NetworkRequest);
							if (IoStatus < 0)
								return SPDLOG_LOGGER_ERROR(NetworkLog, "Callback failed to handle ship sock control command"),
									IoStatus;
						}
						}
					} 
				}	
			}
		}

		SOCKET GetSocket() const {
			TRACE_FUNTION_PROTO; return RemoteServer;
		}

		int32_t SendShipControlPackageDynamic(    // Sends a package to the connected server 
			const ShipSockControl& RequestPackage // Package to be send
		) {
			TRACE_FUNTION_PROTO;

			// get size of package dynamically
			// send package

			auto PackageSize = sizeof(RequestPackage); // this has to be done properly, skipping it for now


			auto Result = send(RemoteServer,
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
				RemoteServer = socket(
					ServerInformationIterator->ai_family,
					ServerInformationIterator->ai_socktype,
					ServerInformationIterator->ai_protocol);
			} while (RemoteServer == INVALID_SOCKET &&
				(ServerInformationIterator = ServerInformationIterator->ai_next));
			if (RemoteServer == INVALID_SOCKET)
				throw (freeaddrinfo(ServerInformation),
					SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to allocate socket for target server {}",
						WSAGetLastError()),
					runtime_error("Failed to allocate socket for target server"));
			SPDLOG_LOGGER_INFO(NetworkLog, "Allocated socket {} for remote",
				RemoteServer);

			u_long BlockingMode = 1;
			Result = ioctlsocket(
				RemoteServer,
				FIONBIO,
				&BlockingMode);
			if (Result == SOCKET_ERROR)
				throw (freeaddrinfo(ServerInformation),
					closesocket(RemoteServer),
					SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to switch socket {} to nonblocking mode {}",
						RemoteServer, WSAGetLastError()),
					runtime_error("Failed to switch server socket to nonblocking mode"));
			SPDLOG_LOGGER_INFO(NetworkLog, "Switched socket {} to nonblocking mode",
				RemoteServer);

			Result = connect(
				RemoteServer,
				ServerInformation->ai_addr,
				ServerInformation->ai_addrlen);
			if (auto Response = WSAGetLastError(); Result == SOCKET_ERROR)
				if (Response != WSAEWOULDBLOCK)
					throw (freeaddrinfo(ServerInformation),
						closesocket(RemoteServer),
						SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to queue connection attempt on socket {} {}",
							RemoteServer, WSAGetLastError()),
						runtime_error("Failed to queue nonblocking connection attempt"));
			SPDLOG_LOGGER_INFO(NetworkLog, "Queed connect atempt on socket {} in async mode",
				RemoteServer);
		}

		queue<PacketQueueEntry> PacketQueue;
		SOCKET    RemoteServer = INVALID_SOCKET;
		bool      SocketAttached = false;
		addrinfo* ServerInformation = nullptr;
	};
}
