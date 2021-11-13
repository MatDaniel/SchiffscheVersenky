// This File will describe all interfaces for the GamestateManager (will be implementing the games server sided logic ig -> philip)
// to control the clients and establish a Lobby over a Socket.
#include "ShipSock.h"
#include <vector>
#include <shared_mutex>


class NetWorkIoControl 
	: private SocketWrap {
public:
	class ClientController {
		friend NetWorkIoControl;
	public:



	private:
		ClientController();

		SOCKET   AssociatedClient;
		sockaddr ClientInformation;
		int32_t  ClientInfoSize;

		uint32_t WorkerThreadId;
	};

	typedef long ConnectionHandler(       // this has to handle the networking requests being both capable of reading and sending requests
		NetWorkIoControl& NetworkObject,  // a reference to the NetWorkIoController responsible of the allocation of said specified 
		SOCKET           AcceptedClient,
		void*            UserContext
	);

	NetWorkIoControl(
		const char* ServerPort
	);
	~NetWorkIoControl();

	SOCKET GetServerSocketHandle();
	
	__declspec(noreturn)
	void LaunchClientManagerConvertThread(           // Starts internally handling accept requests passively, accepted requests get added to a 
		ConnectionHandler AcceptConnectionHandler,
		void* UserContext
	);

private:

	std::shared_mutex NetworkLock;
	SOCKET LocalServerSocket = INVALID_SOCKET;
	std::vector<ClientController> ConnectedClients;
};
