#pragma once

namespace Cyan 
{
    class Entity;
    class DirectionalLightComponent;
    class SkyLightComponent;

    class DirectionalLightEntity : public Entity
    {
    public:
        DirectionalLightEntity(World* world, const char* name, const Transform& local);
        ~DirectionalLightEntity();

        DirectionalLightComponent* getDirectionalLightComponent();
    protected:
        std::shared_ptr<DirectionalLightComponent> m_directionalLightComponent = nullptr;
    };

    class SkyLightEntity : public Entity
    {
    public:
        SkyLightEntity(World* world, const char* name, const Transform& local);
        ~SkyLightEntity();

        SkyLightComponent* getSkyLightComponent();
    protected:
        std::shared_ptr<SkyLightComponent> m_skyLightComponent = nullptr;
    };
}
