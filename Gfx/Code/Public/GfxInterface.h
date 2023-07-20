#pragma once

/**
 * Note: This file includes all the interfaces exposed by the Gfx module
 */

#include <memory>

#include "Gfx.h"
#include "CameraViewInfo.h"

namespace Cyan
{
    class IScene
    {
    public:
        virtual ~IScene() { }

        // factory method
        static GFX_API std::unique_ptr<IScene> create();
    };

    class ISceneRender
    {
    public:
    };

    struct SceneViewState
    {
        glm::uvec2 resolution;
        f32 aspectRatio;
        glm::mat4 viewMatrix;
        glm::mat4 prevFrameViewMatrix;
        glm::mat4 projectionMatrix;
        glm::mat4 prevFrameProjectionMatrix;
        glm::vec3 cameraPosition;
        glm::vec3 prevFrameCameraPosition;
        glm::vec3 cameraLookAt;
        glm::vec3 cameraRight;
        glm::vec3 cameraForward;
        glm::vec3 cameraUp;
        i32 frameCount;
        f32 elapsedTime;
        f32 deltaTime;
    };

    class ISceneRenderer
    {
    public:
        virtual ~ISceneRenderer() { }

        // factory method
        GFX_API static std::unique_ptr<ISceneRenderer> create();

        // interface
        virtual void render(ISceneRender* outRender, IScene* scene, const SceneViewState& viewState) = 0;
    };
}