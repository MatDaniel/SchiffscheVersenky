#ifndef SHIFVRSKY_GAME_HPP
#define SHIFVRSKY_GAME_HPP

#include <entt/entity/registry.hpp>

namespace Game
{

    /**
     * @brief Initializes the game and executes the game loop.
     * @retval The error code with which the game ended.
     */
    int run();

    /**
     * @brief Requests the termination of the game.
     *        Note: The game will not end until the beginning of the next frame.
     * @param code The error code with which to end the game.
     */
    void exit(int code = 0);

    /**
     * @retval The enity world.
     */
    entt::registry &world();

}

#endif // SHIFVRSKY_GAME_HPP