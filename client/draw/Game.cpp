#include "Game.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <optional>
#include <iostream>
#include <chrono>

// Properties
//------------

static glm::uvec2 s_windowSize { 960, 540 };
static std::chrono::high_resolution_clock s_timer;
static auto s_frameStart = s_timer.now();
static float s_deltaTime = 0.0F;
static GLFWwindow *s_window = nullptr;
static std::optional<int> s_exitCode;

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
    
    // Save size
    s_windowSize.x = width;
    s_windowSize.y = height;

    // Ensures that the viewport size correspond to the new window size.
    glViewport(0, 0, width, height);

}

static void glfwErrorCallback(int error, const char* description)
{
    std::cerr << "[Error] GLFW-Fehler " << error << ": " << description << std::endl;
}

// Utility
//---------

/**
 * @brief Initializes GLFW, OpenGL and ImGUI.
 * @retval Whether successful or not.
 */
static bool init()
{

    // GLFW
    //------

    // Initializes and configures GLFW for OpenGL 4.6 with the core profile.
    glfwSetErrorCallback(glfwErrorCallback);
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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
    glEnable(GL_DEPTH_TEST);

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

// Functions
//-----------

int Game::run(std::unique_ptr<Scene> &&scene)
{

    // Initializes the window and OpenGL.
    if (!init()) return EXIT_FAILURE;

    // Load entry scene
    Scene::load(std::move(scene));

    // The rendering loop. Exectues until the window is closed or a system asks for it.
    while (!glfwWindowShouldClose(s_window) && !s_exitCode.has_value())
    {

        // Update delta time.
        auto frameEnd = s_timer.now();
        using fdur = std::chrono::duration<float>;
        s_deltaTime = std::chrono::duration_cast<fdur>(frameEnd - s_frameStart).count();
        s_frameStart = frameEnd;

        // Pulls the IO events. (keyboard input, mouse, etc.)
        glfwPollEvents();

        // Begin new frame in imgui.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Update game logic
        Scene::current()->update();

        // Render frame in imgui.
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swaps the buffer and presents the image.
        glfwSwapBuffers(s_window);

    }

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

glm::uvec2 Game::windowSize() noexcept
{
    return s_windowSize;
}