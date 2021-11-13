#include <iostream>
#include "NetworkIoControl.h"

int main(
	int argc,
	const char* argv[]
) {
	// Create Server on passed port number or use default
	const char* PortNumber = DefaultPortNumber;
	if (argc == 2)
		PortNumber = argv[1];
	NetWorkIoControl ShipSocketObject(PortNumber);
	if (SsAssert(ShipSocketObject.GetServerSocketHandle() == INVALID_SOCKET,
		"critical failure, invalid socket escaped constructor ?!\n"))
		return EXIT_FAILURE;




	return EXIT_SUCCESS;
}