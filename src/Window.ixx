module;

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

export module Draw.Window;
import Draw.Scene;

static std::shared_ptr<spdlog::logger> s_logger;
static GLFWwindow* s_handle { nullptr };
static glm::uvec2 s_windowSize { 960, 540 };
static glm::vec2 s_cursorPos { 960, 540 };

#ifndef NDEBUG

/**
 * @brief A callback for errors.
 * @param error The error code.
 * @param desc The error description.
 */
static void glfwErrorCallback(int error, const char* desc)
{
	s_logger->error("Error {}: {}", error, desc);
}

#endif

/**
 * @brief A callback for window resize.
 * @param window The resized window.
 * @param width The new window width.
 * @param height The new window height.
 */
static void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{

    // Save window size
    s_windowSize.x = width;
    s_windowSize.y = height;

    // Set viewport to the size of the actual window
    glViewport(0, 0, width, height);

    // Process callback
    auto* scene = Scene::current();
    if (scene)
        scene->onWindowResize();

}

/**
 * @brief A callback for the mouse cursor.
 *        Will be called every time the cursor position changes.
 * @param window The window where the cursor moved.
 * @param xpos The new x position.
 * @param ypos The new y position.
 */
static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{

    // Save cursor location
    s_cursorPos = glm::vec2(xpos, ypos);

    // Process callback
	auto* scene = Scene::current();
    if (scene)
        scene->onCursorMoved();

}

export namespace Window
{

	namespace Properties
	{

		GLFWwindow* handle() noexcept
		{
			return s_handle;
		}

		const glm::uvec2& windowSize() noexcept
		{
			return s_windowSize;
		}

		const glm::vec2& cursorPos() noexcept
		{
			return s_cursorPos;
		}

	}

	bool init()
	{

		// Create logger
		s_logger = spdlog::stderr_color_mt("ShipWindow", spdlog::color_mode::automatic);

		// Configure error callback
#ifndef NDEBUG
		glfwSetErrorCallback(glfwErrorCallback);
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
		s_handle = glfwCreateWindow(s_windowSize.x, s_windowSize.y, "SchiffscheVersenky", NULL, NULL);
		if (s_handle == nullptr)
		{
			s_logger->critical("Window creation failed!");
			glfwTerminate();
			return false;
		}

		// Configures the created window
		glfwMakeContextCurrent(s_handle);
		glfwSwapInterval(1); // VSync

		// Callbacks
		glfwSetCursorPosCallback(s_handle, cursorPosCallback);
		glfwSetFramebufferSizeCallback(s_handle, framebufferSizeCallback);

		// Exit with success
		return true;

	}

	void close()
	{
		glfwSetWindowShouldClose(s_handle, 1);
	}

	void cleanUp()
	{
		if (s_handle)
		{
			glfwDestroyWindow(s_handle);
			glfwTerminate();
		}
	}

	bool closed()
	{
		return glfwWindowShouldClose(s_handle);
	}

	namespace Frame
	{
	
		void begin()
		{
			// Polls the IO events. (keyboard input, mouse, etc.)
			glfwPollEvents();
		}

		void end()
		{
			glfwSwapBuffers(s_handle);
		}
	
	}

}