module;

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <imgui.h>
#include <GLFW/glfw3.h>

#include <memory>
#include <string>
#include <utility>

export module Scenes.FieldSetup;
import Draw.Window;
import Draw.Scene;
import Draw.Render;
import Draw.Resources;
import Draw.Renderer;
import Game.GameField;
import Game.ShipInfo;
import Network.Client;
import GameManagement;
import DispatchRoutine;
import Game.ShipInfo;
import Scenes.Game;

import Draw.NetEngine.Callbacks;

namespace Draw::Scenes
{

	using namespace std;
	using namespace glm;
	using namespace Draw;
	using namespace Network;
	using namespace Network::Client;
	using namespace GameManagement;

	export class FieldSetupScene final : public Scene
	{
	public:

		FieldSetupScene(GameField* GameField, bool WithoutServer = false)
			: m_GameField(GameField)
			, m_WithoutServer(WithoutServer)
		{
		}

		void Init_UpdateProjection()
		{

			// Upload projection
			m_SceneData.UploadProjection();

			// Recalculate ocean
			m_GameField->RecalculateOcean(Window::Properties::FrameBuffer, 1.1F, -0.4F);

		}

		void OnInit() override
		{

			/* Setup game field */

			m_GameField->SetupPhase_UpdateState();

			/* Setup scene rendering */

			// Set view and projection properties.
			m_SceneData.Clear = {
				.Color = { 0.8F, 0.2F, 0.1F, 1.0F }
			};
			m_SceneData.Camera = {
				.Position = { 0.0F, 10.0F, 0.4F },
				.Direction = { 0.0F, -1.0F, 0.0F },
				.Up = { 1.0F, 0.0F, 0.0F }
			};
			m_SceneData.Projection = SceneData::OrthoProjectionInfo{
				.Height = 1.1F
			};

			// Upload scene data
			m_SceneData.UploadCamera();

			// Update projection
			Init_UpdateProjection();

			/* Register Callbacks */
			Draw::NetEngine::Callbacks::OnGameStart = [this]() {
				Scene::Load(make_unique<GameScene>(m_GameField));
			};

		}

		void OnDraw() override
		{

			/* Render game field */

			m_SceneData.BeginFrame();
			{
				m_GameField->SetupPhase_PrepareCursor(m_SceneData);
				m_GameField->DrawOcean(m_SceneRenderer);
				m_GameField->DrawBackground(m_SceneRenderer);
				m_GameField->DrawPlacedShips(m_SceneRenderer);
				m_GameField->SetupPhase_DrawSquares(m_SceneRenderer);
				if (!m_GameField->SetupPhase_IsFinished())
				{
					m_GameField->SetupPhase_PreviewPlacement(m_SceneRenderer);
					m_GameField->SetupPhase_ShipSelection(m_SceneRenderer, m_SceneData, &m_SelectNextShip);
				}
			}
			m_SceneRenderer.Render();

			/* Draw GUI */

			ImGui::SetNextWindowPos(ImVec2(600, 10), ImGuiCond_Once, ImVec2());
			ImGui::Begin("Field Setup", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavInputs);
				ImGui::BeginDisabled(!m_GameField->SetupPhase_IsFinished() || m_IsReady || m_IsReadyWait);
				if (ImGui::Button("Ready"))
				{
					if (!m_WithoutServer)
					{
						m_IsReadyWait = true;
						m_ReadyFail = false;
					}
					else
						Scene::Load(make_unique<GameScene>(m_GameField));
				}
				ImGui::EndDisabled();
				if (m_ReadyFail)
					ImGui::Text("Failed to ready up!");
				if (m_IsReady)
					ImGui::Text("Waiting for the other player...");
				
			ImGui::End();
			
			/* Ready Networking */

			if (m_IsReadyWait && !m_IsReady)
			{
				auto Response = ::Client::QueueServerReadyAndPoll();

				switch (Response)
				{
				case ::Client::QUE_READYED_UP:         // yea server granted us ready state :)
					m_IsReady = true;
					break;
				case ::Client::QUE_COULD_NOT_READY_UP: // Received but error
					m_IsReadyWait = false;
					m_ReadyFail = true;
					break;
				case ::Client::QUE_NOINFO:             // work pending, please wait nigga
				case ::Client::QUE_QUEUED_FOR_READY:   // this call resulted in the request being queued	
					break;
				default:                               // all cases are handled, this should never occur
					__debugbreak();
					break;
				}
			}

		}

		void OnWindowResize() override
		{

			// Update projection
			Init_UpdateProjection();

		}

		void OnMouseButton(int button, int action, int mods) override
		{
			if (action == GLFW_PRESS)
			{
				switch (button)
				{
				case GLFW_MOUSE_BUTTON_1:
					m_GameField->SetupPhase_PlaceShip();
					m_SelectNextShip = true;
					break;
				case GLFW_MOUSE_BUTTON_2:
					m_GameField->SetupPhase_DestroyShip();
					break;
				default:
					break;
				}
			}
		}

	private:

		// Click
		bool m_SelectNextShip { false };

		// General
		GameField* m_GameField;
		bool m_WithoutServer;
		bool m_IsReadyWait { false };
		bool m_IsReady { false };
		bool m_ReadyFail { false };

		// Renderer
		Draw::SceneData m_SceneData { &Window::Properties::FrameBuffer };
		Draw::Renderer m_SceneRenderer { &Window::Properties::FrameBuffer };

	};

}