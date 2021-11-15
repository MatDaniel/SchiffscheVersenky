#ifndef SHIFVRSKY_MENUSCENE_HPP
#define SHIFVRSKY_MENUSCENE_HPP

#include "draw/Scene.hpp"
#include "draw/gobj/Camera.hpp"
#include "draw/gobj/SunLight.hpp"

class MenuScene final : public Scene
{
public:

    void update() override;

private:

    // gui values
    char m_username[32] = "Player";
    char m_address[64] = "127.0.0.1";
    bool m_serverConnectionWindow = false;

    // scene values
    PerspectiveCamera m_camera;
    SunLight m_light;

};

#endif // SHIFVRSKY_MENUSCENE_HPP