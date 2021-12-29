module;

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <GLFW/glfw3.h>

#include <memory>
#include <string>
#include <utility>

export module Scenes.Menu;
import Draw.Window;
import Draw.Scene;
import Draw.Engine;
import Draw.Resources;
import Draw.Renderer;
import Network.Client;
import GameManagment;
import Scenes.FieldSetup;

using namespace GameManagment;
using namespace Network;
using namespace std;

export class MenuScene final : public Scene
{
public:

	void OnDraw() override
	{
		// Clear screen
		//--------------

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

			ImVec2 ImConnectWinSize { 400, 82 };
			ImVec2 ImConnectWinPos {
				(ImWindowSize.x - ImConnectWinSize.x) / 2,
				(ImWindowSize.y - ImConnectWinSize.y) / 2
			};

			ImGui::SetNextWindowPos(ImConnectWinPos);
			ImGui::BeginChild("Connect", ImConnectWinSize, true, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);

				// Connection form
				ImGui::InputText("Username", m_InputUsername, IM_ARRAYSIZE(m_InputUsername));
				ImGui::InputText("Address", m_InputAddress, IM_ARRAYSIZE(m_InputAddress));
				
				// Buttons
				if (ImGui::Button("Connect"))
				{
					// TODO: Connect to the server
					Scene::Load(std::make_unique<FieldSetupScene>());
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					m_ShowServerConnectWindow = false;
				}

			ImGui::EndChild();
		}

		ImGui::End();

	}

private:

	char m_InputUsername[32] { "Player" };
	char m_InputAddress[64] { "127.0.0.1" };
	bool m_ShowServerConnectWindow { false };

};