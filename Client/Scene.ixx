module;

#include <memory>
#include <glm/mat4x4.hpp>

export module Draw.Scene;

export class Scene
{
public:

    /**
     * @brief This creates a new instance of a scene.
     * @retval A pointer to an instance of a scene.
     */
    typedef std::unique_ptr<Scene>(*GetterFunc)();

    /**
     * @brief Creates a scene getter function,
     *        that can be used to hand over the scene
     *        with the intention to load it later.
     * @tparam T The type of scene to return.
     * @retval A function that returns a new instance
     *         of the specified scene type.
     */
    template <class T>
    inline static GetterFunc getter()
    {
        return []() -> std::unique_ptr<Scene> {
            return { std::make_unique<T>() };
        };
    }

    /**
     * @brief Loads a scene.
     *        Note: Ensure that the game is running,
     *              otherwise the scene might not properly load
     *              and will be replaced with the entry scene at run.
     * @param next The scene to load at the next update.
     */
    static void load(GetterFunc next) noexcept;

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
    static void release() noexcept;

    /**
     * @brief Updates the scene. Is called every frame.
     */
    virtual void update() = 0;

    /**
     * @brief Window resize callback.
     */
    virtual void onWindowResize() = 0;

};

static std::unique_ptr<Scene> s_scene; // The current loaded scene
static Scene::GetterFunc s_next; // The next scene to load

inline void Scene::load(GetterFunc next) noexcept
{
	s_next = next;
}

inline Scene* Scene::current() noexcept
{
	return s_scene.get();
}

inline void Scene::next() noexcept
{
    if (s_next)
    {
        s_scene = s_next();
        s_next = nullptr;
    }
}

inline void Scene::release() noexcept
{
    s_scene.release();
}