#ifndef SHIFVRSKY_GAME_HPP
#define SHIFVRSKY_GAME_HPP

#include <glm/vec2.hpp>

#include "Scene.hpp"

namespace Game
{

    /**
     * @brief Initializes the game and executes the game loop.
     * @param initalSceneGetter The entry scene getter, loads the returned scene.
     * @retval The error code with which the game ended.
     */
    int run(Scene::GetterFunc initalSceneGetter);

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
    const glm::uvec2 &windowSize() noexcept;

}

#endif // SHIFVRSKY_GAME_HPP