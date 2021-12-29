module;

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include <memory>

#include "BattleShip.h"

export module Draw.Window;
import Draw.Scene;

// Logger
export SpdLogger WindowLog;

namespace
{

	// Properties
	static GLFWwindow* s_Handle{ nullptr };
	static glm::uvec2 s_WindowSize{ 960, 540 };
	static glm::vec2 s_CursorPos{ 960, 540 };

#ifndef NDEBUG

	/**
	 * @brief A callback for errors.
	 * @param error The error code.
	 * @param desc The error description.
	 */
	static void ErrorCallbackGLFW(int error, const char* desc)
	{
		SPDLOG_LOGGER_ERROR(WindowLog, "Error-Code {}: {}", error, desc);
	}

#endif

	/**
	 * @brief A callback for window resize.
	 * @param window The resized window.
	 * @param width The new window width.
	 * @param height The new window height.
	 */
	static void CallbackFramebufferSize(GLFWwindow* window, int width, int height)
	{

		// Save window size
		s_WindowSize.x = width;
		s_WindowSize.y = height;

		// Set viewport to the size of the actual window
		glViewport(0, 0, width, height);

		// Process callback
		auto* scene = Scene::Current();
		if (scene)
			scene->OnWindowResize();

	}

	/**
	 * @brief A callback for the mouse cursor.
	 *        Will be called every time the cursor position changes.
	 * @param window The window where the cursor moved.
	 * @param xpos The new x position.
	 * @param ypos The new y position.
	 */
	static void CallbackCursorPos(GLFWwindow* window, double xpos, double ypos)
	{

		// Save cursor location
		s_CursorPos = glm::vec2(xpos, ypos);

		// Process callback
		auto* scene = Scene::Current();
		if (scene)
			scene->OnCursorMoved();

	}

	static void CallbackKeyboardKey(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		// Process callback
		auto* scene = Scene::Current();
		if (scene)
			scene->OnKeyboardKey(key, action, mods);
	}

	static void CallbackMouseButton(GLFWwindow* window, int button, int action, int mods)
	{
		// Process callback
		auto* scene = Scene::Current();
		if (scene)
			scene->OnMouseButton(button, action, mods);
	}

}

export namespace Window
{

	namespace Properties
	{

		auto& Handle = s_Handle;
		const auto& WindowSize = s_WindowSize;
		const auto& CursorPos = s_CursorPos;

	}

	bool Init()
	{

		// Configure error callback
#ifndef NDEBUG
		glfwSetErrorCallback(ErrorCallbackGLFW);
#endif

		// Initializes and configures GLFW for OpenGL 4.6 with the core profile.
		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_SAMPLES, 16); // MSAA - TODO: Do msaa manually

#ifdef __APPLE__
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required for Mac
#endif

		// Creates a window with the title "SchiffscheVersenky"
		s_Handle = glfwCreateWindow(s_WindowSize.x, s_WindowSize.y, "SchiffscheVersenky", NULL, NULL);
		if (s_Handle == nullptr)
		{
			SPDLOG_LOGGER_CRITICAL(WindowLog, "Failed to initialize window!");
			glfwTerminate();
			return false;
		}

		// Configures the created window
		glfwMakeContextCurrent(s_Handle);
		glfwSwapInterval(1); // VSync

		// Callbacks
		glfwSetCursorPosCallback(s_Handle, CallbackCursorPos);
		glfwSetFramebufferSizeCallback(s_Handle, CallbackFramebufferSize);
		glfwSetKeyCallback(s_Handle, CallbackKeyboardKey);
		glfwSetMouseButtonCallback(s_Handle, CallbackMouseButton);

		// Debug
		SPDLOG_LOGGER_INFO(WindowLog, "Initialized window");

		// Exit with success
		return true;

	}

	void Close()
	{
		glfwSetWindowShouldClose(s_Handle, 1);
	}

	void CleanUp()
	{
		if (s_Handle)
		{
			glfwDestroyWindow(s_Handle);
			glfwTerminate();
		}
	}

	bool IsClosed()
	{
		return glfwWindowShouldClose(s_Handle);
	}

	void BeginFrame()
	{
		// Polls the IO events. (keyboard input, mouse, etc.)
		glfwPollEvents();
	}

	void EndFrame()
	{
		glfwSwapBuffers(s_Handle);
	}

	void SetTitle(const char* title)
	{
		glfwSetWindowTitle(s_Handle, title);
	}

}