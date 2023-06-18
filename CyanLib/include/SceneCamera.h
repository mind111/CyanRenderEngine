#pragma once

#include "Camera.h"

namespace Cyan
{
    class Scene;
    class SceneRender;
    class ProgramPipeline;

    /**
     * A camera used for scene rendering
     */
    class SceneCamera
    {
    public:
        enum class RenderMode
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

        struct ViewParameters
        {
            void update(const SceneCamera& camera);
            void setShaderParameters(ProgramPipeline* p) const;

            glm::uvec2 renderResolution;
            f32 aspectRatio;
            glm::mat4 viewMatrix;
            glm::mat4 projectionMatrix;
            glm::vec3 cameraPosition;
            glm::vec3 cameraLookAt;
            glm::vec3 cameraRight;
            glm::vec3 cameraForward;
            glm::vec3 cameraUp;
            i32 frameCount;
            f32 elapsedTime;
            f32 deltaTime;
        };

        SceneCamera(const glm::uvec2& resolution);
        ~SceneCamera();

        void render();

        void turnOn() { m_bPower = true; }
        void turnOff() { m_bPower = false; }

        void setScene(Scene* scene);
        void setResolution(const glm::uvec2& resolution) { m_resolution = resolution; }
        void setProjectionMode(const Camera::ProjectionMode& mode) { m_camera.setProjectionMode(mode); }
        void setRenderMode(const RenderMode& mode) { m_renderMode = mode; }

        bool m_bPower = true;
        u32 m_numRenderedFrames = 0u;
        f32 m_elapsedTime = 0.f;
        f32 m_deltaTime = 0.f;
        glm::uvec2 m_resolution;
        RenderMode m_renderMode;

        Scene* m_scene = nullptr;
        Camera m_camera;
        std::unique_ptr<SceneRender> m_render = nullptr;
        ViewParameters m_viewParameters;
    };

    #define VM_RESOLVED_SCENE_COLOR SceneCamera::RenderMode::kResolvedSceneColor
    #define VM_SCENE_COLOR SceneCamera::RenderMode::kSceneColor
    #define VM_SCENE_ALBEDO SceneCamera::RenderMode::kSceneAlbedo
    #define VM_SCENE_DEPTH SceneCamera::RenderMode::kSceneDepth
    #define VM_SCENE_NORMAL SceneCamera::RenderMode::kSceneNormal
    #define VM_SCENE_DIRECT_LIGHTING SceneCamera::RenderMode::kSceneDirectLighting
    #define VM_SCENE_INDIRECT_LIGHTING SceneCamera::RenderMode::kSceneIndirectLighting
    #define VM_SCENE_LIGHTING_ONLY SceneCamera::RenderMode::kSceneLightingOnly
}
