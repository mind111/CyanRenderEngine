#pragma once

#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Common.h"
#include "Geometry.h"

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
            return glm::lookAt(m_position, m_lookAt, m_worldUp);
        }

        virtual glm::mat4 projection() const = 0;
        glm::vec3 forward() const { return glm::normalize(m_lookAt - m_position); }
        glm::vec3 right() const { return glm::normalize(glm::cross(forward(), m_worldUp)); }
        glm::vec3 up() const { return glm::normalize(glm::cross(right(), forward())); }

        Camera(const glm::vec3& inPosition, const glm::vec3& inLookAt, const glm::vec3& inWorldUp, const glm::vec2& renderResolution, const Camera::ViewMode& viewMode);

        void setSceneRender(SceneRender* sceneRender);

        bool bEnabledForRendering = true;

        glm::vec3 m_position;
        glm::vec3 m_lookAt;
        glm::vec3 m_worldUp;

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
            : Camera(glm::vec3(0.f, 1.f, -2.f), glm::vec3(0.f, 0.f, -4.f), glm::vec3(0.f, 1.f, 0.f), glm::vec2(1280, 720), VM_RESOLVED_SCENE_COLOR),
            fov(90.f),
            n(0.5f),
            f(128.f),
            aspectRatio(16.f / 9.f)
        {

        }

        PerspectiveCamera(const glm::vec3& inPosition, const glm::vec3& inLookAt, const glm::vec3& inWorldUp, const glm::uvec2& renderResolution, const Camera::ViewMode& viewMode, f32 inFov, f32 inN, f32 inF)
            : Camera(inPosition, inLookAt, inWorldUp, renderResolution, viewMode), fov(inFov), n(inN), f(inF), aspectRatio(renderResolution.x / renderResolution.y)
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

        OrthographicCamera(const glm::vec3& inPosition, const glm::vec3& inLookAt, const glm::vec3& inWorldUp, const glm::vec2& renderResolution, const BoundingBox3D& inViewAABB, const Camera::ViewMode& viewMode = VM_RESOLVED_SCENE_COLOR)
            : Camera(inPosition, inLookAt,  inWorldUp, renderResolution, viewMode), m_viewVolume(inViewAABB)
        {

        }

        // aabb of the view volume in view space
        BoundingBox3D m_viewVolume;
    };
}

