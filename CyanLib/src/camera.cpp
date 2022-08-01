#include <iostream>
#include "glfw3.h"
#include "gtc/matrix_transform.hpp"
#include "camera.h"

namespace Cyan
{
    void CameraEntity::orbit(f32 phi, f32 theta)
    {
        ICamera* camera = getCamera();
        glm::vec3 p = camera->position - camera->lookAt;
        glm::quat quat(cos(.5f * -phi), sin(.5f * -phi) * camera->worldUp);
        quat = glm::rotate(quat, -theta, camera->right());
        glm::mat4 model(1.f);
        model = glm::translate(model, camera->lookAt);
        glm::mat4 rot = glm::toMat4(quat);
        glm::vec4 pPrime = rot * glm::vec4(p, 1.f);
        camera->position = glm::vec3(pPrime.x, pPrime.y, pPrime.z) + camera->lookAt;
    }

    void CameraEntity::rotate()
    {

    }

    void CameraEntity::zoom(f32 distance)
    {
        ICamera* camera = getCamera();
        glm::vec3 forward = camera->forward();
        glm::vec3 translation = forward * distance;
        glm::vec3 v1 = glm::normalize(camera->position + translation - camera->lookAt); 
        if (glm::dot(v1, forward) >= 0.f)
        {
            camera->position = camera->lookAt - 0.001f * forward;
        }
        else
        {
            camera->position += translation;
        }
    }
}