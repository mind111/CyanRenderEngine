#include "DirectionalLight.h"
#include "CyanRenderer.h"
#include "Scene.h"
#include "LightComponent.h"

namespace Cyan
{
    DirectionalLightEntity::DirectionalLightEntity(Scene* scene, const char* inName, const Transform& t, Entity* inParent)
        : Entity(scene, inName, t, inParent, EntityFlag_kDynamic)
    {
        addComponent(new DirectionalLightComponent());
    }
    
    DirectionalLightEntity::DirectionalLightEntity(Scene* scene, const char* inName, const Transform& t, Entity* inParent, const glm::vec3& direction, const glm::vec4& colorAndIntensity, bool bCastShadow)
        : Entity(scene, inName, t, inParent, EntityFlag_kDynamic)
    {
        addComponent(new DirectionalLightComponent(direction, colorAndIntensity, bCastShadow));
    }
}
