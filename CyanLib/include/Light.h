#pragma once

#include "glm.hpp"
#include "Component.h"

#include "Entity.h"
#include "Texture.h"

struct Light
{
    Entity* m_entity; // 8 bytes
    glm::vec4 color;
    Light(Entity* entity, const glm::vec4& inColor)
        : m_entity(entity), color(inColor)
    {

    }
    virtual void update() = 0;
};

#if 0
struct DirLightGpuData
{
    glm::vec4 m_color;
    glm::vec4 m_direction;
};

struct DirectionalLight : public Light
{
    // Light baseLight;
    glm::vec4 direction;

    DirectionalLight()
        : Light(0, glm::vec4(1.f)), direction(glm::vec4(0.f))
    {}

    DirectionalLight(Entity* entity, const glm::vec4& color, const glm::vec4 inDirection)
        : Light(entity, color), direction(inDirection)
    {}

    virtual void update() override
    {
        // FIXME: this is wrong!! 
        direction = m_entity->getWorldTransform().toMatrix() * direction;
    }

    DirLightGpuData getData()
    {
        return {color, direction};
    }
};
#endif

struct PointLightGpuData
{
    glm::vec4 m_color;
    glm::vec4 m_position;
};

struct PointLight : public Light
{
    // Light baseLight;
    glm::vec4 position;
    PointLight(Entity* entity, const glm::vec4& color, const glm::vec4 inPosition)
        : Light(entity, color), position(inPosition)
    {}

    virtual void update() override
    {
        position = m_entity->getSceneNode("LightMesh")->getWorldTransform().toMatrix()[3];
    }

    PointLightGpuData getData()
    {
        return {color, position};
    }
}; 

namespace Cyan
{
}
