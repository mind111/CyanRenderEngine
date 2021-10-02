#pragma once

#include "glm.hpp"

#include "Entity.h"

// color.w is intensity, making it vec4 to avoid alignment issue when using with SSBO
// TODO: come up with a better way of dealing with GPU memory alignment for this struct 
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

struct DirLightData
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

    DirLightData getData()
    {
        return {color, direction};
    }
}; 

struct PointLightData
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

    PointLightData getData()
    {
        return {color, position};
    }
}; 

struct Light2 : public Entity
{
    glm::vec4 color;
};

struct PointLight2 : public Light2
{
    void updateData()
    {
        m_lightData.m_color = this->color;
        m_lightData.m_position = glm::vec4(getWorldTransform().m_translate, 1.0f);
    }

    PointLightData m_lightData;
};