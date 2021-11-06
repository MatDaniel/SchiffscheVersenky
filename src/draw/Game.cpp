#include "Game.hpp"

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

// Properties
//------------

static Scene *s_scene = &EmptyScene::instance;
static GLFWwindow *s_window = nullptr;
static std::optional<int> s_exitCode;

// Utility
//---------

/**
 * @brief Initialisiert das Fenster und OpenGL.
 * @retval Ob erfolgreich oder nicht.
 */
static bool init()
{
    
    // Initializiere und konfiguriere GLFW für OpenGL 4.6 im 'Core'-Profil
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
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

    // Lädt alle OpenGL-Funktionspointer mit GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "[Critical] Das Laden der OpenGL-Funktionspointer ist fehlgeschlagen!" << std::endl;
        glfwTerminate();
        return false;
    }

    // Konfiguriert den globalen Zustand von OpenGL 
    glEnable(GL_DEPTH_TEST);

    // Initialisierung erfolgreich beendet.
    return true;

}

// Functions
//-----------

int Game::run()
{

    // Initialisiert das Fenster und OpenGL.
    if (!init()) return EXIT_FAILURE;

    // Initialisiere die Eintritts-Szene.
    s_scene->init();

    // Die Haupt-Schleife. Führt solange aus, bis entweder das Fenster vom Benutzer geschlossen wurde,
    // oder ein System im Spiel eine Beendigung beantragt hat.
    while (!glfwWindowShouldClose(s_window) || s_exitCode.has_value())
    {

        // Führt die Benutzer-Schleife aus, die das Bild für den jeweils nächsten Frame zeichnen soll.
        s_scene->draw(0.0F);

        // Vertausche die Buffer und präsentiere somit das Bild.
        glfwSwapBuffers(s_window);

        // Rufe die IO-Events ab. (Tastatur-Eingabe, Maus, etc.)
        glfwPollEvents();

    }

    // Beendet die Szene.
    s_scene->deinit();

    // Beendet GLFW und alle damit verbundenen Ressourcen.
    glfwTerminate();

    // Gibt den den Exit-Code der Ausführung zurück.
    return s_exitCode.value_or(EXIT_SUCCESS);

}

void Game::exit(int code)
{
    s_exitCode = code;
}

void Game::scene(Scene *scene)
{
    s_scene->deinit();
    s_scene = scene;
    s_scene->init();
}

Scene *Game::scene()
{
    return s_scene;
}