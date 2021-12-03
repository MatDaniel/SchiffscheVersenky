#ifndef SHIFVRSKY_PLAYSCENE_HPP
#define SHIFVRSKY_PLAYSCENE_HPP

#include "draw/Scene.hpp"

class PlayScene final : public Scene
{
public:

    PlayScene();

    void update() override;

};

#endif // SHIFVRSKY_PLAYSCENE_HPP