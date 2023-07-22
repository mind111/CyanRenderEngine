#include "World.h"
#include "GfxInterface.h"
#include "AssetManager.h"

namespace Cyan
{
    World::World(const char* name)
        : m_name(name), m_scene(nullptr)
    {
        assert(m_root == nullptr);
        m_root = createEntity<Entity>(m_name.c_str(), Transform());
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