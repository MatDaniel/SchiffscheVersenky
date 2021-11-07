#ifndef SHIFVRSKY_GAME_HPP
#define SHIFVRSKY_GAME_HPP

#include <glm/vec2.hpp>

#include "Scene.hpp"

namespace Game
{

    /**
     * @brief Initializes the game and executes the game loop.
     * @param scene The entry scene to load.
     * @retval The error code with which the game ended.
     */
    int run(std::unique_ptr<Scene> &&scene);

    /**
     * @brief Initializes the game and executes the game loop.
     * @tparam T The entry scene to load.
     * @retval The error code with which the game ended.
     */
    template <SceneDerived T>
    inline int run()
    {
        return run(std::make_unique<T>());
    }

    /**
     * @brief Requests the termination of the game.
     *        Note: The game will not end until the beginning of the next frame.
     * @param code The error code with which to end the game.
     */
    void exit(int code = 0) noexcept;

    /**
     * @retval The time between the previous and current frame in seconds.
     */
    float deltaTime() noexcept;

    /**
     * @retval The size of the current viewport and window.
     */
    glm::uvec2 windowSize() noexcept;

}

#endif // SHIFVRSKY_GAME_HPP