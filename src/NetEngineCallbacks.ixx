module;

#include <functional>

export module Draw.NetEngine.Callbacks;

using namespace std;

export namespace Draw::NetEngine::Callbacks
{

	using VoidCallbackFunc = function<void()>;

	VoidCallbackFunc OnConnectStart;
	VoidCallbackFunc OnConnectFail;
	VoidCallbackFunc OnGameStart;
	VoidCallbackFunc OnReset;
	VoidCallbackFunc OnEndLost;
	VoidCallbackFunc OnEndWin;

}