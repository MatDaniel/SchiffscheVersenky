#include "MenuScene.hpp"

#include <imgui.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "draw/Game.hpp"
#include "draw/res/Models.hpp"
#include "draw/res/Materials.hpp"
#include "draw/SceneRenderer.hpp"
#include "FieldSetupScene.hpp"

MenuScene::MenuScene()
{

    auto& ren = *Game::renderer();
    auto& info = ren.info();
    
    // Set camera orientation
    info.camera = {
        .position = { -0.3F, -0.3F, 1.0F },
        .direction = { 6.0F, 6.0F, -17.8F },
        .up = { 0.0F, 1.0F, 0.0F }
    };
    
    // Set projection
    info.projection = PerspectiveProjection();

    // Upload data to the scene ubo
    ren.uploadCamera();
    ren.uploadProjection();
    
}

void MenuScene::update()
{

    // Clear screen
    //--------------

    glClearColor(0.2F, 0.4F, 0.9F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Main menu
    //-----------

    auto windowSize = Game::windowSize();
    auto imWindowSize = ImVec2(static_cast<float>(windowSize.x), static_cast<float>(windowSize.y));
    ImGui::SetNextWindowSize(imWindowSize);
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    
    ImGui::Begin("Main Menu", nullptr,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);

        // Style
        ImGui::PopStyleVar(2);

        // Buttons
        ImGui::BeginDisabled(m_serverConnectionWindow);
        if (ImGui::Button("Play"))
            m_serverConnectionWindow = true;
        if (ImGui::Button("Quit"))
            Game::exit();
        ImGui::EndDisabled();

        // Darken the scene while the connection popup is open
        if (m_serverConnectionWindow)
        {
            auto drawList = ImGui::GetWindowDrawList();
            auto col = ImColor(0.0F, 0.0F, 0.0F, 0.5F);
            drawList->AddRectFilled(ImVec2(0.0F, 0.0F), imWindowSize, col, 0.0f, ImDrawFlags_None); 
        }

    ImGui::End();

    // Connection popup
    //-------------------

    if (m_serverConnectionWindow)
    {
        ImGui::Begin("Connect", &m_serverConnectionWindow,
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);

            // Close the window on focus loss
            if (!ImGui::IsWindowFocused())
            {
                m_serverConnectionWindow = false;
            }

            // Connection form
            ImGui::InputText("Username", m_username, IM_ARRAYSIZE(m_username));
            ImGui::InputText("Address", m_address, IM_ARRAYSIZE(m_address));
            if (ImGui::Button("Connect"))
            {
                // TODO: Connect to server
                Scene::load<FieldSetupScene>();
            }

        ImGui::End();
    }

    // Render background scene
    //-------------------------

    for (int x = -10; x <= 10; x++)
        for (int y = -10; y <= 10; y++)
            Game::renderer()->draw(
                glm::translate(glm::mat4(1.0F), glm::vec3(x*8.0F, 0.0F, y*8.0F)), // Position
                Models::teapot, // Model
                Models::teapot.all().meshes.begin()->second, // Which part of the model?
                Materials::green); // What material to use for rendering?

    

}