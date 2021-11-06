#ifndef SHIFVRSKY_GAME_HPP
#define SHIFVRSKY_GAME_HPP

#include <entt/entity/registry.hpp>

namespace Game
{

    /**
     * @brief Initialisiert das Spiel und führt die Game-Loop aus.
     * @retval Der Fehlercode, mit dem das Spiel beendet wurde. 
     */
    int run();

    /**
     * @brief Beantragt die Beendendigung des Spiels.
     *        Das Spiel wird erst zu Beginn des nächsten Frames beendet.
     * @param code Der Fehlercode, mit dem das Spiel beendet werden soll.
     */
    void exit(int code = 0);

    /**
     * @retval Gibt die Welt mit den Objekten zurück.
     */
    entt::registry &world();

}

#endif // SHIFVRSKY_GAME_HPP