module;

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <memory>

#include "BattleShip.h"

export module Draw.DearImGUI;
import Draw.Window;

// Properties
export SpdLogger DearImGUILog;
static ImGuiContext* m_context;

export namespace DearImGUI
{

	void Init()
	{
		
		// Setup
		IMGUI_CHECKVERSION();
		m_context = ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		// Theme
		ImGui::StyleColorsDark();

		// Platform/Renderer Backend.
		ImGui_ImplGlfw_InitForOpenGL(Window::Properties::Handle, true);
		ImGui_ImplOpenGL3_Init("#version 460");

		SPDLOG_LOGGER_INFO(DearImGUILog, "Initialized DearImGUI");

	}

	void CleanUp()
	{
		ImGui::DestroyContext(m_context);
	}

	void BeginFrame()
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void DrawFrame()
	{
		ImGui::Begin("Performance Monitor", NULL, ImGuiWindowFlags_NoSavedSettings);
		ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}

	void EndFrame()
	{
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

}