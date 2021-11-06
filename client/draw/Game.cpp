#include "Game.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <optional>
#include <iostream>

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
    // Ensures that the viewport size correspond to the new window size.
    glViewport(0, 0, width, height);
}

static void glfwErrorCallback(int error, const char* description)
{
    std::cerr << "[Error] GLFW-Fehler " << error << ": " << description << std::endl;
}

// Properties
//------------

static entt::registry s_world;
static GLFWwindow *s_window = nullptr;
static std::optional<int> s_exitCode;

// Utility
//---------

/**
 * @brief Initializes GLFW, OpenGL und ImGUI.
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
    s_window = glfwCreateWindow(960, 540, "SchiffscheVersenky", NULL, NULL);
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

int Game::run()
{

    // Initializes the window and OpenGL.
    if (!init()) return EXIT_FAILURE;

    // The rendering loop. Exectues until the window is closed or a system asks for it.
    while (!glfwWindowShouldClose(s_window) || s_exitCode.has_value())
    {

        // Pulls the IO events. (keyboard input, mouse, etc.)
        glfwPollEvents();

        // Begin new frame in imgui.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // TODO: Game Loop...

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

void Game::exit(int code)
{
    s_exitCode = code;
}

entt::registry& Game::world()
{
    return s_world;
}