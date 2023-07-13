#include "World.h"
#include "GfxInterface.h"

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
}