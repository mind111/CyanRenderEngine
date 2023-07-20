#include "World.h"
#include "GfxInterface.h"
#include "AssetManager.h"

namespace Cyan
{
    World::World(const char* name)
        : m_name(name), m_scene(nullptr)
    {
        m_scene = std::move(IScene::create());
    }

    World::~World()
    {

    }

    void World::update()
    {

    }

    void World::import(const char* filename)
    {
        AssetManager::import(this, filename);
    }
}