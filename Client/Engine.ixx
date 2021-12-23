module;

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <optional>

export module Draw.Engine;
import Draw.Resources;
import Draw.Scene;
import Draw.Window;
import Draw.Timings;
import Draw.DearImGUI;

// Properties
static std::shared_ptr<spdlog::logger> s_glLogger;
static std::optional<int> s_exitCode;

#ifndef NDEBUG

static const std::unordered_map<GLenum, const char*> enumTranslations{ {
    { GL_DEBUG_SOURCE_API, "API" },
    { GL_DEBUG_SOURCE_WINDOW_SYSTEM, "Window System" },
    { GL_DEBUG_SOURCE_SHADER_COMPILER, "Shader Compiler" },
    { GL_DEBUG_SOURCE_THIRD_PARTY, "Third Party" },
    { GL_DEBUG_SOURCE_APPLICATION, "Application" },
    { GL_DEBUG_SOURCE_OTHER, "Other" },
    { GL_DEBUG_TYPE_ERROR, "Error" },
    { GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, "Deprecated behavior" },
    { GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, "Undefined behavior" },
    { GL_DEBUG_TYPE_PORTABILITY, "Portability" },
    { GL_DEBUG_TYPE_PERFORMANCE, "Performance" },
    { GL_DEBUG_TYPE_OTHER, "Other" }
}};

static const char* dbgGLStr(GLenum dbgVal)
{
    auto iter = enumTranslations.find(dbgVal);
    return iter != enumTranslations.end() ? iter->second : "Unknown";
}

constexpr uint32_t severityLevel(GLenum severity)
{
    switch (severity)
    {
	case GL_DEBUG_SEVERITY_HIGH:
		return 1;
	case GL_DEBUG_SEVERITY_MEDIUM:
		return 2;
	case GL_DEBUG_SEVERITY_LOW:
		return 3;
	case GL_DEBUG_SEVERITY_NOTIFICATION:
		return 4;
    default:
        return 0;
    }
}

static void glErrorCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    constexpr GLenum MinimumSeverity = GL_DEBUG_SEVERITY_NOTIFICATION;
    if (severityLevel(MinimumSeverity) >= severityLevel(severity))
        switch (severity)
        {
		case GL_DEBUG_SEVERITY_HIGH:
            s_glLogger->critical("{} from {}: {}", dbgGLStr(type), dbgGLStr(source), message);
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
            s_glLogger->error("{} from {}: {}", dbgGLStr(type), dbgGLStr(source), message);
			break;
		case GL_DEBUG_SEVERITY_LOW:
            s_glLogger->warn("{} from {}: {}", dbgGLStr(type), dbgGLStr(source), message);
			break;
        default:
            s_glLogger->info("{} from {}: {}", dbgGLStr(type), dbgGLStr(source), message);
			break;
        }
}

#endif

export namespace Engine
{

	bool init()
	{

		s_glLogger = spdlog::stderr_color_mt("OpenGL", spdlog::color_mode::automatic);

		// Loads the opengl function pointer with GLAD
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		{
			s_glLogger->critical("Loading the OpenGL function pointers failed!");
			return false;
		}

		// Configures the global state of OpenGL
#ifndef NDEBUG
		glEnable(GL_DEBUG_OUTPUT); // Enables error callback: Used to listen to errors and print them out.
		glDebugMessageCallback(glErrorCallback, nullptr);
#endif
		glEnable(GL_DEPTH_TEST); // Depth Buffer: used to sort out the order of the overlapping faces.
		glEnable(GL_CULL_FACE); // Performance optimization: skips fragment shader calls for faces that are not visible to the camera.
		glEnable(GL_MULTISAMPLE); // Required for MSAA

		// Finished successfully
		return true;

	}

    /**
     * @brief Initializes the game and executes the game loop.
     * @retval The error code with which the game ended.
     */
    int run()
	{

		// The rendering loop. Executes until the window is closed or a system asks for it.
		while (!Window::closed() && !s_exitCode.has_value())
		{

			// Begin Frame
			Window::Frame::begin();
			DearImGUI::Frame::begin();
			Timings::update();
			Scene::next(); // Loads next scene if available
			
			// Execute draw
			DearImGUI::Frame::draw();
            Scene::current()->onDraw();

			// End Frame
			DearImGUI::Frame::end();
			Window::Frame::end();

		}

		// Returns the exit code.
		return s_exitCode.value_or(EXIT_SUCCESS);

	}

    void exit(int code) noexcept
	{
		s_exitCode = code;
	}

}