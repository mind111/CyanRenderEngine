#pragma once

namespace Cyan 
{
    class Entity;
    class DirectionalLightComponent;

    class DirectionalLightEntity : public Entity
    {
    public:
        DirectionalLightEntity(World* world, const char* name, const Transform& local);
        ~DirectionalLightEntity();

        DirectionalLightComponent* getDirectionalLightComponent();
    protected:
        std::shared_ptr<DirectionalLightComponent> m_directionalLightComponent = nullptr;
    };

#if 0
    struct Entity;
    struct DirectionalLightComponent;

    struct DirectionalLightEntity : public Entity
    {
        /* Entity interface */
        virtual void update() override;
        virtual const char* getTypeDesc() override { return "DirectionalLightEntity"; }
        virtual void renderUI() override;

        DirectionalLightEntity(Scene* scene, const char* inName, const Transform& t, Entity* inParent);
        DirectionalLightEntity(Scene* scene, const char* inName, const Transform& t, Entity* inParenat, const glm::vec3& direction, const glm::vec4& colorAndIntensity, bool bCastShadow);

        DirectionalLightComponent* getDirectionalLightComponent() { return directionalLightComponent.get(); }

    private:
        std::unique_ptr<DirectionalLightComponent> directionalLightComponent = nullptr;
    };
#endif
}
