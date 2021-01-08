#pragma once

#include "glm.hpp"

// color.w is intensity, making it vec4 to avoid alignment issue when using with SSBO
struct Light
{
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
