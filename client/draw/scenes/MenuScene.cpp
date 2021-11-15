#include "MenuScene.hpp"

#include <imgui.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "draw/Game.hpp"
#include "draw/res/ShaderPrograms.hpp"
#include "draw/res/Models.hpp"
#include "draw/res/Materials.hpp"
#include "draw/util/UniformBufferObjects.hpp"

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
            }

        ImGui::End();
    }

    // Render background scene
    //-------------------------

    ShaderPrograms::cel.use();
    m_light.use();
    m_camera.direction({ -0.5F, -0.3F, 1.0F });
    m_camera.position({ 2.0F, 3.0F, -3.6F });
    m_camera.use();
    Models::teapot.use();

    InstanceUBO instanceUbo;
    instanceUbo.model = glm::mat4(1.0F);
    glProgramUniformMatrix4fv(ShaderPrograms::cel.id(), glGetUniformLocation(ShaderPrograms::cel.id(), "model"), 1, false, (GLfloat*)&instanceUbo.model);

    for (auto &iter : Models::teapot.all().meshes)
    {
        // iter.first->use();
        glDrawElementsInstanced(GL_TRIANGLES, iter.second.size, GL_UNSIGNED_INT, 0, 1);
    }
    

}