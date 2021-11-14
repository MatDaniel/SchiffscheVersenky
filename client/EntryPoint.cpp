#include "NetworkQueue.h"
#include "draw/Game.hpp"
#include "draw/scenes/MenuScene.hpp"

int main(
	int         argc,
	const char* argv[]
) {
	auto ServerAddress = DefaultServerAddress;
	auto PortNumber = DefaultPortNumber;
	if (argc >= 2)
		ServerAddress = argv[1];
	if (argc >= 3)
		PortNumber = argv[2];


	long Result = 0;
	ServerIoController NetworkManager(
		ServerAddress,
		PortNumber,
		&Result);
	SsAssert(Result < 0,
		"failed to connect client socket to server for io operation with: %d\n",
		Result);



	return Game::run<MenuScene>();

}