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

// Misc
using namespace std;

namespace
{

	enum NetState {
		NS_NONE,
		NS_CONNECTING,
		NS_CONNECTED
	};

	::Client::ManagementDispatchState s_ManagementState;

	NetState s_InternalNetworkState;
	function<void()> s_FailureCallback;
	function<void()> s_SuccessCallback;

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

		// Clear State
		s_ManagementState = { };

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
				auto ResponseOption = ServerObject.ExecuteNetworkRequestHandlerWithCallback(
					::Client::ManagementDispatchRoutine,
					(void*)&s_ManagementState);

				if (ResponseOption < 0)
				{
					s_InternalNetworkState = NS_NONE;
					s_FailureCallback();
				}
				else if (s_ManagementState.StateReady)
				{
					s_ManagementState.StateReady = false;
					GameManager2::CreateObject(
						s_ManagementState.InternalFieldDimensions,
						s_ManagementState.NumberOFShipsPerType);

					::Client::InstallGameManagerInstrumentationCallbacks(
						Network::Client::NetworkManager2::GetInstancePointer());

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

	void CleanUp()
	{
		if (s_InternalNetworkState != NS_NONE)
		{
			s_InternalNetworkState = NS_NONE;
			Network::Client::NetworkManager2::ManualReset();
		}
	}

}