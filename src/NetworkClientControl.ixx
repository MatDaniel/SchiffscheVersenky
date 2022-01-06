// This file implements the client side of the NetworkIoControl manager,
// it provides the necessary interfaces to communicate back to the server
// and receive multiplexed nonblocking io to run in parallel with the engine
module;

#define FD_SETSIZE 1
#include "BattleShip.h"
#include <functional>
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

	private:
		NwRequestPacket() = default; // Make the constructor private so only the dispatcher can allocate NRP's
	};




	class NetworkManager2 final
		: public MagicInstanceManagerBase<NetworkManager2>,
		  private WsaNetworkBase {
		friend class MagicInstanceManagerBase<NetworkManager2>;

		// Number of bytes to cache at least in order to parse stream
		static constexpr size_t MinimumSizeRead =
			offsetof(ShipSockControl, SizeOfThisStruct) +
			sizeof(ShipSockControl::SizeOfThisStruct);
		struct PacketQueueEntry {              // A Entry in the packet queue of data to be send later
			unique_ptr<char[]> DataPacket;     // The actual data or rest of the data to send
			size_t             SizeOfDatapack; // The length of data stored to be send
			size_t             WriteOffset;    // The starting point from where send should start reading
		};

	public:
		enum DispatchStatus {                 // Tristate response made by the dispatcher to determin execution flow
			STATUS_FAILED_TO_CONNECT = -2000, // The network controller failed to connect to the remote, actively refused
			STATUS_SOCKET_DISCONNECTED,       // Dispatcher had an internal server error and lead to a shutdown

			STATUS_DISPATCH_OK = 0, // No errors, all requests successfully dispatched and handled
			STATUS_SOCKET_CONNECTED // Signals that a client successfully connected to the server
			                        // it is now in a fully operational state until disconnect
		};
		using MajorFunction = void(           // this has to handle the networking requests being both capable of reading and sending requests
			NetworkManager2* NetworkDevice,   // a pointer to the NetWorkIoController responsible of said request packet
			NwRequestPacket& NetworkRequest,  // a reference to a network request packet describing the current request
			void*            UserContext,     // A pointer to caller defined data thats forwarded to the callback in every call, could be the GameManager class or whatever
			ShipSockControl* RequestPackage); // TODO:
		using MajorFunction_t = add_pointer_t<MajorFunction>;

		~NetworkManager2() {
			TRACE_FUNTION_PROTO;

			// Close socket and cleanup
			auto Result = closesocket(RemoteServer);
			if (Result == SOCKET_ERROR)
				SPDLOG_LOGGER_ERROR(NetworkLog, "closesocket() failed to close the remote socket with {}",
					WSAGetLastError());
			SPDLOG_LOGGER_INFO(NetworkLog, "Client network manager destroyed");
		}

		/// LEGACY: NOT REQUIRED IN THE CLIENT AS THERE IS NO CLIENT MANAGER ONLY A SINGLE CONENCTION
		//          THREFORE NO SCOPE DISPATCH IS REQUIRED, THIS IS LEFT AS A RELIC TO LOOK AT
		//
		// DispatchStatus DispatchCallbackWithNrpScopeEx( // Allows to dispatch a callback with a shell NRP in scope,
		// 											   // which grants access to direct network io functions 
		// 	function<MajorFunction> IoServiceRoutine,  // a user provided callback responsible for handling/completing requests
		// 	void*                   UserContext		   // some user provided polymorphic value forwarded to the handler routine
		// ) {
		// 	TRACE_FUNTION_PROTO;
		// 
		// 	NwRequestPacket ShellRequest{};
		// 	ShellRequest.IoRequestStatus = NwRequestPacket::STATUS_REQUEST_NOT_HANDLED;
		// 	auto RequestStatus = RequestDispatchAndReturn(IoServiceRoutine,
		// 		UserContext,
		// 		ShellRequest);
		// 	if (RequestStatus < 0) {
		// 
		// 		// The shell dispatch callback failed to handle the NRP, this will cause a disconnect of the client
		// 		SPDLOG_LOGGER_ERROR(NetworkLog, "Dispatched shell callback failed to handle NRP, causing disconnect");
		// 		DispatchDisconnectService(IoServiceRoutine,
		// 			UserContext);
		// 		return STATUS_SOCKET_DISCONNECTED;
		// 	}
		// 
		// 	return STATUS_DISPATCH_OK;
		// }


		DispatchStatus
		ExecuteNetworkRequestHandlerWithCallback(     // Probes requests and prepares IORP's for asynchronous networking
		                                              // notifies the caller over a callback and provides completion routines
		                                              // returns the last tracked request issue or status complete
			function<MajorFunction> IoServiceRoutine, // a user provided callback responsible for handling/completing requests
			void*                   UserContext       // some user provided polymorphic value forwarded to the handler routine
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
				SPDLOG_LOGGER_ERROR(NetworkLog, "Socket {} query failed with {}, shuting down",
					RemoteServer,
					WSAGetLastError());
				DispatchDisconnectService(IoServiceRoutine,
					UserContext);
				throw runtime_error("Failed to query socket, fatal cannot happen");
				// return STATUS_SOCKET_DISCONNECTED;

			case 0:
				// Check for connect pending or no operations required
				SPDLOG_LOGGER_TRACE(NetworkLog, "Socket query, no status information received, pending");
				return STATUS_DISPATCH_OK;

			default:
				// Check except fd_set and report oob data / failed connect attempt
				if (DescriptorTable[2].fd_count) {
					
					// Check if we are not connected yet, if so the connection attempt failed
					if (!SocketAttached) {

						// Process cleanup and notify caller of failed connect attempt,
						// Same as with DispatchDisconnectService, this will destroy the current object,
						// therefore invalidate the object and all references/pointers to it
						// DO NOT USE THIS OBJECT AFTER RECEIVING THIS STATUS
						SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to connect to remote {}, refused connection",
							RemoteServer);
						this->ManualReset();
						return STATUS_FAILED_TO_CONNECT;
					}

					// This can be happily ignored cause I dont use OOB data :)
					SPDLOG_LOGGER_DEBUG(NetworkLog, "Out of boud, data is ready for read");
				}
				
				// Check for readability and dispatch appropriate handling
				if (DescriptorTable[0].fd_count) {

					// Read all data available into the buffer by appending it to everything comissioned
					bool DataExceededBufferReadAgain = false;
					do {
						auto Result = recv(RemoteServer,
							DataReceiverStream.get() + ReceiverStreamLength,
							PACKET_BUFFER_SIZE - ReceiverStreamLength, 0);
						SPDLOG_LOGGER_DEBUG(NetworkLog, "recv() in dispatch returned {}:{}",
							Result, WSAGetLastError());
					
						// Handle recv() response and dispatch proper handling
						switch (Result) {
						case SOCKET_ERROR:
							// recv error handler (this may lead to the desired path of WSAWOULDBLOCK)
							switch (auto SocketErrorCode = WSAGetLastError()) {
							case WSAEWOULDBLOCK:
								// Force read loop to stop if we ended up again when no data is available anymore
								DataExceededBufferReadAgain = false;
								SPDLOG_LOGGER_TRACE(NetworkLog, "No (more) request to handle");
								break;

							default:
								SPDLOG_LOGGER_CRITICAL(NetworkLog, "unhandled recv status {}",
									SocketErrorCode);
								DispatchDisconnectService(IoServiceRoutine,
									UserContext);
								return STATUS_SOCKET_DISCONNECTED;
							}
							break;

						case 0:
							// Server disconnected gracefully, dispatch disconnect and shutdown
							SPDLOG_LOGGER_INFO(NetworkLog, "Server disconnected from client");
							DispatchDisconnectService(IoServiceRoutine,
								UserContext);
							return STATUS_SOCKET_DISCONNECTED;

						default: {
							// The start of the stream always represents the start of a package
							auto PackageStreamPointer = (ShipSockControl*)DataReceiverStream.get();
							ReceiverStreamLength += Result;

							// Check if the current buffer has been completely filled and schedule
							// a new recv() iteration to get the possibly stored data
							DataExceededBufferReadAgain = false;
							if (ReceiverStreamLength >= SizeOfReceiverStream) {

								// Schedule recv() stream exceed iteration, this will redo the loop until the recv() buffer
								// has been fully emptied or something else like a disconnect etc occurs
								DataExceededBufferReadAgain = true;
								SPDLOG_LOGGER_INFO(NetworkLog, "The stream was completely filled by recv(), scheduling redo");
							}

							// Iterate through the current stream until we either reach the end or cannot
							// dispatch incomplete packages anymore
							while (ReceiverStreamLength >= MinimumSizeRead &&
								PackageStreamPointer->SizeOfThisStruct <= ReceiverStreamLength) {

								// Dispatch message as we know the current contained buffer content is large enough,
								// to contain at least one ship control packet, therefore it can be directly referenced
								auto NetworkRequest = BuildNetworkRequestPacket(
									NwRequestPacket::INCOMING_PACKET);
								auto IoStatus = RequestDispatchAndReturn(IoServiceRoutine,
									UserContext,
									NetworkRequest,
									PackageStreamPointer);

								// Check for dispatch errors, if any we terminate the session and notify the caller
								if (IoStatus < 0) {

									// Dispatch disconnect service routine and notify caller
									SPDLOG_LOGGER_CRITICAL(NetworkLog, "Callback failed to handle ship sock control command, terminating connection");
									DispatchDisconnectService(IoServiceRoutine,
										UserContext);
									return STATUS_SOCKET_DISCONNECTED;
								}

								// Update stream iterator states
								ReceiverStreamLength -= PackageStreamPointer->SizeOfThisStruct;
								(char*&)PackageStreamPointer += PackageStreamPointer->SizeOfThisStruct;
							}

							// Move the left over data in package stream back to the start of the stream
							// This prepares the left over incomplete package for the next dispatch cycle
							memmove(DataReceiverStream.get(),
								(void*)PackageStreamPointer,
								ReceiverStreamLength);
						}}
					} while (DataExceededBufferReadAgain);					
				}	

				// Check for writeability / connected state, and dispatch queued data from stream if available
				if (DescriptorTable[1].fd_count) {

					// The remote socket is ready to take data again, check if we have anything left to send
					// If the server is not yet attached, the queue wont be able to contain any data,
					// so its ok to prioritize and check this first
					if (OutboundPacketQueue.size()) {

						// The socket is ready to handle and send data, send until it doesnt wanna send anymore
						int32_t Result = 0;
						do {
							// Get first queue entry and send actual packet to remote
							auto& InternalPacketEntry = OutboundPacketQueue.front();
							Result = send(RemoteServer,
								InternalPacketEntry.DataPacket.get() +
									InternalPacketEntry.WriteOffset,
								InternalPacketEntry.SizeOfDatapack,
								0);
							switch (Result) {
							case SOCKET_ERROR:
								// Check why we failed, hopefully WOULDBLOCK if any, anything else leads to a disconnect
								switch (auto LastError = WSAGetLastError()) {
								case WSAEWOULDBLOCK:
									SPDLOG_LOGGER_WARN(NetworkLog, "Could not send delayed packet, will retry next time");
									break;

								default:
									// Dispatch error and disconnect from remote, this is a destructive call,
									// it will destroy this object, and invalidate all references to it
									SPDLOG_LOGGER_ERROR(NetworkLog, "send() failed to properly transmit delayed package");
									DispatchDisconnectService(IoServiceRoutine,
										UserContext);
									return STATUS_SOCKET_DISCONNECTED;
								}
								break;

							default:
								// Calculate new package size and check if package still hasnt fully transmitted
								if (InternalPacketEntry.SizeOfDatapack -= Result) {
									
									// Update write offset, and exit the loop to delay sending further
									InternalPacketEntry.WriteOffset += Result;
									SPDLOG_LOGGER_WARN(NetworkLog, "The message was not fully send yet ({} bytes left)",
										InternalPacketEntry.SizeOfDatapack);
									Result = 0; break;
								}

								// The package was fully send, remove it from the queue and continue
								SPDLOG_LOGGER_INFO(NetworkLog, "Queued package {} with {} bytes was transmited",
									(void*)&InternalPacketEntry,
									((ShipSockControl*)InternalPacketEntry.DataPacket.get())->SizeOfThisStruct);
								OutboundPacketQueue.pop();
							}

						// Check if send() was successful and if so also check if we again have anything to send anymore
						} while (Result > 0
							&& OutboundPacketQueue.size());
					}

					// We were notified about a write ready state, check if we successfully connected
					if (!SocketAttached) {

						// Successfully connected to remote, enable this and notify caller
						SPDLOG_LOGGER_INFO(NetworkLog, "Socket {} succesfully connected to remote",
							RemoteServer);
						SocketAttached = true;
						return STATUS_SOCKET_CONNECTED;
					}
				}
			}
		}

		SOCKET GetSocket() const {
			TRACE_FUNTION_PROTO; return RemoteServer;
		}

		void QueueShipControlPacket(              // Queues a package to be send to the server
			const ShipSockControl& RequestPackage // The ship control package to be queued
		) {
			TRACE_FUNTION_PROTO;

			// Queue the request for delayed handling, it will be dispatched from the dispatcher
			auto& QueueEntry = OutboundPacketQueue.emplace(
				make_unique<char[]>(RequestPackage.SizeOfThisStruct),
				RequestPackage.SizeOfThisStruct, 0);
			memcpy(QueueEntry.DataPacket.get(),
				&RequestPackage,
				RequestPackage.SizeOfThisStruct);
			SPDLOG_LOGGER_INFO(NetworkLog, "Ship control packet with ssclt [{}] was scheduled for send()",
				RequestPackage.ControlCode);
		}

		void DispatchDisconnectService(               // Dispatches a disconnect notification and terminates the connection
			function<MajorFunction> IoServiceRoutine, // The passed routine responsible for handling
			void*                   UserContext       // An associated user context that should be passed along to the routine
		) {
			TRACE_FUNTION_PROTO;

			// Build basic disconnect packet
			NwRequestPacket NetworkRequest{};
			NetworkRequest.IoControlCode = NwRequestPacket::SOCKET_DISCONNECTED;
			NetworkRequest.IoRequestStatus = NwRequestPacket::STATUS_REQUEST_NOT_HANDLED;

			// Dispatch disconnect notification to client
			auto RequestStatus = RequestDispatchAndReturn(IoServiceRoutine,
				UserContext,
				NetworkRequest,
				nullptr);
			if (RequestStatus < 0)
				throw (SPDLOG_LOGGER_CRITICAL(NetworkLog, "YOu fucked up to fuckiung hanle disconnet fucking faggot"),
					runtime_error("Are you even trying anymore? HOW THE FUCK DID WE END UP HERE"));

			// Schedule ourself for automatic deletion,
			// THIS IS A SUICIDE MOVE, DO NOT REPLICATE !
			// Its only possible cause the underlying instance manager
			// dynamically allocates this and manages the object,
			// after this call no members are touched 
			// and all functions up to this point will immediately return.
			SPDLOG_LOGGER_WARN(NetworkLog, "Warning this object {}(NetworkManager Instance) is being deleted",
				(void*)this);
			this->ManualReset();
		}

	private:
		NetworkManager2(
			const char*  ServerName = DefaultServerAddress, // aka ipv4 address (may add ipv6 support)
			const char*  PortNumber = DefaultPortNumber,    // the port number the server runs on
			      size_t SizeOfStream = PACKET_BUFFER_SIZE  // The size of the desired data stream cache
		) 
			: DataReceiverStream(make_unique<char[]>(SizeOfStream)),
			  SizeOfReceiverStream(SizeOfStream) {
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

		NwRequestPacket BuildNetworkRequestPacket(      // Helper function for building NRP's
			NwRequestPacket::NwServiceCtl IoControlCode //
		) {
			NwRequestPacket NetworkRequest{};
			NetworkRequest.IoControlCode = IoControlCode;
			NetworkRequest.IoRequestStatus = NwRequestPacket::STATUS_REQUEST_NOT_HANDLED;
			return NetworkRequest;
		};

		NwRequestPacket::NwRequestStatus
		RequestDispatchAndReturn(                     // Dispatches and catches a network request
			function<MajorFunction> IoServiceRoutine, // The passed routine responsible for handling
			void*                   UserContext,      // The passed user context
			NwRequestPacket&        NetworkRequest,   // A NRP to extend and dispatch
			ShipSockControl*        RequestPackage    // An optional pointer to a control package
		) {
			TRACE_FUNTION_PROTO;

			// Dispatch request and checkout
			SPDLOG_LOGGER_INFO(NetworkLog, "Dispatching NRP with NWCTL [{}]",
				NetworkRequest.IoControlCode);
			try {
				IoServiceRoutine(this,
					NetworkRequest,
					UserContext,
					RequestPackage);

				// If execution resumes here the packet was not completed, server has to terminate,
				// as the state of several components could now be indeterminate 
				constexpr auto ErrorMessage = "The network request returned, it was not completed";
				SPDLOG_LOGGER_CRITICAL(NetworkLog, ErrorMessage);
				throw runtime_error(ErrorMessage);
			}
			catch (const NwRequestPacket* CompletedPacket) {

				// Check if allocated packet is the same as returned
				if (CompletedPacket == &NetworkRequest)
					return CompletedPacket->IoRequestStatus;

				// If we get there the dispatched NRP is not the one collected, this should be impossible
				SPDLOG_LOGGER_ERROR(NetworkLog, "The completed packet {} doesnt match the send package signature",
					(void*)CompletedPacket);
				throw runtime_error("The completed package doesnt match the dispatched packages signature");
			}
		}



		// Receiver/sender stream context
		queue<PacketQueueEntry> OutboundPacketQueue;
		unique_ptr<char[]>      DataReceiverStream;
		const size_t            SizeOfReceiverStream;
		      size_t            ReceiverStreamLength = 0;

		// Network manager internal context
		SOCKET    RemoteServer = INVALID_SOCKET;
		bool      SocketAttached = false;
		addrinfo* ServerInformation = nullptr;
	};
}
