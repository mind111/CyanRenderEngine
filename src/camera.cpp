#include <iostream>
#include "glfw3.h"
#include "gtc/matrix_transform.hpp"
#include "camera.h"

void CameraManager::initDefaultCamera(Camera& camera)
{
    /* Default camera settings */
    camera.position = glm::vec3(1.f, 2.f, 5.f);
    camera.worldUp = glm::vec3(0.f, 1.f, 0.f);
    camera.forward = glm::vec3(0.f, 0.f, -1.f);
    camera.fov = 50.f;
    camera.n = 0.1f;
    camera.f = 100.f;
}

void CameraManager::updateCamera(Camera& camera)
{
    /* Update camera frames and view matrix accordingly */
    camera.forward = glm::normalize(camera.lookAt - camera.position);
    camera.right = glm::normalize(glm::cross(camera.forward, camera.worldUp));
    camera.up = glm::normalize(glm::cross(camera.right, camera.forward));
    camera.view = glm::lookAt(camera.position, camera.lookAt, camera.worldUp);
}