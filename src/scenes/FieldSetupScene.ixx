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
import Game.ShipInfo;
import Scenes.Game;

namespace Draw::Scenes
{

	using namespace std;
	using namespace glm;
	using namespace Draw;
	using namespace Network;
	using namespace GameManagement;

	export class FieldSetupScene final : public Scene
	{
	public:

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

			auto& ManagerInstance = GameManager2::GetInstance();
			auto* PlayerGameField = ManagerInstance.TryAllocatePlayerWithId(1);
			m_GameField = make_unique<GameField>(ManagerInstance, PlayerGameField);
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

		}

		void OnDraw() override
		{

			/* Render game field */

			m_SceneData.BeginFrame();
			{
				m_GameField->DrawOcean(m_SceneRenderer);
				m_GameField->DrawBackground(m_SceneRenderer);
				m_GameField->DrawPlacedShips(m_SceneRenderer);
				m_GameField->SetupPhase_DrawSquares(m_SceneRenderer);
				if (!m_GameField->SetupPhase_IsFinished())
				{
					m_GameField->SetupPhase_PreviewPlacement(m_SceneRenderer, m_SceneData);
					m_GameField->SetupPhase_ShipSelection(m_SceneRenderer, m_SceneData, m_WasLeftClicked);
				}
			}
			m_SceneRenderer.Render();

			// Reset single frame values
			m_WasLeftClicked = false;

			/* Draw GUI */

			ImGui::SetNextWindowPos(ImVec2(600, 10), ImGuiCond_Once, ImVec2());
			ImGui::Begin("Field Setup", nullptr, ImGuiWindowFlags_NoSavedSettings);
			{
				if (ImGui::Button("Done"))
					Scene::Load(make_unique<GameScene>(move(m_GameField)));
			}
			ImGui::End();

		}

		void OnWindowResize() override
		{

			// Update projection
			Init_UpdateProjection();

		}

		void OnMouseButton(int button, int action, int mods) override
		{
			if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS)
			{
				m_GameField->SetupPhase_PlaceShip(m_SceneData);
				m_WasLeftClicked = true;
			}
		}

	private:

		// Renderer
		Draw::SceneData m_SceneData { &Window::Properties::FrameBuffer };
		Draw::Renderer m_SceneRenderer { &Window::Properties::FrameBuffer };

		// Game Field
		unique_ptr<GameField> m_GameField;

		// Click
		bool m_WasLeftClicked{ false };

	};

}