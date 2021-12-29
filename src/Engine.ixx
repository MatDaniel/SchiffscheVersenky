module;

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <optional>
#include <memory>

#include "BattleShip.h"

export module Draw.Engine;
import Draw.Resources;
import Draw.Scene;
import Draw.Window;
import Draw.Timings;
import Draw.DearImGUI;

using namespace std;

// Logger
export SpdLogger EngineLog;

namespace
{

	// Private Properties
	std::optional<int> s_ExitCode;

#ifndef NDEBUG

	// This maps opengl debug enums to their names, used by the error callback.
	static const std::unordered_map<GLenum, const char*> OpenGLDebugTranslations{ {

			// SOURCE
			{ GL_DEBUG_SOURCE_API, "API" },
			{ GL_DEBUG_SOURCE_WINDOW_SYSTEM, "Window System" },
			{ GL_DEBUG_SOURCE_SHADER_COMPILER, "Shader Compiler" },
			{ GL_DEBUG_SOURCE_THIRD_PARTY, "Third Party" },
			{ GL_DEBUG_SOURCE_APPLICATION, "Application" },
			{ GL_DEBUG_SOURCE_OTHER, "Other" },

			// TYPE
			{ GL_DEBUG_TYPE_ERROR, "Error" },
			{ GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, "Deprecated behavior" },
			{ GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, "Undefined behavior" },
			{ GL_DEBUG_TYPE_PORTABILITY, "Portability" },
			{ GL_DEBUG_TYPE_PERFORMANCE, "Performance" },
			{ GL_DEBUG_TYPE_OTHER, "Other" }

		} };

	static void ErrorCallbackGL(GLenum Source, GLenum Type, GLuint Id, GLenum Severity, GLsizei Length, const GLchar* Message, const void* UserParam)
	{
		constexpr const char* DebugOutput = "{} from {}: {}";
		static auto EnumStr = [](GLenum e) -> auto {
			auto iter = OpenGLDebugTranslations.find(e);
			return iter != OpenGLDebugTranslations.end() ? iter->second : "Unknown";
		};

		switch (Severity)
		{
		case GL_DEBUG_SEVERITY_HIGH:
			SPDLOG_LOGGER_CRITICAL(EngineLog, DebugOutput, EnumStr(Type), EnumStr(Source), Message);
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			SPDLOG_LOGGER_ERROR(EngineLog, DebugOutput, EnumStr(Type), EnumStr(Source), Message);
			break;
		case GL_DEBUG_SEVERITY_LOW:
			SPDLOG_LOGGER_WARN(EngineLog, DebugOutput, EnumStr(Type), EnumStr(Source), Message);
			break;
		default:
			SPDLOG_LOGGER_INFO(EngineLog, DebugOutput, EnumStr(Type), EnumStr(Source), Message);
			break;
		}
	}

#endif

}

export namespace Engine
{

	bool Init()
	{

		// Loads the opengl function pointer with GLAD
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		{
			SPDLOG_LOGGER_CRITICAL(EngineLog, "Failed to initialize the OpenGL function pointers!");
			return false;
		}

		SPDLOG_LOGGER_INFO(EngineLog, "Initialized opengl function pointers");

		// Configures the global state of OpenGL
#ifndef NDEBUG
		glEnable(GL_DEBUG_OUTPUT); // Enables error callback: Used to listen to errors and print them out.
		glDebugMessageCallback(ErrorCallbackGL, nullptr);
#endif
		glEnable(GL_DEPTH_TEST); // Depth Buffer: used to sort out the order of the overlapping faces.
		glEnable(GL_CULL_FACE); // Performance optimization: skips fragment shader calls for faces that are not visible to the camera.
		glEnable(GL_MULTISAMPLE); // Required for MSAA
		SPDLOG_LOGGER_INFO(EngineLog, "Configured opengl state");

		// Finished successfully
		return true;

	}

    /**
     * @brief Initializes the game and executes the game loop.
     * @retval The error code with which the game ended.
     */
    int Run()
	{

		// The rendering loop. Executes until the window is closed or a system asks for it.
		while (!Window::IsClosed() && !s_ExitCode.has_value())
		{

			// Begin Frame
			Window::BeginFrame();
			DearImGUI::BeginFrame();
			Timings::Update();
			Scene::Next(); // Loads next scene if available
			
			// Execute draw
            Scene::Current()->OnDraw();

			// End Frame
			DearImGUI::EndFrame();
			Window::EndFrame();

		}

		SPDLOG_LOGGER_INFO(EngineLog, "Initialized opengl function pointers");

		// Returns the exit code.
		return s_ExitCode.value_or(EXIT_SUCCESS);

	}

    void Exit(int code = 0) noexcept
	{
		s_ExitCode = code;
	}

}