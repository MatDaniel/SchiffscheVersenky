module;

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include <optional>
#include <iostream>
#include <chrono>

#include "resource.h"

export module Draw.Engine;
import Draw.Resources;
import Draw.Scene;

// Window & rendering
static glm::uvec2 s_windowSize{ 960, 540 };
static GLFWwindow* s_window = nullptr;
static std::optional<int> s_exitCode;

// Timings
static std::chrono::high_resolution_clock s_timer;
static auto s_firstFrame = s_timer.now();
static auto s_frameStart = s_firstFrame;
static float s_deltaTime = 0.0F;
static float s_time = 0.0F;

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

    // Update projection
    auto* scene = Scene::current();
    if (scene)
        scene->onWindowResize();

}

#ifndef NDEBUG

static void glfwErrorCallback(int error, const char* description)
{
    spdlog::error("[ShipRenderer] GLFW-Error {}: {}", error, description);
}

static void gladErrorCallback(const char* name, void* funcptr, int len_args, ...)
{
	GLenum error_code = glad_glGetError();
	if (error_code)
        spdlog::error("[ShipRenderer] GL-Error {} in {}", error_code, name);
}

#endif

/**
 * @brief Initializes GLFW, OpenGL and ImGUI.
 * @retval Whether successful or not.
 */
inline static bool init()
{

    // Error Callbacks
    //-----------------

#ifndef NDEBUG
    glfwSetErrorCallback(glfwErrorCallback);
    glad_set_post_callback(gladErrorCallback);
#endif

    // GLFW
    //------

    // Initializes and configures GLFW for OpenGL 4.6 with the core profile.
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 16); // MSAA

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required for Mac
#endif

    // Creates a window with the title "SchiffscheVersenky"
    s_window = glfwCreateWindow(s_windowSize.x, s_windowSize.y, "SchiffscheVersenky", NULL, NULL);
    if (s_window == nullptr)
    {
        std::cout << "[Critical] Window creation failed!" << std::endl;
        glfwTerminate();
        return false;
    }

    // Configures the created window
    glfwMakeContextCurrent(s_window);
    glfwSetFramebufferSizeCallback(s_window, framebufferSizeCallback);
    glfwSwapInterval(1); // VSync

    // OpenGL
    //--------

    // Loads the opengl function pointer with GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "[Critical] Loading the OpenGL function pointers failed!" << std::endl;
        glfwTerminate();
        return false;
    }

    // Configures the global state of OpenGL
    glEnable(GL_DEPTH_TEST); // Depth Buffer:
                             // used to sort out the order of the overlapping faces.
    glEnable(GL_CULL_FACE); // Performance optimization:
                            // skips fragment shader calls for faces that are not visible to the camera.
    glEnable(GL_MULTISAMPLE);

    // Dear ImGui
    //------------

    // Setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Theme
    ImGui::StyleColorsDark();
    //// ImGui::StyleColorsClassic();

    // Platform/Renderer Backend.
    ImGui_ImplGlfw_InitForOpenGL(s_window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    // Finished successfully
    return true;

}

inline static void initResources()
{

    // Shader Stages
    auto* cel_vert = Resources::emplace<ShaderStage>("VERT:Cel", GL_VERTEX_SHADER, Resource(IDR_VSHA_CEL));
    auto* cel_frag = Resources::emplace<ShaderStage>("FRAG:Cel", GL_FRAGMENT_SHADER, Resource(IDR_FSHA_CEL));

	// Shader Pipelines
    auto* cel_prog = Resources::emplace<ShaderPipeline>("Cel", cel_vert, cel_frag);

	// Materials
    Resources::emplace<Material>("Border", cel_prog, nullptr, glm::vec4(1.0F, 1.0F, 1.0F, 1.0F));
    Resources::emplace<Material>("Water", cel_prog, nullptr, glm::vec4(0.18F, 0.33F, 1.0F, 1.0F));

	// Models
    Resources::emplace<Model>("Water", Resource(IDR_MESH_WATER));
    Resources::emplace<Model>("Teapot", Resource(IDR_MESH_TEAPOT));

}

inline static void destroyResources()
{
    Resources::clear<Model>();
    Resources::clear<Material>();
    Resources::clear<ShaderPipeline>();
    Resources::clear<ShaderStage>();
    Resources::clear<Texture>();
}

export namespace Engine
{

    /**
     * @brief Initializes the game and executes the game loop.
     * @retval The error code with which the game ended.
     */
    inline int run()
	{

		// Initializes the window and OpenGL.
		if (!init()) return EXIT_FAILURE;

        // Load resources
        initResources();

		// The rendering loop. Executes until the window is closed or a system asks for it.
		while (!glfwWindowShouldClose(s_window) && !s_exitCode.has_value())
		{

			// Update delta time.
			auto frameEnd = s_timer.now();
			using fdur = std::chrono::duration<float>;
			s_deltaTime = std::chrono::duration_cast<fdur>(frameEnd - s_frameStart).count();
			s_time = std::chrono::duration_cast<fdur>(frameEnd - s_firstFrame).count();
			s_frameStart = frameEnd;

			// Pulls the IO events. (keyboard input, mouse, etc.)
			glfwPollEvents();

			// Begin new frame in imgui.
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			// Load next scene if available
			Scene::next();
            
            // Update scene logic
            Scene::current()->update();

			// Render frame in imgui.
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			// Swaps the buffer and presents the image.
			glfwSwapBuffers(s_window);

		}

        // Release the current scene. (destroys it)
        Scene::release();

        // Clean up resources
        destroyResources();

		// Terminates GLFW and all its resources.
		glfwTerminate();

		// Returns the exit code.
		return s_exitCode.value_or(EXIT_SUCCESS);

	}

    inline void exit(int code) noexcept
	{
		s_exitCode = code;
	}

    inline float deltaTime() noexcept
	{
		return s_deltaTime;
	}

    inline float time() noexcept
	{
		return s_time;
	}

    inline const glm::uvec2& windowSize() noexcept
	{
		return s_windowSize;
	}

}