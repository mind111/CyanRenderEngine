#pragma once

#include "glm.hpp"
#include "gtx/quaternion.hpp"

struct Rotation
{
    float angle;
    glm::vec3 axis;
};

struct Transform
{
    glm::vec3 translation;
    glm::quat qRot; 
    glm::vec3 scale;
};