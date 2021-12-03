// This File will describe all interfaces for the GamestateManager (will be implementing the games server sided logic ig -> philip)
// to control the clients and establish a Lobby over a Socket.

#ifdef _MSC_VER
#pragma warning(disable : 26812) // unscoped enum
#pragma warning(disable : 6336)  // heusteric, bufferoverrun
#pragma warning(disable : 28183) // heusteric, nullpointer realloc
#pragma warning(disable : 6201)  // heusteric, outofbounds index
// #pragma warning(disable : )
#endif

#include "ShipSock.h"
#include <memory>
#include <vector>



class NetWorkIoControl;

class ClientController {
	friend NetWorkIoControl;
public:
	SOCKET GetSocket() const;

	bool operator==(
		const ClientController* Rhs
	) const;

private:
	ClientController(size_t InformationSize);

	SOCKET   AssociatedClient;
	sockaddr ClientInformation;
	int32_t  ClientInfoSize;
};

struct EnableMakeUnique;
class NetWorkIoControl 
	: private SocketWrap {
public:
	struct IoRequestPacket {
		enum IoServiceRoutineCtl {           // a control code specifiying the type of handling required
			NO_COMMAND,                      // reserved for @Lima, dont use
			INCOMING_CONNECTION,
			INCOMING_PACKET,
			INCOMING_PACKET_PRIORITIZED,
			OUTGOING_PACKET_COMPLETE,
			SOCKET_DISCONNECTED,
			SERVICE_ERROR_DETECTED,
		}                 IoControlCode;    
		SOCKET            RequestingSocket; // the socket which was responisble for the incoming request
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
		NetWorkIoControl& NetworkDevice,  // a pointer to the NetWorkIoController responsible of said requestpacket
		IoRequestPacket&  NetworkRequest, // a pointer to a network request packet describing the current request
		void*             UserContext     // A pointer to caller defined data thats forwarded to the callback in every call, could be the GameManager class or whatever
	);

	// Constructor/Desctructor Pseudosingleton bullshit
	static NetWorkIoControl* CreateSingletonOverride(
		const char* ServerPort
	);
	static NetWorkIoControl* GetInstance();	
	~NetWorkIoControl();
	NetWorkIoControl(const NetWorkIoControl&) = delete;
	NetWorkIoControl& operator=(const NetWorkIoControl&) = delete;



	SOCKET GetServerSocketHandle();
	void AcceptIncomingConnection(
		IoRequestPacket& NetworkRequest
	);

	long PollNetworkConnectionsAndDispatchCallbacks( // Starts internally handling accept requests passively,
	                                                 // notifies caller over callbacks and handles io requests
	                                                 // @Sherlock will have to call this with his gamemanager ready,
	                                                 // the callback is reponsible for handling incoming io and connections.
		MajorFunction IoCompleteRequestRoutine,      // a function pointer to a callback that handles socket operations
		void*         UserContext,                   // a user suplied buffer containign arbitrary data forwarded to the callback
		int32_t       Timeout = -1                   // specifies the timeout count the functions waits on poll till it returns
	);

private:
	NetWorkIoControl(
		const char* ServerPort
	);

	SOCKET                        LocalServerSocket = INVALID_SOCKET;
	std::vector<ClientController> ConnectedClients;
	std::vector<WSAPOLLFD>        SocketDescriptorTable;
 
	static std::unique_ptr<NetWorkIoControl> InstanceObject;
};
inline std::unique_ptr<NetWorkIoControl> NetWorkIoControl::InstanceObject;

class GameManagmentController {
public:
	
	
	struct PlayerFiled {
		enum CellState : uint8_t {
			CELL_IS_EMPTY,
			CELL_IS_IN_USE,
			CELL_WAS_SHOT_EMPTY,
			CELL_WAS_SHOT_IN_USE
		} ** CellField;
		struct {
			uint8_t Xd,
			        Yd;
		} FieldDimensions;

		struct ShipState {
			ShipSockControl::ShipTypes ShipType;
			uint8_t XCord,
				    YCord;
			uint8_t Rotation;
			bool    Destroyed;
		} * ShipStates;
		struct ShipData {
			uint8_t DestroyerCount;
			uint8_t CruiserCount;
			uint8_t SubmarineCount;
			uint8_t BattleshipCount;
			uint8_t CarrierCount;
		};
	};


private:
	


};


