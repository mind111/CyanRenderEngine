#pragma once

#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Common.h"
#include "Geometry.h"
#include "Transform.h"

namespace Cyan
{
    class SceneRender;

    class Camera
    {
    public:

        enum class ViewMode
        {
            kSceneColor = 0,
            kResolvedSceneColor,
            kSceneAlbedo,
            kSceneDepth,
            kSceneNormal,
            kSceneDirectLighting,
            kSceneIndirectLighting,
            kSceneLightingOnly,
            kWireframe,
            kCount
        };

        glm::mat4 view() const
        { 
            return glm::lookAt(m_position, m_position + forward(), m_worldUp);
        }

        virtual glm::mat4 projection() const = 0;
        glm::vec3 forward() const { return m_forward; }
        glm::vec3 right() const { return m_right; }
        glm::vec3 up() const { return m_up; }

        Camera();
        Camera(const Transform& localTransform, const glm::vec3& worldUp, const glm::vec2& renderResolution, const Camera::ViewMode& viewMode);
        virtual ~Camera();

        void setSceneRender(SceneRender* sceneRender);

        bool bEnabledForRendering = true;

        glm::vec3 m_position;
        glm::vec3 m_worldUp;

        static constexpr glm::vec3 localRight = glm::vec3(1.f, 0.f, 0.f);
        static constexpr glm::vec3 localForward = glm::vec3(0.f, 0.f, -1.f);
        static constexpr glm::vec3 localUp = glm::vec3(0.f, 1.f, 0.f);

        glm::vec3 m_right;   // x 
        glm::vec3 m_forward; //-z
        glm::vec3 m_up;      // y

        glm::uvec2 m_renderResolution;
        ViewMode m_viewMode;
    protected:
        SceneRender* m_sceneRender = nullptr;
    };

    #define VM_RESOLVED_SCENE_COLOR Camera::ViewMode::kResolvedSceneColor
    #define VM_SCENE_COLOR Camera::ViewMode::kSceneColor
    #define VM_SCENE_ALBEDO Camera::ViewMode::kSceneAlbedo
    #define VM_SCENE_DEPTH Camera::ViewMode::kSceneDepth
    #define VM_SCENE_NORMAL Camera::ViewMode::kSceneNormal
    #define VM_SCENE_DIRECT_LIGHTING Camera::ViewMode::kSceneDirectLighting
    #define VM_SCENE_INDIRECT_LIGHTING Camera::ViewMode::kSceneIndirectLighting
    #define VM_SCENE_LIGHTING_ONLY Camera::ViewMode::kSceneLightingOnly

    class PerspectiveCamera : public Camera
    {
    public:
        virtual glm::mat4 projection() const override 
        {
            return glm::perspective(glm::radians(fov), aspectRatio, n, f);
        }

        PerspectiveCamera()
            : Camera()
        {
            fov = 45.f;
            n = 0.5f;
            f = 128.f;
            aspectRatio = ((f32)m_renderResolution.x / m_renderResolution.y);
        }

        PerspectiveCamera(const Transform& localToWorld, const glm::vec3& worldUp, const glm::uvec2& renderResolution, const Camera::ViewMode& viewMode, f32 inFov, f32 inN, f32 inF)
            : Camera(localToWorld, worldUp, renderResolution, viewMode), fov(inFov), n(inN), f(inF), aspectRatio(renderResolution.x / renderResolution.y)
        {

        }

        f32 fov, n, f, aspectRatio;
    };

    class OrthographicCamera : public Camera
    {
    public:
        virtual glm::mat4 projection() const override
        {
            f32 left = m_viewVolume.pmin.x;
            f32 right = m_viewVolume.pmax.x;
            f32 bottom = m_viewVolume.pmin.y;
            f32 top = m_viewVolume.pmax.y;
            f32 zNear = m_viewVolume.pmax.z;
            f32 zFar = m_viewVolume.pmin.z;
            return glm::orthoLH(left, right, bottom, top, zNear, zFar);
        }

        OrthographicCamera()
            : Camera()
        {

        }

        OrthographicCamera(const Transform& localToWorld, const glm::vec3& worldUp, const glm::vec2& renderResolution, const BoundingBox3D& inViewAABB, const Camera::ViewMode& viewMode = VM_RESOLVED_SCENE_COLOR)
            : Camera(localToWorld, worldUp, renderResolution, viewMode), m_viewVolume(inViewAABB)
        {

        }

        // aabb of the view volume in view space
        BoundingBox3D m_viewVolume;
    };
}

