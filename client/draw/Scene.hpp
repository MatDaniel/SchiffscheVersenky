#ifndef SHIFVRSKY_SCENE_HPP
#define SHIFVRSKY_SCENE_HPP

#include <memory>
#include <entt/core/hashed_string.hpp>

class Scene;

template <typename T>
concept SceneDerived = std::is_base_of_v<Scene, T>;

class Scene
{
public:

    /**
     * @brief Loads a scene.
     *        Note: Ensure that the game is running,
     *              otherwise the scene might not properly load
     *              and will be replaced with the entry scene at run.
     * @tparam T The scene to load.
     */
    template <SceneDerived T>
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
     * @brief Initializes the scene. Called when the scene is loaded.
     */
    virtual void init();

    /**
     * @brief Updates the scene. Is called every frame.
     */
    virtual void update();

};

#endif // SHIFVRSKY_SCENE_HPP