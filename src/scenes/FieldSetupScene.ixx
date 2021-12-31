module;

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <GLFW/glfw3.h>

#include <memory>
#include <string>
#include <utility>

export module Scenes.FieldSetup;
import Draw.Window;
import Draw.Scene;
import Draw.Engine;
import Draw.Render;
import Draw.Resources;
import Draw.Renderer;
import Game.GameField;
import Game.ShipInfo;
import Network.Client;
import GameManagment;
import Scenes.Game;

using namespace GameManagment;
using namespace Network;
using namespace std;
using namespace Draw;

export class FieldSetupScene final : public Scene
{
public:

	void OnInit() override
	{

		/* Setup game field */

		auto& ManagerInstance = GameManager2::GetInstance();
		auto* PlayerGameField = ManagerInstance.TryAllocatePlayerWithId(1);
		m_GameField = make_unique<GameField>(ManagerInstance, PlayerGameField);

		/* Setup scene rendering */

		// Set view and projection properties.
		auto& info = m_Renderer.info();
		info.camera = {
			.position = { 0.0F, 10.0F, 8.0F },
			.direction = { 0.0F, -1.0F, 0.0F },
			.up = { 1.0F, 0.0F, 0.0F }
		};
		info.projection = OrthoProjection{
			.height = 22.0F
		};

		// Upload scene data
		m_Renderer.uploadCamera();
		m_Renderer.uploadProjection(Window::Properties::FrameBuffer);
		
		// Setup game field
		m_GameField->SetTransform(glm::scale(glm::mat4(1.0F), glm::vec3(21.0F, 21.0F, 21.0F)));

	}

	void OnDraw() override
	{

		// Prepare framebuffer for rendering
		Window::Properties::FrameBuffer.Select();
		glClearColor(0.8F, 0.2F, 0.1F, 1.0F);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Render game field
		m_GameField->DrawBackground(m_Renderer);
		m_GameField->DrawPlacedShips(m_Renderer);
		m_GameField->SetupPhase_PreviewPlacement(m_Renderer, (ShipType)m_SelectedType);
		m_GameField->SetupPhase_DrawSquares(m_Renderer);
		m_Renderer.render();

		// Render GUI
		ImGui::SetNextWindowPos(ImVec2 { 600, 10 }, ImGuiCond_Once, ImVec2 { 0, 0 });
		ImGui::Begin("GameField Debug", nullptr, ImGuiWindowFlags_NoSavedSettings);
		ImGui::Combo("Ship Type", &m_SelectedType, [](void* data, int idx, const char** out) -> bool {
			*out = ShipInfos[idx].name;
			return true;
			}, nullptr, ShipTypeCount);
		if (ImGui::Button("Done"))
		{
			Scene::Load(make_unique<Draw::Scenes::GameScene>(move(m_GameField)));
		}
		ImGui::End();

	}

	void OnWindowResize() override
	{
		m_Renderer.uploadProjection(Window::Properties::FrameBuffer);
	}

	void OnMouseButton(int button, int action, int mods) override
	{
		if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS)
		{
			m_GameField->SetupPhase_PlaceShip(m_Renderer, (ShipType)m_SelectedType);
		}
	}

private:
	
	// Renderer
	SceneRenderer m_Renderer;

	// Game Field
	unique_ptr<GameField> m_GameField;	

	// Debug
	int m_SelectedType = 0;
	
};