#pragma once

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"

#include "Common.h"
#include "Component.h"
#include "Entity.h"

namespace Cyan
{
    struct ICamera
    {
        virtual glm::mat4 view() const
        { 
            return glm::lookAt(position, lookAt, worldUp);
        }

        virtual glm::mat4 projection() const = 0;
        glm::vec3 forward() const { return glm::normalize(lookAt - position); }
        glm::vec3 right() const { return glm::normalize(glm::cross(forward(), worldUp)); }
        glm::vec3 up() const { return glm::normalize(glm::cross(right(), forward())); }

        ICamera(const glm::vec3& inPosition, const glm::vec3& inLookAt, const glm::vec3& inWorldUp)
            : position(inPosition), lookAt(inLookAt), worldUp(inWorldUp)
        { }

        glm::vec3 position;
        glm::vec3 lookAt;
        glm::vec3 worldUp;
    };

    struct PerspectiveCamera : public ICamera
    {
        /* ICamera interface */
        virtual glm::mat4 projection() const override 
        {
            return glm::perspective(glm::radians(fov), aspectRatio, n, f);
        }

        PerspectiveCamera()
            : ICamera(glm::vec3(0.f, 1.f, -2.f), glm::vec3(0.f, 0.f, -4.f), glm::vec3(0.f, 1.f, 0.f)),
            fov(45.f),
            n(0.5f),
            f(150.f),
            aspectRatio(16.f / 9.f)
        {

        }

        PerspectiveCamera(const glm::vec3& inPosition, const glm::vec3& inLookAt, const glm::vec3& inWorldUp, f32 inFov, f32 inN, f32 inF, f32 inAspectRatio)
            : ICamera(inPosition, inLookAt, inWorldUp), fov(inFov), n(inN), f(inF), aspectRatio(inAspectRatio)
        {

        }

        f32 fov, n, f, aspectRatio;
    };

    struct OrthographicCamera : public ICamera
    {
        /* ICamera interface */
        virtual glm::mat4 projection() const override
        {
            f32 left = viewVolume.pmin.x;
            f32 right = viewVolume.pmax.x;
            f32 bottom = viewVolume.pmin.y;
            f32 top = viewVolume.pmax.y;
            f32 zNear = viewVolume.pmax.z;
            f32 zFar = viewVolume.pmin.z;
            return glm::orthoLH(left, right, bottom, top, zNear, zFar);
        }

        OrthographicCamera(const glm::vec3& inPosition, const glm::vec3& inLookAt, const glm::vec3& inWorldUp, const BoundingBox3D& inViewAABB)
            : ICamera(inPosition, inLookAt,  inWorldUp), viewVolume(inViewAABB)
        {

        }

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

        CameraComponent(const glm::vec3& inPosition, const glm::vec3& inLookAt, const glm::vec3& inWorldUp, f32 inFov, f32 inN, f32 inF, f32 inAspectRatio)
            : projectionType(ProjectionType::kPerspective)
        {
            cameraPtr = std::make_unique<PerspectiveCamera>(inPosition, inLookAt, inWorldUp, inFov, inN, inF, inAspectRatio);
        }

        CameraComponent(const glm::vec3& inPosition, const glm::vec3& inLookAt, const glm::vec3& inWorldUp, const BoundingBox3D& inViewAABB)
            : projectionType(ProjectionType::kOrthographic)
        {
            cameraPtr = std::make_unique<OrthographicCamera>(inPosition, inLookAt, inWorldUp, inViewAABB);
        }

        ICamera* getCamera() { return cameraPtr.get(); }

        const glm::mat4& view() { return cameraPtr->view(); }
        const glm::mat4& projection() { return cameraPtr->projection(); }

    private:
        std::unique_ptr<ICamera> cameraPtr;
    };

    // todo: camera's lookAt should be determined from camera's facing direction (forward vector)
    struct CameraEntity : public Entity
    {
        // perspective
        CameraEntity(Scene* scene, const char* inName, const Transform& t, const glm::vec3& inLookAt, const glm::vec3& inWorldUp,f32 inFov, f32 inN, f32 inF, f32 inAspectRatio, Entity* inParent = nullptr, u32 inProperties = (EntityFlag_kDynamic))
            : Entity(scene, inName, t, inParent, inProperties)
        {
            glm::vec3 position = t.m_translate;
            cameraComponentPtr = std::make_unique<CameraComponent>(position, inLookAt, inWorldUp, inFov, inN, inF, inAspectRatio);
        }

        // ortho
        CameraEntity(Scene* scene, const char* inName, const Transform& t, const glm::vec3& inLookAt, const BoundingBox3D& inViewAABB, glm::vec3& inWorldUp, Entity* inParent = nullptr, u32 inProperties = (EntityFlag_kDynamic))
            : Entity(scene, inName, t, inParent, inProperties)
        {
            glm::vec3 position = t.m_translate;
            cameraComponentPtr = std::make_unique<CameraComponent>(position, inLookAt, inWorldUp, inViewAABB);
        }

        /* Entity interface */
        virtual void update() override
        {

        }

        const glm::mat4& view() { return cameraComponentPtr->view(); }
        const glm::mat4& projection() { return cameraComponentPtr->projection(); }
        ICamera* getCamera() { return cameraComponentPtr->getCamera(); }

        /* Camera movements */
        void moveForward()
        { 
            ICamera* camera = getCamera();
            getCamera()->position += camera->forward() * 0.1f;
        }

        void moveLeft()
        {
            ICamera* camera = getCamera();
            getCamera()->position -= camera->right() * 0.1f;
        }
        void moveRight()
        {
            ICamera* camera = getCamera();
            getCamera()->position += camera->right() * 0.1f;
        }

        void moveBack()
        {
            ICamera* camera = getCamera();
            getCamera()->position -= camera->forward() * 0.1f;
        }

        void orbit(f32 phi, f32 theta);
        void rotate();
        void zoom(f32 distance);

    private:
        std::unique_ptr<CameraComponent> cameraComponentPtr = nullptr;
    };
}
