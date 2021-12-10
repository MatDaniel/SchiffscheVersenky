// This file implements the client side of the NetworkIoControl manager,
// it provides the necessary interfaces to communicate back to the server
// and receive multiplexed nonblocking io to run in parallel with the engine
module;

#define FD_SETSIZE 1
#include <SharedLegacy.h>
#include <memory>

export module NetworkControl;
import ShipSock;
using namespace std;



export class ServerIoController
	: private SocketWrap {
public:
	struct IoRequestPacketIndirect {
		enum IoServiceRoutineCtl {           // a control code specifying the type of handling required
			NO_COMMAND,                      // reserved for @Lima, dont use
			INCOMING_PACKET,
			INCOMING_PACKET_PRIORITIZED,
			OUTGOING_PACKET_COMPLETE,
			SOCKET_DISCONNECTED,
			SERVICE_ERROR_DETECTED,
		} IoControlCode;

		ShipSockControl* ControlIoPacketData;


		enum IoRequestStatusType {      // describes the current state of this network request packet,
										// the callback has to set it correctly depending on what it did with the request
			STATUS_REQUEST_ERROR = -3,
			STATUS_REQUEST_NOT_HANDLED = -2,
			INVALID_STATUS = -1,
			STATUS_REQUEST_COMPLETED = 0,
			STATUS_REQUEST_IGNORED = 1,
		} IoRequestStatus;
	};
	typedef void(*MajorFunction)(                // this has to handle the networking requests being both capable of reading and sending requests
		ServerIoController*      NetworkDevice,  // a pointer to the NetWorkIoController responsible of said requestpacket
		IoRequestPacketIndirect* NetworkRequest, // a pointer to a network request packet describing the current request
		void*                    UserContext     // A pointer to caller defined data thats forwarded to the callback in every call, could be the GameManager class or whatever
		);



	static ServerIoController* CreateSingletonOverride(
		const char* ServerName,
		const char* ServerPort
	) {
		SsLog("ServerIoCtl Factory called, creating NetworkingObject\n");

		// Magic fuckery cause make_unique cannot normally access a private constructor
		struct EnableMakeUnique : public ServerIoController {
			inline EnableMakeUnique(
				const char* ServerName,
				const char* ServerPort
			) : ServerIoController(
				ServerName,
				ServerPort) {}
		};
		return (InstanceObject = make_unique<EnableMakeUnique>(
			ServerName,
			ServerPort)).get();
	}
	static ServerIoController* GetInstance() {
		return InstanceObject.get();
	}

	~ServerIoController() {
		SsLog("Network manger deconstructor called,\n"
			"closing socket and cleaning up");
	}
	ServerIoController(const ServerIoController&) = delete;
	ServerIoController& operator=(const ServerIoController&) = delete;



	long ExecuteNetworkRequestHandlerWithCallback( // Probes requests and prepares IORP's for asynchronous networking
												   // notifies the caller over a callback and provides completion routines
		MajorFunction IoCompleteRequestQueue,      // yes this is your callback sir, treat it carefully, i wont do checks on this
		void*         UserContext                  // some user provided polymorphic value forwarded to the handler routine
	) {
		// Check if Multiplexer has attached to server 
		if (!SocketAttached) {

			SsLog("Querying connecting socket request");
			fd_set WriteSet;
			FD_ZERO(&WriteSet);
			FD_SET(GameServer, &WriteSet);
			fd_set ExceptionSet;
			FD_ZERO(&ExceptionSet);
			FD_SET(GameServer, &ExceptionSet);
			timeval Timeout{};
			auto Result = select(0, nullptr,
				&WriteSet,
				&ExceptionSet,
				&Timeout);

			switch (Result) {
			case SOCKET_ERROR:
				SsLog("socket query failed, cause: %d",
					WSAGetLastError());
				return SOCKET_ERROR;

			case 0:
				SsLog("Connect has not finished work yet");
				return STATUS_WORK_PENDING;

			case 1:
				if (WriteSet.fd_count) {

					SsLog("Server connected on socket, switching controller IoMode");
					freeaddrinfo(ServerInformation);
					SocketAttached = true;
					break;
				}
				if (ExceptionSet.fd_count)
					return SsLog("Client failed to connect to socket"),
					STATUS_FAILED_TO_CONNECT;
			}
		}

		SsLog("Checking for input and handling messages\n");
		long Result = 0;
		auto PacketBuffer = make_unique<char[]>(PACKET_BUFFER_SIZE);
		do {
			Result = recv(GameServer,
				PacketBuffer.get(),
				PACKET_BUFFER_SIZE,
				0);

			switch (Result) {
			case SOCKET_ERROR:
				switch (auto SocketErrorCode = WSAGetLastError()) {
				case WSAEWOULDBLOCK:
					SsLog("No (more) request to handle, we can happily exit now :)\n");
					Result = 0;
					break;

				case WSAECONNRESET:
					SsLog("Connection reset, server aborted, closing socket");
					SsAssert(closesocket(GameServer),
						"Server socket failed to close properly: %d",
						WSAGetLastError());
					return IoRequestPacketIndirect::STATUS_REQUEST_ERROR;

				default:
					SsLog("Ok shit went south here, uh dont know what to do here ngl,\n"
						"time to die ig: %d",
						SocketErrorCode);
					Result = IoRequestPacketIndirect::STATUS_REQUEST_ERROR;
				}
				break;

			case 0: {
				SsLog("Server disconnected from client by remote\n");

				IoRequestPacketIndirect RequestDispatchPacket{
					.IoControlCode = IoRequestPacketIndirect::SOCKET_DISCONNECTED,
					.IoRequestStatus = IoRequestPacketIndirect::STATUS_REQUEST_NOT_HANDLED
				};
				IoCompleteRequestQueue(this,
					&RequestDispatchPacket,
					UserContext);
				if (SsAssert(RequestDispatchPacket.IoRequestStatus < 0,
					"Callback refused or did not handle server disconnect properly,\n"
					"tHE FUck are you doing @Daniel")) {

					Result = RequestDispatchPacket.IoRequestStatus;
				}
			}
				break;

			default: {
				auto ShipPacket = (ShipSockControl*)PacketBuffer.get();
				SsLog("Recived Gamenotification, IOCTL: &d",
					ShipPacket->ControlCode);

				IoRequestPacketIndirect RequestDispatchPacket{
					.IoControlCode = IoRequestPacketIndirect::INCOMING_PACKET,
					.ControlIoPacketData = ShipPacket,
					.IoRequestStatus = IoRequestPacketIndirect::STATUS_REQUEST_NOT_HANDLED
				};
				IoCompleteRequestQueue(this,
					&RequestDispatchPacket,
					UserContext);
				if (SsAssert(RequestDispatchPacket.IoRequestStatus < 0,
					"Callback failed to handle request")) {

					Result = RequestDispatchPacket.IoRequestStatus;
				}
			}
			}
		} while (Result > 0);

		return Result;
	}

private:
	ServerIoController(
		const char* ServerName, // aka ipv4/6 address
		const char* PortNumber  // the Port Number the server runs on
	) {
		SsLog("Preparing internal server structures for async IO operation\n"
			"Retrieving remote server information\n");
		addrinfo AddressHints{
			.ai_family = AF_INET,
			.ai_socktype = SOCK_STREAM,
			.ai_protocol = IPPROTO_TCP
		};
		auto Result = getaddrinfo(
			ServerName,
			PortNumber,
			&AddressHints,
			&ServerInformation);
		if (SsAssert(Result,
			"failes to retieve portinfo on \"%s\" with: %d\n",
			PortNumber,
			Result))
			throw -1;

		SsLog("Preparing gameserver socket\n");
		auto ServerInformationIterator = ServerInformation;
		u_long BlockingMode = 1;
		do {
			GameServer = socket(
				ServerInformationIterator->ai_family,
				ServerInformationIterator->ai_socktype,
				ServerInformationIterator->ai_protocol);
		} while (GameServer == INVALID_SOCKET &&
			(ServerInformationIterator = ServerInformationIterator->ai_next));
		if (SsAssert(GameServer == INVALID_SOCKET,
			"failed to create socked with: %d\n",
			WSAGetLastError())) {

			freeaddrinfo(ServerInformation);
			throw -2;
		}

		SsLog("Switching to nonblocking mode on socket: [%p]",
			GameServer);
		Result = ioctlsocket(
			GameServer,
			FIONBIO,
			&BlockingMode);
		if (SsAssert(Result,
			"failed to switch socket into non blocking mode: %d",
			WSAGetLastError())) {

			freeaddrinfo(ServerInformation);
			SsAssert(closesocket(GameServer),
				"failed to close server socket, critical?: %d\n",
				WSAGetLastError());
			throw -3;
		}

		SsLog("Queuing socket connect request in async mode");
		Result = connect(
			GameServer,
			ServerInformation->ai_addr,
			ServerInformation->ai_addrlen);
		if (Result == SOCKET_ERROR) {
			auto Response = WSAGetLastError();
			if (SsAssert(Response != WSAEWOULDBLOCK,
				"failed to schedule conneciton request, cause unknown controller invlaid: %d",
				Response))
				throw -4;
		}
	}



	SOCKET    GameServer = INVALID_SOCKET;
	bool      SocketAttached = false;
	addrinfo* ServerInformation = nullptr;

	static inline unique_ptr<ServerIoController> InstanceObject;
};
