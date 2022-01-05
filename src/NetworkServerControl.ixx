// This module implements the network io manager (pseudo singleton),
// it provides an io bride to several clients, 
// and manages them in a multiplexed independent asynchronous manner
module;

#include "BattleShip.h"
#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <span>

export module Network.Server;
export import LayerBase;
using namespace std;



// Server sided network management code
export namespace Network::Server {
	using namespace Network;

	// Network-Request-Packet (NRP), a context dispatched with every single operation
	class NwClientControl;
	class NwRequestPacket
		: public NwRequestBase {
		friend class NetworkManager2;
	public:
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
			}

			// We now raise the package in order to immediately return to the manager
			// The package is thrown as a pointer to the original passed request
			throw this;
		}

		SOCKET           RequestingSocket; // the socket which was responsible for the incoming request
		NwClientControl* OptionalClient;   // an optional pointer set by the NetworkManager to the client responsible for the request,
			                               // on accept this will contain the information about the connecting client the callback can decide to accept the connection

	private:
		NwRequestPacket() = default; // Prevent others from being able to build this request

		bool StatusListModified = false; // Internal flag relevant to the socket descriptor list, as it logs modification and restarts the evaluation loop
	};
	


	// The primary, per client interface, used to control individual connections
	// This object is managed by the NetworkManager2 and can only be allocated from there
	class NwClientControl {
		friend class NetworkManager2;
	public:
		class PrivateTag {
			friend NetworkManager2;
		public:
			PrivateTag(const PrivateTag&) = default;
			PrivateTag& operator=(const PrivateTag&) = default;
		private:
			PrivateTag() = default;
		};

		NwClientControl(                             // Constructs a network client controller
			PrivateTag PassKey,                      // Prevents construction of this object from
			                                         // outside of the specified class
			size_t SizeOfStream = PACKET_BUFFER_SIZE // The size of the desired data stream cache
		)
			: DataReceiverStream(make_unique<char[]>(SizeOfStream)) {
			TRACE_FUNTION_PROTO;
		}
		~NwClientControl() {
			TRACE_FUNTION_PROTO; SPDLOG_LOGGER_INFO(NetworkLog, "Client controller has been closed");
		}
		bool operator==(
			const NwClientControl* Rhs
		) const {
			TRACE_FUNTION_PROTO; return this == Rhs;
		}

		int32_t DirectSendPackageOutboundOrComplete( // Directly sends out the package to the remote in blocking mode
		                                             // If the package fails to be send completes the NRP with an error
			      NwRequestPacket& NetworkRequest,   // A reference to the NRP of the current network dispatch frame
			const ShipSockControl& RequestPackage    // The Package to be send, the data behind the struct may actually be bigger
		) {
			TRACE_FUNTION_PROTO;
			
			// Try to send the pending package to the registered remote
			auto RemoteSocket = GetSocket();
			auto Result = send(RemoteSocket,
				(char*)&RequestPackage,
				RequestPackage.SizeOfThisStruct, 0);
			switch (Result) {
			case SOCKET_ERROR:
				// Log error and dispatch, this will cause the manager to disconnect this client
				SPDLOG_LOGGER_ERROR(NetworkLog, "send() failed to transmit package ({} bytes)",
					RequestPackage.SizeOfThisStruct);
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_SOCKET_ERROR);
		
			default:
				if (Result < RequestPackage.SizeOfThisStruct) {

					// send failed to fully send the package (possibly timeout'ed ?)
					// this state shall be impossible but will rather cause a disconnect
					// than causing a runtime error to be raised
					SPDLOG_LOGGER_ERROR(NetworkLog, "send() did not fully transmit package in blocking mode ({} of {} send)",
						Result, RequestPackage.SizeOfThisStruct);
					NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_SOCKET_ERROR);
				}

				// The package was fully send, remove it from the queue and continue
				SPDLOG_LOGGER_INFO(NetworkLog, "Package of {} with {} bytes was transmited",
					RequestPackage.ControlCode,
					RequestPackage.SizeOfThisStruct);
				return Result;
			}
		}
		
		void RaiseStatusMessageOrComplete(                      // Raises a status on the specified remote of specified type
			NwRequestPacket&                   NetworkRequest,  // A reference to an NRP to raise the status on
			ShipSockControl::ShipControlStatus StatusMessage,   // The status to raise on the remote
			size_t                             UniqueIdentifier // The id of an incoming request requiering handling
		) {
			TRACE_FUNTION_PROTO;

			DirectSendPackageOutboundOrComplete(
				NetworkRequest,
				(const ShipSockControl&)ShipSockControl {
					.SpecialRequestIdentifier = UniqueIdentifier,
					.ControlCode = ShipSockControl::RAISE_STATUS_MESSAGE,
					.ShipControlRaisedStatus = StatusMessage
			});
			SPDLOG_LOGGER_INFO(NetworkLog, "Raised status {} on socket {}",
				StatusMessage,
				GetSocket());
		}

		SOCKET GetSocket() const;

	private:
		// Receiver stream context (no outbound stream), client handle
		unique_ptr<char[]> DataReceiverStream;
		      size_t       ReceiverStreamLength = 0;
	};

	// Primary server component responsible for handling inbound/outbound packages,
	// dispatching 
	class NetworkManager2 final
		: public MagicInstanceManagerBase<NetworkManager2>,
		  private WsaNetworkBase {
		friend class MagicInstanceManagerBase<NetworkManager2>;

		// Number of bytes to cache at least in order to parse stream
		static constexpr size_t MinimumSizeRead =
			offsetof(ShipSockControl, SizeOfThisStruct) +
			sizeof(ShipSockControl::SizeOfThisStruct);
		using NRPBuilder = NwRequestPacket(              // Helper for building NRP's that gets passed to the dispatcher
			NwRequestPacket::NwServiceCtl IoControlCode, // 
			SOCKET                        SocketAsId);   //

	public:
		enum DispatchStatus {               // Tristate response made by the dispatcher to determin execution flow
			STATUS_SERVER_SHUTDOWN = -2000, // Dispatcher had an internal server error and lead to a shutdown
			STATUS_DISPATCH_OK = 0,         // No errors, all requests successfully dispatched and handled
			STATUS_SOCKET_DISCONNECTED,     // (Mostly internal), signals that a client was disconnected
											// and that a redispatch was required
			STATUS_SOCKET_ADDED             // (Mostly internal), signals that a client was added
											// and that a redispatch was required
		};
		using MajorFunction = void(             // This has to handle the networking requests being both capable of reading and sending requests
			NetworkManager2* NetworkDevice,     // A pointer to the NetWorkIoController responsible of said request packet
			NwRequestPacket& NetworkRequest,    // A reference to a network request packet describing the current request
			void*            UserContext,       // A pointer to caller defined data thats forwarded to the callback in every call, could be the GameManager class or whatever
			ShipSockControl* IoControlPackage); // Optional pointer to a ship sock control package associated to the dispatched NRP
		using MajorFunction_t = add_pointer_t<MajorFunction>;


		~NetworkManager2() {
			TRACE_FUNTION_PROTO; 
			
			// This has to close all ports etc and completely shut down the server (TO BE IMPLEMENTED)
			SPDLOG_LOGGER_INFO(NetworkLog, "Network manager destroyed, cleaning up sockets");
			
			// for (auto NextIterator, Iterator = ConnectedClients.begin();
			// 	Iterator != ConnectedClients.end();
			// 	Iterator = NextIterator) {
			// 
			// 	DispatchDisconnectService()
			// }

		}

		DispatchStatus
		PollNetworkConnectionsAndDispatchCallbacks(   // Starts internally handling accept requests passively,
													  // notifies caller over callbacks and handles io requests
													  // @Sherlock will have to call this with his game manager ready,
													  // the callback is responsible for handling incoming io and connections.
			function<MajorFunction> IoServiceRoutine, // a function pointer to a callback that handles socket operations
			void*                   UserContext,      // a user supplied buffer containing arbitrary data forwarded to the callback
			int32_t                 Timeout = -1      // specifies the timeout count the functions waits on poll till it returns
		) {
			TRACE_FUNTION_PROTO;

			auto BuildNetworkRequestPacket = [this](         // Helper function for building NRP's
				NwRequestPacket::NwServiceCtl IoControlCode, //
				SOCKET                        SocketAsId     //
				) -> NwRequestPacket {
					NwRequestPacket NetworkRequest{};
					NetworkRequest.IoControlCode = IoControlCode;
					NetworkRequest.RequestingSocket = SocketAsId;
					NetworkRequest.OptionalClient = GetClientBySocket(SocketAsId);
					NetworkRequest.IoRequestStatus = NwRequestPacket::STATUS_REQUEST_NOT_HANDLED;
					return NetworkRequest;
			};

			// Poll the socket descriptor table (SDT) and block until event
			auto Result = WSAPoll(
				SocketDescriptorTable.data(),
				SocketDescriptorTable.size(),
				Timeout);
			if (Result <= 0) {

				// Failed to query socket descriptor list, this is fatal
				SPDLOG_LOGGER_CRITICAL(NetworkLog, "poll() failed to query socket states with: {}",
					WSAGetLastError());

				// Again a suicide move just like within the client...
				// this will destroy the network manager and clean up
				this->ManualReset();
				return STATUS_SERVER_SHUTDOWN;
			}
				
			// Dispatch handling until either a new client was inserted or an error occurs 
			auto ConvertDispatchStatusToBool = [](
				DispatchStatus Status
				) {
					return Status == STATUS_SOCKET_ADDED
						|| Status == STATUS_SOCKET_DISCONNECTED;
			};
			DispatchStatus LastDispatchStatus{};
			do {
				LastDispatchStatus = TableDispatchCallbackInternal(
					BuildNetworkRequestPacket,
					IoServiceRoutine,
					UserContext,
					Timeout);
			} while (ConvertDispatchStatusToBool(LastDispatchStatus));

			if (LastDispatchStatus < 0)
				throw runtime_error("Impossible error, to be implemented");
			return STATUS_DISPATCH_OK;
		}


		NwClientControl& AllocateConnectionAndAccept( // Accepts an incoming connection and allocates the client,
		                                              // if successful invalidates all references to existing clients
		                                              // This provides default request handling, RequestStatus modified
			NwRequestPacket& NetworkRequest           // The NetworkRequest to apply accept handling on
		) {
			TRACE_FUNTION_PROTO;

			// Accepting incoming connection and allocating controller
			auto RemoteSocket = accept(
				NetworkRequest.RequestingSocket,
				nullptr, nullptr);
			if (RemoteSocket == INVALID_SOCKET) {

				SPDLOG_LOGGER_ERROR(NetworkLog, "accept() failed to connect with {}",
					WSAGetLastError());
				NetworkRequest.CompleteIoRequest(NwRequestBase::STATUS_FAILED_TO_CONNECT);
			}
			SPDLOG_LOGGER_INFO(NetworkLog, "Accepted client connection {}",
				RemoteSocket);

			// Adding client to local client list and socket descriptor table
			auto [RemoteClient, Inserted] = ConnectedClients.try_emplace(RemoteSocket,
				NwClientControl::PrivateTag{}, SizeOfReceiverStream);
			if (!Inserted) {
			
				SPDLOG_LOGGER_ERROR(NetworkLog, "STL failed to insert client into map");
				closesocket(RemoteSocket);
				NetworkRequest.CompleteIoRequest(NwRequestPacket::STATUS_FAILED_TO_CONNECT);
			}
			SocketDescriptorTable.emplace_back(RemoteSocket, 
				POLLIN /* | POLLOUT */);
			
			SPDLOG_LOGGER_INFO(NetworkLog, "Client on socket {} was sucessfully connected",
				RemoteSocket);
			NetworkRequest.StatusListModified = true;
			return RemoteClient->second;
		}

		void BroadcastShipSockControlExcept(       // Broadcasts the message details to all connected clients,
		                                           // except for the one passed in SocketAsId
			      NwRequestPacket& NetworkRequest, // A NRP forwarded to dispatch all messages on
			      SOCKET           SocketAsId,     // The id of the client to be excluded
			const ShipSockControl& PackageDetails  // The package to be send, the data behind the struct may be bigger
		) {
			TRACE_FUNTION_PROTO;

			// Broadcast message by sending data over all client controllers,
			// maybe not the most efficient but easy to implement
			for (auto& [RemoteSocket, ClientNode] : ConnectedClients)
				if (RemoteSocket != SocketAsId)
					ClientNode.DirectSendPackageOutboundOrComplete(NetworkRequest,
						PackageDetails);
					
		}


		// Helper and utility functions
		uint32_t GetNumberOfConnectedClients() {
			TRACE_FUNTION_PROTO; return ConnectedClients.size();
		}
		NwClientControl* GetClientBySocket(
			SOCKET SocketAsId
		) {
			TRACE_FUNTION_PROTO;

			auto Iterator = find_if(ConnectedClients.begin(),
				ConnectedClients.end(),
				[SocketAsId](
					decltype(ConnectedClients)::const_reference Entry
					) {
						return Entry.first == SocketAsId;
				});
			return Iterator != ConnectedClients.end() 
				? &Iterator->second : nullptr;
		}
		enum GetterOperationType {
			INVALID_OPERATION_TYPE = 0,
			GET_LOCAL_SOCKET,
			GET_SOCKET_OF_CLIENT,
		};
		SOCKET GetSocketByOperation(
			GetterOperationType OperationId,
			void*               OperationContext = nullptr
		) {
			TRACE_FUNTION_PROTO;
			switch (OperationId) {

			// Get the servers listening socket, its always the first entry stored in the SDT
			case GET_LOCAL_SOCKET:
				return SocketDescriptorTable.at(0).fd;

			case GET_SOCKET_OF_CLIENT: {
				auto ClientController = (NwClientControl*)OperationContext;

				auto Iterator = find_if(ConnectedClients.begin(),
					ConnectedClients.end(),
					[ClientController](
						decltype(ConnectedClients)::const_reference Entry
						) -> bool {
							return &Entry.second == ClientController;
					});

				return Iterator->first;
			}

			default:
				SPDLOG_LOGGER_ERROR(NetworkLog, "Invalid operation {} specified",
					OperationId);
				return INVALID_SOCKET;
			}
		}


	private:
		NetworkManager2(
			const char*  PortNumber = DefaultPortNumber,    // the port number the server runs on
			      size_t SizeOfStream = PACKET_BUFFER_SIZE  // The size of the desired data stream cache
		)
			: SizeOfReceiverStream(SizeOfStream) {
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
			SOCKET ThisServerSocket = INVALID_SOCKET;
			do {
				ThisServerSocket = socket(
					ServerInformationIterator->ai_family,
					ServerInformationIterator->ai_socktype,
					ServerInformationIterator->ai_protocol);
			} while (ThisServerSocket == INVALID_SOCKET &&
				(ServerInformationIterator = ServerInformationIterator->ai_next));
			if (ThisServerSocket == INVALID_SOCKET)
				throw (freeaddrinfo(ServerInformation),
					SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to create io handler socket for connect with {}",
						WSAGetLastError()),
					runtime_error("Failed to create io handler socket for connect"));
			SPDLOG_LOGGER_INFO(NetworkLog, "Created socket {} for network manager",
				ThisServerSocket);

			Result = ::bind(
				ThisServerSocket,
				ServerInformationIterator->ai_addr,
				ServerInformationIterator->ai_addrlen);
			freeaddrinfo(ServerInformation);
			ServerInformation = nullptr;
			if (Result == SOCKET_ERROR)
				throw (freeaddrinfo(ServerInformation),
					closesocket(ThisServerSocket),
					SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to bind port {} on socket {} with {}",
						PortNumber, ThisServerSocket, WSAGetLastError()),
					runtime_error("Failed to bind io socket"));
			SPDLOG_LOGGER_INFO(NetworkLog, "Bound port {} to socket {}",
				PortNumber, ThisServerSocket);

			Result = listen(
				ThisServerSocket,
				SOMAXCONN);
			if (Result == SOCKET_ERROR)
				throw (freeaddrinfo(ServerInformation),
					closesocket(ThisServerSocket),
					SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to switch socket {} to listening mode with {}",
						ThisServerSocket, WSAGetLastError()),
					runtime_error("Failed to switch io socket to listening mode"));

			SocketDescriptorTable.emplace_back(ThisServerSocket,
				POLLIN);
			SPDLOG_LOGGER_INFO(NetworkLog, "Created network manager object successfully on socket [{:04x}:{}]",
				ThisServerSocket, PortNumber);
		}

		DispatchStatus TableDispatchCallbackInternal( // The internal version of the dispatcher that actually does 90% of the work
			function<NRPBuilder>    INrpBuilder,      // A swapable function for building NRP's for instrumentation
			function<MajorFunction> IoServiceRoutine, // a function pointer to a callback that handles socket operations
			void*                   UserContext,      // a user supplied buffer containing arbitrary data forwarded to the callback
			int32_t                 Timeout           // specifies the timeout count the functions waits on poll till it returns
		) {
			TRACE_FUNTION_PROTO;

			// Walk over the entire socket descriptor table (SDT) with tis current state
			// The loop may be able to skip several descriptors as they possibly
			// have already been handled in a previous dispatch aborted due to a
			// descriptor list modification that invalidated references and iterators
			for (auto& SocketDescriptor : SocketDescriptorTable) {

				// Skip current round if there are no return events listed or recapture helper
				if (!SocketDescriptor.revents)
					continue;
				auto DispactchedIsServer = [this, &SocketDescriptor]() -> bool {
					TRACE_FUNTION_PROTO; return SocketDescriptor.fd
						== GetSocketByOperation(GET_LOCAL_SOCKET);
				};

				// Query for soft and hard disconnects + general faults
				// NOTE: Due to a bug in poll pre version MN2004 these may not properly execute,
				//       therefore appropriate backup handling is implemented in later parts
				if (SocketDescriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) {

					// Check if response is the server socket, in this case the server has to be 
					if (DispactchedIsServer()) {

						// Error on server listener occurred, the server has to be shut down
						// This is a destructive action and invalidate the server,
						// this object may no longer be used otherwise behavior is undefined
						SPDLOG_LOGGER_CRITICAL(NetworkLog, "Internal server error, the server socket returned an error");
						this->ManualReset();
						return STATUS_SERVER_SHUTDOWN;
					}

					// Notify endpoint of disconnect and redispatch through caller,
					// also filter out corresponding return flags, as a redispatch
					// does not repoll the socket descriptor list (SDT)
					DispatchDisconnectService(IoServiceRoutine,
						UserContext,
						SocketDescriptor.fd);
					return STATUS_SOCKET_DISCONNECTED;
				}

				// Check if we have input data ready to be read and dispatched,
				// or if we got an incoming connection attempt singaled
				if (SocketDescriptor.revents & POLLRDNORM) {

					// Check if the dispatch is made from the local server socket,
					// in which case we have to deal with a remote connection attempt,
					// do defines a breakout scope here used for skipping
					do {
						if (DispactchedIsServer()) {

							// Dispatch incoming connection and let callback handle the request
							// NOTE: The return event has to be masked here, as after the dispatch
							//       all references/pointers into the SDT have been invalidated
							SPDLOG_LOGGER_INFO(NetworkLog, "Server {} was issued with an remote connect() attempt",
								SocketDescriptor.fd);
							auto NetworkRequest = INrpBuilder(NwRequestPacket::INCOMING_CONNECTION,
								SocketDescriptor.fd);
							SocketDescriptor.revents &= ~POLLRDNORM;
							auto Result = DispatchRequestAndReturn(IoServiceRoutine,
								UserContext,
								NetworkRequest,
								nullptr);
							if (Result < 0) {

								// A faulting callback will cause server termination
								SPDLOG_LOGGER_ERROR(NetworkLog, "Callback failed to properly handle inbound connection request");
								this->ManualReset();
								return STATUS_SERVER_SHUTDOWN;
							}

							// Check if SDT was modified and initiate redispatch, otherwise breakout
							if (Result == NwRequestPacket::STATUS_LIST_MODIFIED)
								return STATUS_SOCKET_ADDED;
							break;
						}

						// Read all data available into the buffer by appending it to the stream
						auto ClientNode = GetClientBySocket(SocketDescriptor.fd);
						bool DataExceededBufferReadAgain = false;
						do {

							// Check if recv is to be redispatched as the buffer limit was exceeded previously
							if (DataExceededBufferReadAgain) {

								// Get the count of bytes ready to be read from the recv() buffer
								u_long DataReadyRead = 0;
								auto Result = ioctlsocket(SocketDescriptor.fd,
									FIONREAD,
									&DataReadyRead);
								if (Result == SOCKET_ERROR) {

									// An error occurred during query of the data ready for read,
									// the faulting socket will be disconnected and callbacks will be notified
									SPDLOG_LOGGER_ERROR(NetworkLog, "ioctlsocket() failed to query FIONREAD on socket {} with {}",
										SocketDescriptor.fd, WSAGetLastError());
									DispatchDisconnectService(IoServiceRoutine,
										UserContext,
										SocketDescriptor.fd);
									return STATUS_SOCKET_DISCONNECTED;
								}

								// Check if there is no more data and break out of the recv() loop,
								// as this would cause recv() to block and stall the server
								if (!DataReadyRead)
									break;
							}

							// Read as much data from the buffer into the stream
							auto Result = recv(SocketDescriptor.fd,
								ClientNode->DataReceiverStream.get() + ClientNode->ReceiverStreamLength,
								PACKET_BUFFER_SIZE - ClientNode->ReceiverStreamLength, 0);
							SPDLOG_LOGGER_DEBUG(NetworkLog, "recv() in dispatch returned {}:{}",
								Result,
								WSAGetLastError());

							// Handle recv() response and dispatch proper handling
							switch (Result) {
							case SOCKET_ERROR:

								// recv() error handler this disconnects or shuts down the server based on severity
								switch (auto SocketErrorCode = WSAGetLastError()) {
								default: {
									if (DispactchedIsServer()) {

										// Error on server listener occurred, the server has to be shut down
										// This is a destructive action and invalidate the server,
										// this object may no longer be used otherwise behavior is undefined
										SPDLOG_LOGGER_CRITICAL(NetworkLog, "Internal server error, recv() on server {} caused {}",
											SocketDescriptor.fd,
											SocketErrorCode);
										this->ManualReset();
										return STATUS_SERVER_SHUTDOWN;
									}

									// The client faulted, this as a result it will be disconnected in order to recover
									SPDLOG_LOGGER_ERROR(NetworkLog, "Unhandled recv() status {} on client {}",
										SocketErrorCode,
										SocketDescriptor.fd);
									DispatchDisconnectService(IoServiceRoutine,
										UserContext,
										SocketDescriptor.fd);
									return STATUS_SOCKET_DISCONNECTED;
								}}
								break;

							// Null read, graceful disconnect made by the remote
							case 0: {

								// A client disconnected gracefully, dispatch disconnect and redispatch dispatcher
								SPDLOG_LOGGER_INFO(NetworkLog, "Client {} gracefully closed session, disconnecting",
									SocketDescriptor.fd);
								DispatchDisconnectService(IoServiceRoutine,
									UserContext,
									SocketDescriptor.fd);
								return STATUS_SOCKET_DISCONNECTED;
							}

							// recv() successfully read the data from its buffer into our stream,
							// we can ignore the server itself as a context here as that is impossible
							// The data in the stream must be split and dispatched or rejected
							default: {

								// The start of the stream always represents the start of a package
								auto PackageStreamPointer = (ShipSockControl*)ClientNode->DataReceiverStream.get();
								ClientNode->ReceiverStreamLength += Result;
								DataExceededBufferReadAgain = false;

								// Check if the current buffer has been completely filled and schedule
								// a new recv() iteration to get the possibly stored data
								if (ClientNode->ReceiverStreamLength >= SizeOfReceiverStream) {

									// Schedule recv() stream exceed iteration, this will redo the loop until the recv() buffer
									// has been fully emptied or something else like a disconnect etc occurs
									DataExceededBufferReadAgain = true;
									SPDLOG_LOGGER_INFO(NetworkLog, "The stream was completely filled by recv(), scheduling redo");
								}

								// Iterate through the current stream until we either reach the end or cannot
								// dispatch incomplete packages anymore
								while (ClientNode->ReceiverStreamLength >= MinimumSizeRead &&
									PackageStreamPointer->SizeOfThisStruct <= ClientNode->ReceiverStreamLength) {

									// Dispatch message as we know the current contained buffer content is large enough,
									// to contain at least one ship control packet, therefore it can be directly referenced
									auto NetworkRequest = INrpBuilder(
										NwRequestPacket::INCOMING_PACKET,
										SocketDescriptor.fd);
									auto IoStatus = DispatchRequestAndReturn(IoServiceRoutine,
										UserContext,
										NetworkRequest,
										PackageStreamPointer);

									// Check for dispatch errors, if any we terminate the session and notify the caller
									if (IoStatus < 0) {

										// NOTE: Same as in the client, we do not need to move the stream, at this point
										//       the stream is already obsolete and will be deleted with the following call
										SPDLOG_LOGGER_CRITICAL(NetworkLog, "Callback failed to handle ship sock control command, terminating connection");
										DispatchDisconnectService(IoServiceRoutine,
											UserContext,
											SocketDescriptor.fd);
										return STATUS_SOCKET_DISCONNECTED;
									}

									// Update stream iterator states, if a error occurred previously they are trashed anyways
									ClientNode->ReceiverStreamLength -= PackageStreamPointer->SizeOfThisStruct;
									(char*&)PackageStreamPointer += PackageStreamPointer->SizeOfThisStruct;
								}

								// Move the left over data in package stream back to the start of the stream
								// This prepares the left over incomplete package for the next dispatch cycle
								memmove(ClientNode->DataReceiverStream.get(),
									(void*)PackageStreamPointer,
									ClientNode->ReceiverStreamLength);
							}}
						} while (DataExceededBufferReadAgain);
					} while (0);

					SocketDescriptor.revents &= ~POLLRDNORM;
				}
				/// Legacy code, probably never used every again
				// if (ReturnEvent & POLLWRNORM) {
				// 
				// 	SPDLOG_LOGGER_INFO(NetworkLog, "Client {} is ready to receive data",
				// 		SocketDescriptorTable[i].fd);
				// 
				// 	auto IoStatus = DispacthRequestAndReturn(IoServiceRoutine,
				// 		UserContext,
				// 		SocketDescriptorTable[i].fd,
				// 		NwRequestPacket::OUTGOING_PACKET_COMPLETE);
				// 	if (IoStatus < 0)
				// 		return SPDLOG_LOGGER_ERROR(NetworkLog, "Callback failed to handle critical request"),
				// 		IoStatus;
				// }
			}

			SPDLOG_LOGGER_DEBUG(NetworkLog, "All incoming requests have been handled successfully");
			return STATUS_DISPATCH_OK;
		}

		NwRequestPacket::NwRequestStatus
		DispatchRequestAndReturn(                     // Dispatches and catches a network request,
		                                              // returns the status of the NRP or translation
			function<MajorFunction> IoServiceRoutine, // Some routine responsible for handling a NRP
			void*                   UserContext,      // The passed user context forwarded to the routine
			NwRequestPacket&        NetworkRequest,   // A prebuild request to dispatch
			ShipSockControl*        ReqeustPackage    // Optional pointer to a ship sock control packet
		) {
			TRACE_FUNTION_PROTO;

			// Dispatch request and checkout
			SPDLOG_LOGGER_INFO(NetworkLog, "Dispatching NRP with NWCTL [{}]",
				NetworkRequest.IoControlCode);
			try {
				IoServiceRoutine(this,
					NetworkRequest,
					UserContext,
					ReqeustPackage);

				// If execution resumes here the packet was not completed, server has to terminate,
				// as the state of several components could now be indeterminate 
				auto ErrorMessage = fmt::format("The network request {} returned, it was not completed",
					(void*) & NetworkRequest);
				SPDLOG_LOGGER_CRITICAL(NetworkLog, ErrorMessage);
				throw runtime_error(ErrorMessage);
			}
			catch (const NwRequestPacket* CompletedPacket) {

				// Check if allocated packet is the same as returned and translate
				if (CompletedPacket == &NetworkRequest)
					switch (CompletedPacket->IoRequestStatus) {
					default:
						return CompletedPacket->StatusListModified ?
							NwRequestPacket::STATUS_LIST_MODIFIED
							: CompletedPacket->IoRequestStatus;
					}

				// If we get here the dispatched NRP is not the one collected, this should be impossible
				auto ErrorMessage = fmt::format("The completed packet {} doesnt match the send package signature {}", 
					(void*)CompletedPacket,
					(void*)&NetworkRequest);
				SPDLOG_LOGGER_ERROR(NetworkLog, ErrorMessage);
				throw runtime_error(ErrorMessage);
			}
		}
		void DispatchDisconnectService(               // Dispatches a disconnect notification to the callback
			                                          // and terminates the associated conneciton/session
			function<MajorFunction> IoServiceRoutine, // The passed routine responsible for handling
			void*                   UserContext,      // An associated user context that should be passed along to the routine
			SOCKET                  TerminateSocket   // The socket as an id used to identify the client to terminate
		) {
			TRACE_FUNTION_PROTO;

			// Build a basic disconnect packet and dispatch notification
			NwRequestPacket NetworkRequest{};
			NetworkRequest.IoControlCode = NwRequestPacket::SOCKET_DISCONNECTED;
			NetworkRequest.RequestingSocket = TerminateSocket;
			NetworkRequest.OptionalClient = GetClientBySocket(TerminateSocket);
			NetworkRequest.IoRequestStatus = NwRequestPacket::STATUS_REQUEST_NOT_HANDLED;
			auto RequestStatus = DispatchRequestAndReturn(IoServiceRoutine,
				UserContext,
				NetworkRequest,
				nullptr);
			if (RequestStatus < 0)
				throw (SPDLOG_LOGGER_CRITICAL(NetworkLog, "YOu fucked up to fuckiung hanle disconnet fucking faggot"),
					runtime_error("Are you even trying anymore? HOW THE FUCK DID WE END UP HERE"));

			// Removing client from entry/descriptor lists
			ConnectedClients.erase(TerminateSocket);
			SocketDescriptorTable.erase(find_if(
				SocketDescriptorTable.begin(),
				SocketDescriptorTable.end(),
				[&](				    // Compares an entry to the to be terminated socket
					const pollfd& Entry // The value of the entry to compare to
					) {
						TRACE_FUNTION_PROTO; return Entry.fd == TerminateSocket;
				}));
			closesocket(TerminateSocket);
			SPDLOG_LOGGER_INFO(NetworkLog, "Client {} was disconnected from the server",
				TerminateSocket);
		}

		int32_t GetLastWSAErrorOfSocket( // Gets the last status of the specified socket
			SOCKET SocketToProbe         //
		) {
			TRACE_FUNTION_PROTO;

			int32_t SocketErrorCode = SOCKET_ERROR;
			int32_t ProbeSize = sizeof(SocketErrorCode);
			if (getsockopt(SocketToProbe,
				SOL_SOCKET,
				SO_ERROR,
				(char*)&SocketErrorCode,
				&ProbeSize) == SOCKET_ERROR)
				return SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to query last error on socket {}, query error {}",
					SocketToProbe, WSAGetLastError()),
				SOCKET_ERROR;
			return SocketErrorCode;
		}



		// Primary server context / client manager
		vector<pollfd>               SocketDescriptorTable;
		map<SOCKET, NwClientControl> ConnectedClients;
		const size_t                 SizeOfReceiverStream;
	};

	SOCKET NwClientControl::GetSocket() const {
		TRACE_FUNTION_PROTO;

		// As long as this exists the Manager must also exist,
		// therefor this may never throw, if it does, job well done :clap:
		auto& NetworkManager = NetworkManager2::GetInstance();
		auto SocketAsId = NetworkManager.GetSocketByOperation(
			NetworkManager2::GET_SOCKET_OF_CLIENT,
			(void*)this);
		return SocketAsId;
	}
}
