module;

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <string>

export module Scenes.FieldSetup;
import Draw.Window;
import Draw.Scene;
import Draw.Engine;
import Draw.Resources;
import Draw.Renderer;
import Game.GameField;

export class FieldSetupScene final : public Scene
{
public:

	void onInit() override
	{

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
		gameField.setTransform(glm::scale(glm::mat4(1.0F), glm::vec3(21.0F, 0.0F, 21.0F)));
		gameField.color(gameField.index(3, 3)) = glm::vec4(1.0F, 0.0F, 0.0F, 1.0F);
		gameField.color(gameField.index(9, 9)) = glm::vec4(0.0F, 1.0F, 0.0F, 1.0F);
		gameField.color(gameField.index(12, 3)) = glm::vec4(0.0F, 0.0F, 0.0F, 1.0F);

	}

	void onDraw() override
	{
		// Clear screen
		//--------------

		glClearColor(0.8F, 0.2F, 0.1F, 1.0F);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		gameField.draw(renderer);
		renderer.render();

		ImGui::Begin("GameField Debug", nullptr, ImGuiWindowFlags_NoSavedSettings);
		auto selected = gameField.cursorPos(renderer);
		ImGui::Text("Selected Field: { %f ; %f }", selected.x, selected.y);
		ImGui::End();

	}

	void onWindowResize() override
	{
		renderer.uploadCamera();
		renderer.uploadProjection();
	}

private:

	SceneRenderer renderer;

	// Resources
	GameField gameField { 12, 12 };
	
};