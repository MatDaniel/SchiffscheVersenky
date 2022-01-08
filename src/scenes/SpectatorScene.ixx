module;

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <GLFW/glfw3.h>

#include <string>
#include <memory>

export module Scenes.Spectator;
import Draw.Scene;
import Draw.Render;
import Draw.Renderer;
import Draw.Resources;
import Draw.Window;
import Game.GameField;
import GameManagement;
import Network.Client;

namespace Draw::Scenes
{

	using namespace std;
	using namespace glm;
	using namespace Draw;
	using namespace Draw::Render;

	constexpr float ScreenScale = 0.85F;

	export class SpectatorScene final : public Scene
	{
	public:

		SpectatorScene(GameField* GameField)
			: m_GameField(GameField)
		{
		}

		void Init_UpdateProjection()
		{
			m_SceneData.UploadProjection();
			m_OutWidth = m_GameField->RecalculateOceanWithMidLane(Window::Properties::FrameBuffer, 3.0F, 0.05F) + 1.0F;
		}

		void OnInit() override
		{
			// Set view and projection properties.
			m_SceneData.Clear = {
				.Color = { 0.8F, 0.2F, 0.1F, 1.0F }
			};
			m_SceneData.Camera = {
				.Position = { -0.4F, 1.1F, 0.0F },
				.Direction = { 0.3F, -1.0F, 0.0F },
				.Up = { 1.0F, 0.0F, 0.0F }
			};
			m_SceneData.Projection = SceneData::PerspectiveProjectionInfo{
				.FieldOfView = radians(60.0F)
			};

			// Upload camera
			m_SceneData.UploadCamera();
			Init_UpdateProjection();
			

		}

		void OnDraw() override
		{

			m_SceneData.BeginFrame();
			
				m_GameField->DrawOcean(m_SceneRenderer, m_FieldTransform[0]);
				m_GameField->DrawBackground(m_SceneRenderer, m_FieldTransform[0]);
				m_GameField->DrawPlacedShips(m_SceneRenderer, m_FieldTransform[0]);
				m_GameField->Game_DrawSquares(m_SceneRenderer, m_FieldTransform[0]);
				m_GameField->SwapFieldTargets(-1.025F, m_OutWidth);

				m_GameField->DrawOcean(m_SceneRenderer, m_FieldTransform[1]);
				m_GameField->DrawBackground(m_SceneRenderer, m_FieldTransform[1]);
				m_GameField->DrawPlacedShips(m_SceneRenderer, m_FieldTransform[1]);
				m_GameField->Game_DrawSquares(m_SceneRenderer, m_FieldTransform[1]);
				m_GameField->SwapFieldTargets(1.025F, -m_OutWidth);

			m_SceneRenderer.Render();

		}

		void OnWindowResize() override
		{
			Init_UpdateProjection();
		}

	private:

		GameField* m_GameField;
		SceneData m_SceneData{ &Window::Properties::FrameBuffer };
		Renderer m_SceneRenderer{ &Window::Properties::FrameBuffer };

		float m_OutWidth;

		mat4 m_FieldTransform[2] {
			glm::translate(mat4(1.0F), vec3(0.0F, 0.0F, -0.525F)),
			glm::translate(mat4(1.0F), vec3(0.0F, 0.0F, 0.525F))
		};

	};

}