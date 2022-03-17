#pragma once

#include "glm.hpp"

struct Camera
{
    glm::vec3 lookAt;
    glm::vec3 position;
    glm::vec3 forward, right, up;
    glm::vec3 worldUp;
    glm::mat4 view, projection;
    float fov, n, f;
    float aspectRatio;

    void update();
};
