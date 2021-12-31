module;

#include <glad/glad.h>

#include <memory>

export module Scenes.Game;
import Draw.Scene;
import Draw.Render;
import Draw.Renderer;
import Draw.Window;
import Game.GameField;
import GameManagment;
import Network.Client;

namespace Draw::Scenes
{

	using namespace std;
	using namespace Draw;

	export class GameScene final : public Scene
	{
	public:

		GameScene(unique_ptr<GameField>&& gameField)
			: m_GameField(move(gameField))
		{
		}

		void OnDraw() override
		{

			/* Draw on the virtual screen */

			m_ScreenTarget.Select();
			glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
			glClear(GL_COLOR_BUFFER_BIT);

			m_GameField->Enemy_DrawBackground(m_Renderer);
			m_Renderer.render();

			/* Draw on the window */

			Window::Properties::FrameBuffer.Select();
			glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
			glClear(GL_COLOR_BUFFER_BIT);

		}

	private:

		unique_ptr<GameField> m_GameField;
		Render::TextureFrameBuffer m_ScreenTarget;
		SceneRenderer m_Renderer;

	};

}