#include "PlayScene.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../draw/Game.hpp"
#include "../draw/SceneRenderer.hpp"
#include "../draw/res/Models.hpp"

PlayScene::PlayScene()
{

    auto& ren = *Game::renderer();
    auto& info = ren.info();
    
    // Set camera orientation
    info.camera = {
        .position = { -5.0F, 10.0F, 0.0F },
        .direction = { 0.25F, -1.0F, 0.0F },
        .up = { 1.0F, 0.0F, 0.0F }
    };
    
    // Set projection
    info.projection = PerspectiveProjection();

    // Upload data to the scene ubo
    ren.uploadCamera();
    ren.uploadProjection();

}

void PlayScene::update()
{

    // Clear screen
    //--------------

    glClearColor(0.8F, 0.2F, 0.1F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Game::renderer()->draw(
        glm::translate(glm::mat4(1.0F), glm::vec3(0.0F, 0.0F, 0.0F)), // Position
        Models::water);

}