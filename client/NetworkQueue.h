// This file implements the interfaces for the gameclient (implemented by @Daniel or @Sherlock)
// to inform or retrieve information from a connected server

#define FD_SETSIZE 1
#include "ShipSock.h"

#ifdef _MSC_VER
#pragma warning(disable : 26812) // unscoped enum
#pragma warning(disable : 6336)  // heusteric, bufferoverrun
#pragma warning(disable : 28183) // heusteric, nullpointer realloc
#pragma warning(disable : 6201)  // heusteric, outofbounds index
// #pragma warning(disable : )
#endif

class ServerIoController 
	: private SocketWrap {
public:
	ServerIoController(
		const char* ServerName, // aka ipv4/6 address
		const char* PortNumber, // the Port Number the server runs on
		long*       OutResponse
	);
	~ServerIoController();

	struct IoRequestPacketIndirect {
		enum IoServiceRoutineCtl {           // a control code specifiying the type of handling required
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
			STATUS_REQUEST_IGNORED = 0,
			STATUS_REQUEST_COMPLETED = 1
		} IoRequestStatus;
	};
	typedef void(*MajorFunction)(                // this has to handle the networking requests being both capable of reading and sending requests
		ServerIoController*      NetworkDevice,	 // a pointer to the NetWorkIoController responsible of said requestpacket
		IoRequestPacketIndirect* NetworkRequest, // a pointer to a network request packet describing the current request
		void*                    UserContext	 // A pointer to caller defined data thats forwarded to the callback in every call, could be the GameManager class or whatever
	);

	long ExecuteNetworkRequestHandlerWithCallback( // Probes requests and prepares IORP's for asynchronous networking
																								 // notifies the caller over a callback and provides completion routines
		MajorFunction IoCompleteRequestQueue,                                                    // yes this is your callback sir, treat it carefully, i wont do checks on this
		void*         UserContext                                                                // some user provided polimorphic value forwarded to the handler routine
	);


private:
	long QueueConnectAttemptTimepoint();
	void ConnectSuccessSwitchToMode();
	long QueryServerIsConnected();

	SOCKET    GameServer = INVALID_SOCKET;
	bool      SocketAttached = false;
	addrinfo* ServerInformation = nullptr;
};

