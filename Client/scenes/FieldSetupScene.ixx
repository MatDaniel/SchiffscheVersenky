module;

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>

export module Scenes.FieldSetup;

import Draw.Scene;
import Draw.Engine;
import Draw.Resources;
import Draw.Renderer;

export class FieldSetupScene final : public Scene
{
public:

	inline FieldSetupScene()
		: renderer()
		, waterModel(Resources::find<Model>("Water"))
	{

		auto& info = renderer.info();

		// Set camera orientation
		info.camera = {
			.position = { 0.0F, 10.0F, 8.0F },
			.direction = { 0.0F, -1.0F, 0.0F },
			.up = { 1.0F, 0.0F, 0.0F }
		};

		// Set projection
		info.projection = OrthoProjection {
			.height = 22.0F
		};

		// Upload data to the scene ubo
		renderer.uploadCamera();
		renderer.uploadProjection();

	}

	void update() override
	{
		// Clear screen
		//--------------

		glClearColor(0.8F, 0.2F, 0.1F, 1.0F);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		renderer.draw(
			glm::translate(glm::mat4(1.0F), glm::vec3(0.0F, 0.0F, 0.0F)), // Position
			waterModel);

		renderer.render();

	}

	void onWindowResize() override
	{
		renderer.uploadCamera();
		renderer.uploadProjection();
	}

private:

	SceneRenderer renderer;

	// Resources
	Model* waterModel;
	
};