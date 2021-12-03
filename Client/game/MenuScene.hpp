#ifndef SHIFVRSKY_MENUSCENE_HPP
#define SHIFVRSKY_MENUSCENE_HPP

#include "../draw/Scene.hpp"

class MenuScene final : public Scene
{
public:

    MenuScene();

    void update() override;

private:

    // gui values
    char m_username[32] = "Player";
    char m_address[64] = "127.0.0.1";
    bool m_serverConnectionWindow = false;

};

#endif // SHIFVRSKY_MENUSCENE_HPP