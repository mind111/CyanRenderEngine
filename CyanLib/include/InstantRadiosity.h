#pragma once

#include "glew.h"

#include "Common.h"

namespace Cyan {
    class Renderer;
    struct RenderableScene;
    struct RenderTexture2D;
    struct Texture2DRenderable;
    struct RenderTarget;

    /**
    * brief: Instant Radiosity implementation
    */
    class InstantRadiosity {
    public:
        InstantRadiosity();
        ~InstantRadiosity() { }

        void initialize();
        void render(Renderer* renderer, RenderableScene& renderableScene, RenderTexture2D* output);
        void render(Renderer* renderer, RenderableScene& renderableScene, RenderTexture2D* sceneDepthBuffer, RenderTexture2D* sceneNormalBuffer, RenderTexture2D* output);
        void visualizeVPLs(Renderer* renderer, RenderTarget* renderTarget, RenderableScene& renderableScene);
    private:
        void generateVPLs(Renderer* renderer, RenderableScene& renderableScene, const glm::uvec2& renderResolution);
        void buildVPLShadowMaps(Renderer* renderer, RenderableScene& renderableScene);
        void renderInternal(Renderer* renderer, RenderableScene& renderableScene, RenderTexture2D* output);
        void renderInternal(Renderer* renderer, RenderableScene& renderableScene, RenderTexture2D* sceneDepthBuffer, RenderTexture2D* sceneNormalBuffer, RenderTexture2D* output);

        struct VPL {
            glm::vec4 position;
            glm::vec4 normal;
            glm::vec4 flux;
        };

        GLuint VPLBuffer;
        GLuint VPLCounter;
        GLuint depthCubeMap;
        static const i32 kVPLShadowResolution = 64;
        static const i32 kOctShadowMapResolution = 256;
        static const int kMaxNumVPLs = 1024;
        u32 numGeneratedVPLs = 0;
        VPL VPLs[kMaxNumVPLs];
        // VPL shadow
        GLuint VPLShadowCubemaps[kMaxNumVPLs];
        Texture2DRenderable* VPLOctShadowMaps[kMaxNumVPLs] = { 0 };
        GLuint VPLShadowHandleBuffer;
        u64 VPLShadowHandles[kMaxNumVPLs];
        // VPL camera projection constants
        const f32 fov = 90.f;
        const f32 aspectRatio = 1.f;
        const f32 nearClippingPlane = 0.005f;
        const f32 farClippingPlane = 20.f;
        bool bIndirectVisibility = true;
        bool bRegenerateVPLs = true;
    };
}
