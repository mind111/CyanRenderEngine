#include <iostream>
#include "glfw3.h"
#include "gtc/matrix_transform.hpp"
#include "camera.h"

void Camera::update()
{
    forward = glm::normalize(lookAt - position);
    right = glm::normalize(glm::cross(forward, worldUp));
    up = glm::normalize(glm::cross(right, forward));
    view = glm::lookAt(position, lookAt, worldUp);
}