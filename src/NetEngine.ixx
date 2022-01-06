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

// Misc
using namespace std;

namespace
{

	::Client::ManagementDispatchState s_ManagementState;

	unique_ptr<GameField> s_GameField;
	GmPlayerField* s_PlayerField;
	GmPlayerField* s_OpponentField;

	bool s_Connected { false };
	function<void()> s_FailureCallback;
	function<void()> s_SuccessCallback;

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

	bool Connect(const char* Username, const char* ServerAddress, const char* PortNumber,
		function<void()>&& FailureCallback, function<void()>&& SuccessCallback)
	{

		// Ensure state is not conflicting
		if (Network::Client::NetworkManager2::GetInstancePointer())
			return false;

		// Setup callbacks
		s_FailureCallback = move(FailureCallback);
		s_SuccessCallback = move(SuccessCallback);

		// Clear State
		s_ManagementState = { };
		s_Connected = false;
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

		if (s_Connected)
		{
			switch (ResponseOption)
			{
			case Network::Client::NetworkManager2::STATUS_SOCKET_DISCONNECTED:
				s_Connected = false;
				break;
			default:
				break;
			}
		}
		else if (ResponseOption < 0)
		{
			s_FailureCallback();
		}
		else if (s_ManagementState.StateReady)
		{
			s_ManagementState.StateReady = false;
			auto& ManagerInstance = GameManager2::CreateObject(
				s_ManagementState.InternalFieldDimensions,
				s_ManagementState.NumberOFShipsPerType);

			::Client::InstallGameManagerInstrumentationCallbacks(ServerObject);

			s_PlayerField = ManagerInstance.TryAllocatePlayerWithId(s_ManagementState.ClientIdByServer);
			s_OpponentField = ManagerInstance.TryAllocatePlayerWithId(1);

			s_GameField = make_unique<GameField>(ManagerInstance, s_PlayerField, s_OpponentField);

			s_Connected = true;
			s_SuccessCallback();

		}
	}

	void CleanUp()
	{
		Network::Client::NetworkManager2::ManualReset();
		s_GameField.reset();
	}

}