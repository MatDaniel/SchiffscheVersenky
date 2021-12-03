#include "Scene.hpp"

#include <cassert>

// Properties
//------------

static std::unique_ptr<Scene> s_scene;

// Implementation
//----------------

void Scene::load(std::unique_ptr<Scene> &&scene) noexcept
{
    assert(scene.get()); // Can't load `null` scene, please use type `Scene` as an empty scene.
    s_scene = std::move(scene);
}

Scene *Scene::current() noexcept
{
    return s_scene.get();
}

void Scene::update()
{
    // Default implementation of `scene` does nothing.
}