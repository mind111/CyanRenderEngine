#pragma once

#include "glm.hpp"

#include "Entity.h"

// color.w is intensity, making it vec4 to avoid alignment issue when using with SSBO
// TODO: come up with a better way of dealing with GPU memory alignment for this struct 
struct Light
{
    Entity* m_entity; // 8 bytes
    f32 m_padding[2]; // 8 bytes
    glm::vec4 color;
};

struct DirectionalLight
{
    Light baseLight;
    glm::vec4 direction;
}; 

struct PointLight
{
    Light baseLight;
    glm::vec4 position;
}; 