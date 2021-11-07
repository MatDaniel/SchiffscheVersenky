#include "Scene.hpp"

static std::unique_ptr<Scene> s_scene;

void Scene::load(std::unique_ptr<Scene> &&scene) noexcept
{
    assert(scene.get()); // Can't load `null` scene, please use type `Scene` as an empty scene.
    s_scene = std::move(scene);
    s_scene->init();
}

Scene *Scene::current() noexcept
{
    return s_scene.get();
}

void Scene::init()
{
    // Default implementation of `init` does nothing.
}

void Scene::update()
{
    // Default implementation of `scene` does nothing.
}