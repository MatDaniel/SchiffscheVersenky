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
import Draw.Resources;
import Draw.Renderer;
import Game.GameField;
import Game.ShipInfo;
import Network.Client;
import GameManagment;

using namespace GameManagment;
using namespace Network;
using namespace std;
using namespace Draw;

export class FieldSetupScene final : public Scene
{
public:

	void OnInit() override
	{

		auto& manager = GameManager2::CreateObject(PointComponent{4,4}, ShipCount{2,2,2,2,2});
		gmGameField = manager.TryAllocatePlayerWithId(1);
		gameField = make_unique<GameField>(manager, gmGameField);

		auto& info = renderer.info();

		// Set camera orientation
		info.camera = {
			.position = { 0.0F, 10.0F, 8.0F },
			.direction = { 0.0F, -1.0F, 0.0F },
			.up = { 1.0F, 0.0F, 0.0F }
		};

		// Set projection
		info.projection = OrthoProjection{
			.height = 22.0F
		};

		// Upload data to the scene ubo
		renderer.uploadCamera();
		renderer.uploadProjection();

		// Setup game field
		gameField->setTransform(glm::scale(glm::mat4(1.0F), glm::vec3(21.0F, 21.0F, 21.0F)));

	}

	void OnDraw() override
	{
		// Clear screen
		//--------------

		glClearColor(0.8F, 0.2F, 0.1F, 1.0F);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ImGui::SetNextWindowPos(ImVec2 { 600, 10 });
		ImGui::Begin("GameField Debug", nullptr, ImGuiWindowFlags_NoSavedSettings);
		ImGui::Combo("Ship Type", &selectedType, [](void* data, int idx, const char** out) -> bool {
			*out = ShipInfos[idx].name;
			return true;
			}, nullptr, ShipTypeCount);
		ImGui::End();

		gameField->DrawBackground(renderer);
		gameField->DrawPlacedShips(renderer);
		gameField->SetupPhase_PreviewPlacement(renderer, (ShipType)selectedType);
		gameField->SetupPhase_DrawSquares(renderer);
		renderer.render();

	}

	void OnWindowResize() override
	{
		renderer.uploadCamera();
		renderer.uploadProjection();
	}

	void OnMouseButton(int button, int action, int mods) override
	{
		if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS)
		{
			gameField->SetupPhase_PlaceShip(renderer, (ShipType)selectedType);
		}
	}

	~FieldSetupScene()
	{
		GameManager2::ManualReset();
	}

private:

	// Renderer
	SceneRenderer renderer;

	// Game Field
	GmPlayerField* gmGameField;
	unique_ptr<GameField> gameField;	

	// Debug
	int selectedType = 0;
	
};