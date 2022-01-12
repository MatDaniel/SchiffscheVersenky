module;

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <GLFW/glfw3.h>

#include <memory>
#include <string>
#include <utility>

#include "../BattleShip.h"

export module Scenes.Menu;
import Draw.Window;
import Draw.Scene;
import Draw.Engine;
import Draw.Resources;
import Draw.Renderer;
import Draw.NetEngine;
import Scenes.FieldSetup;
import Scenes.Spectator;
import Network.Client;
import GameManagement;
import Scenes.FieldSetup;
import Draw.NetEngine;
import Draw.NetEngine.Callbacks;


using namespace std;
using namespace Network;
using namespace GameManagement;

namespace Draw::Scenes
{

	static const char* const DefaultUsername = "Player";

	export class MenuScene final : public Scene
	{
	public:

		enum Winner
		{
			WIN_NONE,
			WIN_PLAYER,
			WIN_OPPNENT
		};


		MenuScene(Winner Win = WIN_NONE)
			: m_Win(Win)
		{
		}

		void OnInit() override
		{
			Draw::NetEngine::Callbacks::OnConnectFail = [this]() {
				m_FailedToConnect = true;
				m_IsConnecting = false;
			};
            Draw::NetEngine::Callbacks::OnConnectStart = []() {
                Scene::Load(make_unique<FieldSetupScene>(
                    NetEngine::Properties::GameField.get()
                ));
            };
			Draw::NetEngine::Callbacks::OnReset = []() {
				Scene::Load(make_unique<MenuScene>());
			};
			Draw::NetEngine::Callbacks::OnEndLost = []() {
				Scene::Load(make_unique<MenuScene>(WIN_OPPNENT));
			};
			Draw::NetEngine::Callbacks::OnEndWin = []() {
				Scene::Load(make_unique<MenuScene>(WIN_PLAYER));
			};
		}

		void OnDraw() override
		{


			Window::Properties::FrameBuffer.Select();

			// Clear color
			//-------------

			switch (m_Win)
			{
			case WIN_OPPNENT:
				glClearColor(0.698F, 0.133F, 0.133F, 1.0F);
				break;
			case WIN_PLAYER:
				glClearColor(0.0F, 0.5F, 0.0F, 1.0F);
				break;
			default:
				glClearColor(0.118F, 0.477F, 1.0F, 1.0F);
				break;
			}
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Draw main menu
			//----------------

			bool IsMenuDisabled { m_ShowServerConnectWindow || m_Win != WIN_NONE };
			auto WindowSize { Window::Properties::WindowSize };
			ImVec2 ImWindowSize { ImVec2(static_cast<float>(WindowSize.x), static_cast<float>(WindowSize.y)) };
			ImGui::SetNextWindowSize(ImWindowSize);
			ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

			ImGui::Begin("Main Menu", nullptr,
				ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);

			// Apply style pushed above
			ImGui::PopStyleVar(2);

			// Buttons
			ImGui::BeginDisabled(IsMenuDisabled);
			if (ImGui::Button("Play"))
				m_ShowServerConnectWindow = true;
			if (ImGui::Button("Quit"))
				Engine::Exit();
			ImGui::EndDisabled();

			// Darken the scene while the connection popup is open
			if (IsMenuDisabled)
			{
				auto drawList = ImGui::GetWindowDrawList();
				auto col = ImColor(0.0F, 0.0F, 0.0F, 0.5F);
				drawList->AddRectFilled(ImVec2(0.0F, 0.0F), ImWindowSize, col, 0.0f, ImDrawFlags_None);
			}

			// Winner window
			if (m_Win != WIN_NONE)
			{

				ImVec2 ImConnectWinSize{ 400, 53 };
				ImVec2 ImConnectWinPos{
					(ImWindowSize.x - ImConnectWinSize.x) / 2,
					(ImWindowSize.y - ImConnectWinSize.y) / 2
				};

				ImGui::SetNextWindowPos(ImConnectWinPos);
				ImGui::BeginChild("Connect", ImConnectWinSize, true, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);

				// Winner message
				switch (m_Win)
				{
				case WIN_PLAYER:
					ImGui::Text("Congratulations, you won!");
					break;
				case WIN_OPPNENT:
					ImGui::Text("You lost the game.");
					break;
				default:
					ImGui::Text("This is an error!");
					break;
				}
				

				// Buttons
				if (ImGui::Button("Close"))
				{
					m_Win = WIN_NONE;
				}

				ImGui::EndChild();

			}

			// Connection popup
			if (m_ShowServerConnectWindow)
			{

				ImVec2 ImConnectWinSize{ 400, 105 };
				if (m_FailedToConnect)
					ImConnectWinSize.y += 16;
				ImVec2 ImConnectWinPos{
					(ImWindowSize.x - ImConnectWinSize.x) / 2,
					(ImWindowSize.y - ImConnectWinSize.y) / 2
				};

				ImGui::SetNextWindowPos(ImConnectWinPos);
				ImGui::BeginChild("Connect", ImConnectWinSize, true, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);

				// Connection form
				ImGui::InputTextWithHint("Username", DefaultUsername, m_InputUsername, IM_ARRAYSIZE(m_InputUsername));
				ImGui::InputTextWithHint("Address", DefaultServerAddress, m_InputAddress, IM_ARRAYSIZE(m_InputAddress));
				ImGui::InputTextWithHint("Port", DefaultPortNumber, m_InputPort, IM_ARRAYSIZE(m_InputPort));

				// Connection message
				if (m_FailedToConnect)
					ImGui::Text("Failed to connect!");

				// Buttons
				ImGui::BeginDisabled(m_IsConnecting);
				if (ImGui::Button("Connect"))
				{

					// Select connection variables
					const char* Username = m_InputUsername[0] == '\0' ?
						DefaultUsername : m_InputUsername;
					const char* ServerAddress = m_InputAddress[0] == '\0' ?
						DefaultServerAddress : m_InputAddress;
					const char* PortNumber = m_InputPort[0] == '\0' ?
						DefaultPortNumber : m_InputPort;

					try
					{
						if (NetEngine::Connect(Username, ServerAddress, PortNumber))
						{
							m_IsConnecting = true;
							m_FailedToConnect = false;
						}
					}
					catch (...)
					{
						m_FailedToConnect = true;
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					m_ShowServerConnectWindow = false;
					m_FailedToConnect = false;
				}
#ifndef NDEBUG
				ImGui::SameLine();
				if (ImGui::Button("[Test] Without Server"))
				{
					NetEngine::ConnectWithoutServer();
					Scene::Load(make_unique<FieldSetupScene>(
						NetEngine::Properties::GameField.get(),
						true));
				}
				ImGui::SameLine();
				if (ImGui::Button("[Test] Spectator"))
				{
					NetEngine::ConnectWithoutServer();
					Scene::Load(make_unique<SpectatorScene>(
						NetEngine::Properties::GameField.get()
					));
				}
#endif
				ImGui::EndDisabled();

				ImGui::EndChild();
			}

			ImGui::End();

		}

	private:

		Winner m_Win;
		char m_InputUsername[32]{ "" };
		char m_InputAddress[64]{ "" };
		char m_InputPort[6]{ "" };
		bool m_ShowServerConnectWindow{ false };
		bool m_IsConnecting{ false };
		bool m_FailedToConnect{ false };

	};

}