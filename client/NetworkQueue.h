// This file implements the interfaces for the gameclient (implemented by @Daniel or @Sherlock)
// to inform or retrieve information from a connected server
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

private:

	SOCKET GameServer;
};

