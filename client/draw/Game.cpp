#include "Game.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <optional>
#include <iostream>
#include <chrono>

#include "resource.h"
#include "res/Models.hpp"
#include "res/Textures.hpp"
#include "res/Materials.hpp"
#include "res/ShaderStages.hpp"
#include "res/ShaderPipelines.hpp"
#include "SceneRenderer.hpp"

// Properties
//------------

// Window & rendering
static glm::uvec2 s_windowSize { 960, 540 };
static GLFWwindow *s_window = nullptr;
static std::optional<int> s_exitCode;
static SceneRenderer *s_renderer;

// Timings
static std::chrono::high_resolution_clock s_timer;
static auto s_firstFrame = s_timer.now();
static auto s_frameStart = s_firstFrame;
static float s_deltaTime = 0.0F;
static float s_time = 0.0F;

// Callbacks
//-----------

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
    s_renderer->uploadProjection();

}

#ifndef NDEBUG

static void glfwErrorCallback(int error, const char* description)
{
    std::cerr << "[ShipWindow] GLFW-Error " << error << ": " << description << std::endl;
}

static void gladErrorCallback(const char *name, void *funcptr, int len_args, ...)
{
    GLenum error_code = glad_glGetError();
    if (error_code)
        std::cerr << "[ShipRenderer] GL-Error " << error_code << " in " << name << std::endl;
}

#endif

// Utility
//---------

/**
 * @brief Initializes GLFW, OpenGL and ImGUI.
 * @retval Whether successful or not.
 */
static bool init()
{

    // Error Callbacks
    //-----------------

    #ifndef NDEBUG
    glfwSetErrorCallback(glfwErrorCallback);
    glad_set_post_callback_gl(gladErrorCallback);
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
    //glEnable(GL_CULL_FACE); // Performance optimization:
                            // skips fragment shader calls for faces that are not visible from the camera.
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

static void initResources()
{

    // Shader Stages
    ShaderStages::Vertex::cel = { GL_VERTEX_SHADER, IDR_VSHA_CEL };
    ShaderStages::Fragment::cel = { GL_FRAGMENT_SHADER, IDR_FSHA_CEL };

    // Shader Pipelines
    ShaderPipelines::cel = {{
        &ShaderStages::Vertex::cel,
        &ShaderStages::Fragment::cel
    }};

    // Models
    Models::teapot = { IDR_MESH_TEAPOT };

}

// Functions
//-----------

int Game::run(Scene::GetterFunc initalSceneGetter)
{

    // Initializes the window and OpenGL.
    if (!init()) return EXIT_FAILURE;

    // Create scene renderer
    s_renderer = new SceneRenderer();

    // Load resources
    initResources();

    // Load entry scene
    Scene::load(initalSceneGetter());

    // The rendering loop. Exectues until the window is closed or a system asks for it.
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

        // Update game logic
        Scene::current()->update();

        // Render scene
        s_renderer->render();

        // Render frame in imgui.
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swaps the buffer and presents the image.
        glfwSwapBuffers(s_window);

    }

    // Delete scene renderer
    delete s_renderer;

    // Terminates GLFW and all its resources.
    glfwTerminate();

    // Returns the exit code.
    return s_exitCode.value_or(EXIT_SUCCESS);

}

void Game::exit(int code) noexcept
{
    s_exitCode = code;
}

float Game::deltaTime() noexcept
{
    return s_deltaTime;
}

float Game::time() noexcept
{
    return s_time;
}

const glm::uvec2 &Game::windowSize() noexcept
{
    return s_windowSize;
}

SceneRenderer *Game::renderer() noexcept
{
    return s_renderer;
}