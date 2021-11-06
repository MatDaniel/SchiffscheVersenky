#ifndef SHIFVRSKY_GAME_HPP
#define SHIFVRSKY_GAME_HPP

#include "Scene.hpp"

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
     * @brief Setzt eine neue Szene für das Spiel fest.
     *        Achtung: `nullptr` ist nicht erlaubt.
     * @param scene Die neue Szene des Spiels.
     */
    void scene(Scene*);

    /**
     * @retval Die zurzeit ausgewählte Szene.
     */
    Scene *scene();

}

#endif // SHIFVRSKY_GAME_HPP