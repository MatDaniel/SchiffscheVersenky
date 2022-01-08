module;

#include <memory>
#include <functional>

export module Draw.NetEngine;

// Network & Game Management
import GameManagement;
import Network.Client;
import DispatchRoutine;

using namespace GameManagement;
using namespace Network;

// Game
import Game.GameField;

// Callbacks
import Draw.NetEngine.Callbacks;

// Misc
using namespace std;

namespace
{

	::Client::ManagementDispatchState s_ManagementState;

	unique_ptr<GameField> s_GameField;
	GmPlayerField* s_PlayerField;
	GmPlayerField* s_OpponentField;

	enum GameState
	{
		GS_NONE,
		GS_CONNECTING,
		GS_SETUP,
		GS_GAME
	};

	GameState s_CurrentState { GS_NONE };
	
}

export namespace Draw::NetEngine
{

	namespace Properties
	{
		auto& const GameField = s_GameField;
	}

	void ConnectWithoutServer()
	{
		PointComponent FieldDimensions { 12, 12 };
		ShipCount MaxShipsOnField { 2, 2, 2, 2, 2 };
		auto& ManagerInstance = GameManager2::CreateObject(FieldDimensions, MaxShipsOnField);
		s_PlayerField = ManagerInstance.TryAllocatePlayerWithId(1);
		s_OpponentField = ManagerInstance.TryAllocatePlayerWithId(2);
		s_GameField = make_unique<GameField>(ManagerInstance, s_PlayerField, s_OpponentField);
	}

	bool Connect(const char* Username, const char* ServerAddress, const char* PortNumber)
	{

		// Ensure state is not conflicting
		if (Network::Client::NetworkManager2::GetInstancePointer())
			return false;

		// Clear State
		s_ManagementState = { };
		s_CurrentState = GS_NONE;
		s_PlayerField = nullptr;
		s_OpponentField = nullptr;

		// Connect
		Network::Client::NetworkManager2::CreateObject(
			ServerAddress,
			PortNumber);

		// Return result
		return true;

	}

	void Update()
	{

		// Get network instance
		auto* ServerObject = Network::Client::NetworkManager2::GetInstancePointer();

		// Ensure the network instance exist
		if (!ServerObject)
			return;

		auto ResponseOption = ServerObject->ExecuteNetworkRequestHandlerWithCallback(
			::Client::ManagementDispatchRoutine,
			(void*)&s_ManagementState);


		if (ResponseOption >= 0)
		{
			switch (s_CurrentState)
			{
			case GS_CONNECTING:
				if (s_ManagementState.StateReady)
				{
					s_ManagementState.StateReady = false;
					auto& ManagerInstance = GameManager2::CreateObject(
						s_ManagementState.InternalFieldDimensions,
						s_ManagementState.NumberOFShipsPerType);

					::Client::InstallGameManagerInstrumentationCallbacks(ServerObject);

					s_PlayerField = ManagerInstance.TryAllocatePlayerWithId(s_ManagementState.ClientIdByServer);
					s_OpponentField = ManagerInstance.TryAllocatePlayerWithId(1);

					s_GameField = make_unique<GameField>(ManagerInstance, s_PlayerField, s_OpponentField);

					s_CurrentState = GS_SETUP;
					Callbacks::OnConnectStart();
				}
				break;
			case GS_SETUP: {
					if (s_ManagementState.GameHasStarted)
					{
						s_ManagementState.GameHasStarted = false;
						s_CurrentState = GS_GAME;
						Callbacks::OnGameStart();
					}
				}
				break;
			case GS_GAME: {

				}
				break;
			}
			
			
		}
		else
		{
			if (s_CurrentState == GS_CONNECTING)
				Callbacks::OnConnectFail();
			else
				Callbacks::OnReset();
			s_CurrentState = GS_NONE;
			Network::Client::NetworkManager2::ManualReset();
		}
	}

	void CleanUp()
	{
		Network::Client::NetworkManager2::ManualReset();
		s_GameField.reset();
	}

}