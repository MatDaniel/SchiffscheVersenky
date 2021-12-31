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
import Network.Client;
import GameManagment;
import Scenes.FieldSetup;
import Draw.NetEngine;

using namespace GameManagment;
using namespace Network;
using namespace std;

const char* DefaultUsername = "Player";

export class MenuScene final : public Scene
{
public:

	void OnInit() override
	{

	}

	void OnDraw() override
	{
		// Clear screen
		//--------------

		Window::Properties::FrameBuffer.Select();

		glClearColor(0.8F, 0.2F, 0.1F, 1.0F);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
		ImGui::BeginDisabled(m_ShowServerConnectWindow);
		if (ImGui::Button("Play"))
			m_ShowServerConnectWindow = true;
		if (ImGui::Button("Quit"))
			Engine::Exit();
		ImGui::EndDisabled();

		// Darken the scene while the connection popup is open
		if (m_ShowServerConnectWindow)
		{
			auto drawList = ImGui::GetWindowDrawList();
			auto col = ImColor(0.0F, 0.0F, 0.0F, 0.5F);
			drawList->AddRectFilled(ImVec2(0.0F, 0.0F), ImWindowSize, col, 0.0f, ImDrawFlags_None);
		}

		// Connection popup
		if (m_ShowServerConnectWindow)
		{

			ImVec2 ImConnectWinSize { 400, 105 };
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
				{
					ImGui::Text("Failed to connect!");
				}

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
						if (Draw::NetEngine::Connect(Username, ServerAddress, PortNumber,
							[this]() {
								/* Failure Callback */
								m_FailedToConnect = true;
								m_IsConnecting = false;
							},
							[]() {
								/* Success Callback */
								Scene::Load(make_unique<FieldSetupScene>());
							}))
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
				ImGui::SameLine();
				if (ImGui::Button("[TEST] Without Server"))
				{
					PointComponent FieldDimensions { 12, 12 };
					ShipCount MaxShipsOnField { 2, 2, 2, 2, 2 };
					GameManager2::CreateObject(FieldDimensions, MaxShipsOnField);
					Scene::Load(make_unique<FieldSetupScene>());
				}
				ImGui::EndDisabled();

			ImGui::EndChild();
		}

		ImGui::End();

	}

private:

	char m_InputUsername[32] { "" };
	char m_InputAddress[64] { "" };
	char m_InputPort[6] { "" };
	bool m_ShowServerConnectWindow { false };
	bool m_IsConnecting { false };
	bool m_FailedToConnect { false };

};