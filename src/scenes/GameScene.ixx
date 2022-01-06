module;

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <GLFW/glfw3.h>

#include <string>
#include <memory>

export module Scenes.Game;
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

	export class GameScene final : public Scene
	{
	public:

		GameScene(GameField* GameField)
			: m_GameField(GameField)
		{
		}

		void Init_SceneRendering()
		{

			// Set view and projection properties.
			m_SceneData.Clear = {
				.Color = { 0.8F, 0.2F, 0.1F, 1.0F }
			};
			m_SceneData.Camera = {
				.Position = { -0.4F, 0.75F, 0.0F },
				.Direction = { 0.4F, -1.0F, 0.0F },
				.Up = { 1.0F, 0.0F, 0.0F }
			};
			m_SceneData.Projection = SceneData::PerspectiveProjectionInfo {
				.FieldOfView = radians(60.0F)
			};

			// Upload camera
			m_SceneData.UploadCamera();

		}

		void Init_SceneScreenRendering()
		{

			// Set view and projection properties.
			m_SceneScreenData.Clear = {
				.Color = { 0.0F, 0.0F, 0.0F, 0.0F }
			};
			m_SceneScreenData.Camera = {
				.Position = { 0.0F, 10.0F, 0.0F },
				.Direction = { 0.0F, -1.0F, 0.0F },
				.Up = { 1.0F, 0.0F, 0.0F }
			};
			m_SceneScreenData.Projection = SceneData::OrthoProjectionInfo {
				.Height = 1.0F
			};
			m_SceneScreenData.Depth = {
				.Comparison = GL_ALWAYS
			};

			// Upload camera
			m_SceneScreenData.UploadCamera();

		}

		void Init_ScreenRendering()
		{

			// Set view and projection properties.
			m_ScreenData.Clear = {
				.Bits = GL_COLOR_BUFFER_BIT
			};
			m_ScreenData.Camera = {
				.Position = { 0.0F, 10.0F, 0.0F },
				.Direction = { 0.0F, -1.0F, 0.0F },
				.Up = { 1.0F, 0.0F, 0.0F }
			};
			m_ScreenData.Projection = SceneData::OrthoProjectionInfo {
				.Height = 1.1F
			};
			m_ScreenData.Depth = {
				.Enabled = false
			};

			// Upload camera
			m_ScreenData.UploadCamera();

		}

		void Init_UpdateProjection()
		{

			// Update default renderer
			m_SceneData.UploadProjection();
			m_SceneScreenData.UploadProjection();

			// Update screen frame buffer & renderer
			auto WindowSize = Window::Properties::WindowSize;
			auto ScreenEdge = WindowSize.y * (1.0F - ScreenScale);
			auto ScreenSize = ceil(vec2(WindowSize.x - ScreenEdge, WindowSize.y - ScreenEdge));
			m_ScreenTarget.Resize(ScreenSize.x, ScreenSize.y);
			m_ScreenData.UploadProjection();
			m_ScreenMaterial.Texture = &m_ScreenTarget.UndelyingTexture(); // In case the texture has changed
			auto WidthScale = ScreenSize.x / ScreenSize.y;
			m_GameField->RecalculateOcean(Window::Properties::FrameBuffer, 1.215F);
			m_ScreenTransformation = scale(mat4(1.0F), vec3(
				ScreenScale,
				0.0F,
				ScreenScale * WidthScale
			));
			
		}

		void OnInit() override
		{
			m_GameField->Game_UpdateState();
			Init_ScreenRendering();
			Init_SceneRendering();
			Init_SceneScreenRendering();
			Init_UpdateProjection();
		}

		void OnDraw() override
		{

			/* Draw on the virtual screen */

			if (m_IsScreenOpen)
			{
				m_ScreenData.BeginFrame();
				{
					m_GameField->Enemy_DrawBackground(m_ScreenBackgroundRenderer);
				}
				m_ScreenBackgroundRenderer.Render();
				{
					m_GameField->Enemy_UpdateCursorPosition(m_SceneScreenData, m_ScreenData, m_ScreenTransformation);
					m_GameField->Enemy_DrawHits(m_ScreenForegroundRenderer);
					m_GameField->Enemy_Selection(m_ScreenForegroundRenderer);
					m_GameField->Enemy_DrawShips(m_ScreenForegroundRenderer);
				}
				m_ScreenForegroundRenderer.Render();
			}

			/* Draw on the window */

			m_SceneData.BeginFrame();
			{
				m_GameField->DrawOcean(m_SceneRenderer);
				m_GameField->DrawBackground(m_SceneRenderer);
				m_GameField->DrawPlacedShips(m_SceneRenderer);
				m_GameField->Game_DrawSquares(m_SceneRenderer);
			}
			m_SceneRenderer.Render();

			if (m_IsScreenOpen)
			{
				m_SceneScreenData.BeginFrame();
				{
					m_SceneScreenRenderer.Draw(
						m_ScreenTransformation,
						m_ScreenTarget.PlaneMesh().VertexArray(),
						TextureFrameBuffer::PlaneRange,
						m_ScreenMaterial
					);
					m_SceneScreenRenderer.Draw(m_ScreenTransformation, m_ScreenFrame);
				}
				m_SceneScreenRenderer.Render();
			}

		}

		void OnWindowResize() override
		{
			Init_UpdateProjection();
		}

		void OnKeyboardKey(int key, int action, int mods) override
		{
			if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
			{
				m_IsScreenOpen = !m_IsScreenOpen;
			}
		}

		void OnMouseButton(int button, int action, int mods) override
		{
			if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_1 && m_IsScreenOpen)
			{
				m_GameField->Enemy_Hit();
			}
		}

	private:

		GameField* m_GameField;

		TextureFrameBuffer m_ScreenTarget{ TextureLayout(1, 1, 3).WrapRepeat().FilterNearest() };
		SceneData m_ScreenData { &m_ScreenTarget };
		Renderer m_ScreenBackgroundRenderer { &m_ScreenTarget };
		Renderer m_ScreenForegroundRenderer { &m_ScreenTarget };
		
		SceneData m_SceneData { &Window::Properties::FrameBuffer };
		Renderer m_SceneRenderer { &Window::Properties::FrameBuffer };
		SceneData m_SceneScreenData { &Window::Properties::FrameBuffer };
		Renderer m_SceneScreenRenderer { &Window::Properties::FrameBuffer };

		ConstModel* m_ScreenFrame { Resources::find<ConstModel>("ScreenFrame") };
		Material m_ScreenMaterial { *Resources::find<Material>("Screen_PostProcessing") };
		mat4 m_ScreenTransformation;

		bool m_IsScreenOpen { false };

	};

}