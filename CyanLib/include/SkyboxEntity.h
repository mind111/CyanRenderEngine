#pragma once

#include "Entity.h"

namespace Cyan
{
    class World;
    class SkyboxComponent;

    class SkyboxEntity : public Entity
    {
    public:
        SkyboxEntity(World* world, const char* name, const Transform& local);
        virtual ~SkyboxEntity();

        SkyboxComponent* getSkyboxComponent();

    protected:
        std::shared_ptr<SkyboxComponent> m_skyboxComponent = nullptr;
    };
}
