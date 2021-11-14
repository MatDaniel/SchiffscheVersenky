#include <iostream>
#include "NetworkIoControl.h"

void NetworkDispatchTest(
	NetWorkIoControl*                  NetworkDevice,
	NetWorkIoControl::IoRequestPacket* NetworkRequest,
	void*                              UserContext
) {
	switch (NetworkRequest->IoControlCode) {
	case NetWorkIoControl::IoRequestPacket::INCOMING_CONNECTION:
		NetworkRequest->IoRequestStatus = NetworkDevice->AcceptIncomingConnection(NetworkRequest);
		break;

	case NetWorkIoControl::IoRequestPacket::INCOMING_PACKET:
		
		break;
	}
}

int main(
	int argc,
	const char* argv[]
) {
	// Create Server on passed port number or use default
	const char* PortNumber = DefaultPortNumber;
	if (argc == 2)
		PortNumber = argv[1];
	long Result = 0;
	NetWorkIoControl ShipSocketObject(PortNumber, &Result);
	if (SsAssert(Result < 0,
		"critical failure, invalid socket escaped constructor ?!\n"))
		return EXIT_FAILURE;

	Result = ShipSocketObject.LaunchServerIoManagerAndRegisterServiceCallback(
		NetworkDispatchTest,
		nullptr);
	if (SsAssert(Result < 0,
		"NetworkManager failed to execute properly -> shuting down server : %d\n",
		Result))
		return EXIT_FAILURE;


	return EXIT_SUCCESS;
}