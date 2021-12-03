#ifndef SHIFVRSKY_SCENE_HPP
#define SHIFVRSKY_SCENE_HPP

#include <memory>
#include <glm/mat4x4.hpp>

class Scene
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
     * @tparam T The scene to load.
     */
    template <class T>
    static inline void load()
    {
        load(std::make_unique<T>());
    }

    /**
     * @brief Loads a scene.
     *        Note: Ensure that the game is running,
     *              otherwise the scene might not properly load
     *              and will be replaced with the entry scene at run.
     * @param scene The scene to load.
     */
    static void load(std::unique_ptr<Scene>&& scene) noexcept;

    /**
     * @retval The current scene.
     *         Note: Might be null, if run wasn't called yet.
     */
    static Scene *current() noexcept;

    /**
     * @brief Updates the scene. Is called every frame.
     */
    virtual void update();

};

#endif // SHIFVRSKY_SCENE_HPP