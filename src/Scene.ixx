module;

#include <memory>
#include <glm/mat4x4.hpp>

export module Draw.Scene;

export class Scene
{
public:

    // Management

    /**
     * @brief Loads a scene.
     *        Note: Ensure that the game is running,
     *              otherwise the scene might not properly load
     *              and will be replaced with the entry scene at run.
     * @param next The scene to load at the next update.
     */
    static void Load(std::unique_ptr<Scene>&& next) noexcept;

    /**
     * @retval The current scene.
     *         Note: Might be null, if run wasn't called yet.
     */
    static Scene* Current() noexcept;

    /**
     * @brief Loads the next scene if any is in queue.
     */
    static void Next() noexcept;

    /**
     * @brief Destroys the current scene.
     */
    static void CleanUp() noexcept;

    // Callbacks

    /**
     * @brief Called at initialization.
     */
    virtual void OnInit() { };

    /**
     * @brief Is called every frame.
     */
    virtual void OnDraw() = 0;

    /**
     * @brief Window resize callback.
     */
    virtual void OnWindowResize() { };

    /**
     * @brief Window cursor moved callback.
     */
    virtual void OnCursorMoved() { };

    /**
     * @brief Keyboard key callback.
     */
    virtual void OnKeyboardKey(int key, int action, int mods) { };

    /**
     * @brief Mouse button callback.
     */
    virtual void OnMouseButton(int key, int action, int mods) { };

};

// Management Implementation

// Properties
static std::unique_ptr<Scene> s_current;
static std::unique_ptr<Scene> s_next;

void Scene::Load(std::unique_ptr<Scene>&& next) noexcept
{
	s_next = std::move(next);
}

Scene* Scene::Current() noexcept
{
    return s_current.get();
}

void Scene::Next() noexcept
{
	if (s_next.get())
	{
		s_current = std::move(s_next);
        s_current->OnInit();
	}
}

void Scene::CleanUp() noexcept
{
	s_next.release();
	s_current.release();
}