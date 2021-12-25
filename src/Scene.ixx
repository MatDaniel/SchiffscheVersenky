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
    static void load(std::unique_ptr<Scene>&& next) noexcept;

    /**
     * @retval The current scene.
     *         Note: Might be null, if run wasn't called yet.
     */
    static Scene* current() noexcept;

    /**
     * @brief Loads the next scene if any is in queue.
     */
    static void next() noexcept;

    /**
     * @brief Destroys the current scene.
     */
    static void cleanUp() noexcept;

    // Callbacks

    /**
     * @brief Called at initialization.
     */
    virtual void onInit() = 0;

    /**
     * @brief Is called every frame.
     */
    virtual void onDraw() = 0;

    /**
     * @brief Window resize callback.
     */
    virtual void onWindowResize() { };

    /**
     * @brief Window cursor moved callback.
     */
    virtual void onCursorMoved() { };

};

// Management Implementation

// Properties
static std::unique_ptr<Scene> s_current;
static std::unique_ptr<Scene> s_next;

void Scene::load(std::unique_ptr<Scene>&& next) noexcept
{
	s_next = std::move(next);
}

Scene* Scene::current() noexcept
{
    return s_current.get();
}

void Scene::next() noexcept
{
	if (s_next.get())
	{
		s_current = std::move(s_next);
        s_current->onInit();
	}
}

void Scene::cleanUp() noexcept
{
	s_next.release();
	s_current.release();
}