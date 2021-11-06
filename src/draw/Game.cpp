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
 * @brief Ein Callback für, wenn sich die Fenstergröße verändert.
 * @param window Das betroffene Fenster.
 * @param width Die neue Breite des Fensters.
 * @param height Die neue Höhe des Fensters.
 */
static void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    // Stellt sicher, das die Abmessungen des Viewports mit
    // den neuen Abmessungen des Fensters übereinstimmen.
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
 * @brief Initialisiert das Fenster, OpenGL und ImGUI.
 * @retval Ob erfolgreich oder nicht.
 */
static bool init()
{

    // GLFW
    //------

    // Setze Fehler-Callback für GLFW
    glfwSetErrorCallback(glfwErrorCallback);
    
    // Initializiere und konfiguriere GLFW für OpenGL 4.6 im 'Core'-Profil
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Erforderlich bei Mac
#endif

    // Erstellt ein Fenster mit dem Titel "SchiffscheVersenky"
    s_window = glfwCreateWindow(960, 540, "SchiffscheVersenky", NULL, NULL);
    if (s_window == nullptr)
    {
        std::cout << "[Critical] Das Erstellen eines Fenster ist fehlgeschlagen!" << std::endl;
        glfwTerminate();
        return false;
    }

    // Konfiguriert das erstellte Fenster
    glfwMakeContextCurrent(s_window);
    glfwSetFramebufferSizeCallback(s_window, framebufferSizeCallback);
    glfwSwapInterval(1); // VSync

    // OpenGL
    //--------

    // Lädt alle OpenGL-Funktionspointer mit GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "[Critical] Das Laden der OpenGL-Funktionspointer ist fehlgeschlagen!" << std::endl;
        glfwTerminate();
        return false;
    }

    // Konfiguriert den globalen Zustand von OpenGL 
    glEnable(GL_DEPTH_TEST);

    // Dear ImGui
    //------------

    // Richtet ImGui ein.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Legt ImGui-Theme fest.
    ImGui::StyleColorsDark();
    //// ImGui::StyleColorsClassic();

    // Richtet die Platform/Renderer Backends ein.
    ImGui_ImplGlfw_InitForOpenGL(s_window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    // Initialisierung erfolgreich beendet.
    return true;

}

// Functions
//-----------

int Game::run()
{

    // Initialisiert das Fenster und OpenGL.
    if (!init()) return EXIT_FAILURE;

    // Die Haupt-Schleife. Führt solange aus, bis entweder das Fenster vom Benutzer geschlossen wurde,
    // oder ein System im Spiel eine Beendigung beantragt hat.
    while (!glfwWindowShouldClose(s_window) || s_exitCode.has_value())
    {

        // Ruft die IO-Events ab. (Tastatur-Eingabe, Maus, etc.)
        glfwPollEvents();

        // Beginnt den neuen Frame in ImGUI.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // TODO: Game Loop...

        // Rendert die ImGui-Fenster.
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Vertausche die Buffer und präsentiere somit das Bild.
        glfwSwapBuffers(s_window);

    }

    // Beendet GLFW und alle damit verbundenen Ressourcen.
    glfwTerminate();

    // Gibt den den Exit-Code der Ausführung zurück.
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