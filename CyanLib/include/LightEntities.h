#pragma once

namespace Cyan {
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
