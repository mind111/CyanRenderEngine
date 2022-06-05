#pragma once

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"

#include "Common.h"
#include "Component.h"

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

namespace Cyan
{
    struct ICamera
    {
        virtual glm::mat4 view() = 0;
        virtual glm::mat4 projection() = 0;
        virtual glm::vec3 foward() { return glm::normalize(position - lookAt); }
        virtual glm::vec3 right() { }
        virtual glm::vec3 up() { }

        ICamera(const glm::vec3& inLookAt, const glm::vec3& inPosition)
            : lookAt(inLookAt), position(inPosition)
        {

        }

        glm::vec3 lookAt;
        glm::vec3 position;
    };

    struct PerspectiveCamera : public ICamera
    {
        /* ICamera interface */
        virtual glm::mat4 view() override
        {
            return glm::lookAt(position, lookAt, glm::vec3(0.f, 1.f, 0.f));
        }

        virtual glm::mat4 projection() override
        {
            return glm::perspective(fov, aspectRatio, n, f);
        }

        PerspectiveCamera(const glm::vec3& inLookAt, const glm::vec3& inPosition, f32 inFov, f32 inN, f32 inF, f32 inAspectRatio)
            : ICamera(inLookAt, inPosition), fov(inFov), n(inN), f(inF), aspectRatio(inAspectRatio)
        {

        }

        f32 fov, n, f, aspectRatio;
    };

    struct OrthographicCamera : public ICamera
    {
        /* ICamera interface */
        virtual glm::mat4 view() override
        {

        }

        virtual glm::mat4 projection() override
        {

        }

        f32 l, r, n, f;
    };

    struct CameraComponent : public Component
    {

    };
}
