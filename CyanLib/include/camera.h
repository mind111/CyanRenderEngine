#pragma once

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"

#include "Common.h"
#include "Component.h"
#include "Entity.h"

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
        virtual glm::mat4 view() 
        { 
            return glm::lookAt(position, lookAt, glm::vec3(0.f, 1.f, 0.f));
        }

        virtual glm::mat4 projection() = 0;
        virtual glm::vec3 forward() { return glm::normalize(position - lookAt); }
        virtual glm::vec3 right() { return glm::cross(forward(), glm::vec3(0.f, 1.f, 0.f)); }
        virtual glm::vec3 up() { return glm::cross(right(), forward()); }

        ICamera(const glm::vec3& inLookAt, const glm::vec3& inPosition)
            : lookAt(inLookAt), position(inPosition)
        { }

        glm::vec3 lookAt;
        glm::vec3 position;
    };

    struct PerspectiveCamera : public ICamera
    {
        /* ICamera interface */
        virtual glm::mat4 projection() override
        {
            return glm::perspective(fov, aspectRatio, n, f);
        }

        PerspectiveCamera(const glm::vec3& inLookAt, const glm::vec3& inPosition, f32 inFov, f32 inN, f32 inF, f32 inAspectRatio)
            : ICamera(inLookAt, inPosition), fov(inFov), n(inN), f(inF), aspectRatio(inAspectRatio)
        {

        }

    private:
        f32 fov, n, f, aspectRatio;
    };

    struct OrthographicCamera : public ICamera
    {
        /* ICamera interface */
        virtual glm::mat4 projection() override
        {
            f32 left = viewVolume.pmin.x;
            f32 right = viewVolume.pmax.x;
            f32 bottom = viewVolume.pmin.y;
            f32 top = viewVolume.pmin.y;
            f32 zNear = viewVolume.pmin.z;
            f32 zFar = viewVolume.pmax.z;
            return glm::orthoZO(left, right, bottom, top, zNear, zFar);
        }

        OrthographicCamera(const glm::vec3& inLookAt, const glm::vec3& inPosition, const BoundingBox3D& inViewAABB)
            : ICamera(inLookAt, inPosition), viewVolume(inViewAABB)
        {

        }

    private:
        // aabb of the view volume in view space
        BoundingBox3D viewVolume;
    };

    struct CameraComponent : public Component
    {
        enum class ProjectionType
        {
            kPerspective,
            kOrthographic,
            kInvalid
        } projectionType = ProjectionType::kPerspective;

        CameraComponent(const glm::vec3& inLookAt, const glm::vec3& inPosition, f32 inFov, f32 inN, f32 inF, f32 inAspectRatio)
            : projectionType(ProjectionType::kPerspective)
        {
            cameraPtr = std::make_unique<PerspectiveCamera>(inLookAt, inPosition, inFov, inN, inF, inAspectRatio);
        }

        CameraComponent(const glm::vec3& inLookAt, const glm::vec3& inPosition, const BoundingBox3D& inViewAABB)
            : projectionType(ProjectionType::kOrthographic)
        {
            cameraPtr = std::make_unique<OrthographicCamera>(inLookAt, inPosition, inViewAABB);
        }

        const glm::mat4& view() { return cameraPtr->view(); }
        const glm::mat4& projection() { return cameraPtr->projection(); }

    private:
        std::unique_ptr<ICamera> cameraPtr;
    };

    struct CameraEntity : public Entity
    {
        // perspective
        CameraEntity(Scene* scene, const char* inName, const Transform& t, const glm::vec3& inLookAt, f32 inFov, f32 inN, f32 inF, f32 inAspectRatio, Entity* inParent = nullptr, u32 inProperties = (EntityFlag_kDynamic))
            : Entity(scene, inName, t, inParent, inProperties)
        {
            glm::vec3 position = t.m_translate;
            cameraComponentPtr = std::make_unique<CameraComponent>(inLookAt, position, inFov, inN, inF, inAspectRatio);
        }

        // ortho
        CameraEntity(Scene* scene, const char* inName, const Transform& t, const glm::vec3& inLookAt, const BoundingBox3D& inViewAABB, Entity* inParent = nullptr, u32 inProperties = (EntityFlag_kDynamic))
            : Entity(scene, inName, t, inParent, inProperties)
        {
            glm::vec3 position = t.m_translate;
            cameraComponentPtr = std::make_unique<CameraComponent>(inLookAt, position, inViewAABB);
        }

        virtual void update() override
        {

        }

    private:
        std::unique_ptr<CameraComponent> cameraComponentPtr = nullptr;
    };
}
