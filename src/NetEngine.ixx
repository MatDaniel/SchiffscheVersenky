module;

#include <functional>

export module Draw.NetEngine;

// Network & Game Management
import GameManagment;
import Network.Client;
import DispatchRoutine;

using namespace GameManagment;
using namespace Network;
using namespace std;

namespace
{

	enum NetState {
		NS_NONE,
		NS_CONNECTING,
		NS_CONNECTED
	};

	NetState s_InternalNetworkState;
	function<void()> s_SuccessCallback;
	function<void()> s_FailureCallback;

}

export namespace Draw::NetEngine
{

	bool Connect(const char* Username, const char* ServerAddress, const char* PortNumber,
		function<void()>&& FailureCallback, function<void()>&& SuccessCallback)
	{
	
		// Ensure state is not conflicting
		if (s_InternalNetworkState != NS_NONE)
			return false;

		// Setup callbacks
		s_FailureCallback = move(FailureCallback);
		s_SuccessCallback = move(SuccessCallback);

		// Connect
		Network::Client::NetworkManager2::CreateObject(
			ServerAddress,
			PortNumber);

		// Update State
		s_InternalNetworkState = NS_CONNECTING;

		// Return result
		return true;

	}

	void Update()
	{

		// Ensure network is in some different state than none
		if (s_InternalNetworkState == NS_NONE)
			return;

		// Get network instance
		auto& ServerObject = Network::Client::NetworkManager2::GetInstance();

		switch (s_InternalNetworkState)
		{

		case NS_CONNECTING:
			{
				::Client::ManagmentDispatchState State{};


				auto ResponseOption = ServerObject.ExecuteNetworkRequestHandlerWithCallback(
					::Client::ManagementDispatchRoutine,
					(void*)&State);

				if (ResponseOption < 0)
				{
					s_InternalNetworkState = NS_NONE;
					s_FailureCallback();
				}
				else if (State.StateReady)
				{
					State.StateReady = false;
					GameManager2::CreateObject(State.InternalFieldDimensions, State.NumberOFShipsPerType);
					s_InternalNetworkState = NS_CONNECTED;
					s_SuccessCallback();
				}
			}
			break;
		case NS_CONNECTED:
			// This is the last state, there is nothing more to do, at least for now.
			// TODO: Implement more network states
			break;

		default:
			// This case should not be called.
			__debugbreak();
			s_InternalNetworkState = NS_NONE;
			Network::Client::NetworkManager2::ManualReset();
			break;

		}
	}

}