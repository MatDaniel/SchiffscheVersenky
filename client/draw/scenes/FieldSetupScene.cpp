#include "FieldSetupScene.hpp"

#include <glad/glad.h>

void FieldSetupScene::update()
{

    // Clear screen
    //--------------

    glClearColor(0.8F, 0.2F, 0.1F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

}