#include "MenuScene.hpp"

#include <imgui.h>
#include <glad/glad.h>

#include "draw/Game.hpp"

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

        if (m_serverConnectionWindow)
        {
            auto drawList = ImGui::GetWindowDrawList();
            auto col = ImColor(0.0F, 0.0F, 0.0F, 0.5F);
            drawList->AddRectFilled(ImVec2(-0.1F, -0.1F), imWindowSize, col, 0.0f, ImDrawFlags_None); 
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

}