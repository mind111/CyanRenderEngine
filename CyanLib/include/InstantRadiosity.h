#pragma once

#include <glew/glew.h>

#include "Common.h"

namespace Cyan {
    class Renderer;
    struct RenderableScene;
    struct Texture2D;
    struct RenderTarget;

    /**
    * brief: Instant Radiosity implementation
    */
    class InstantRadiosity {
    public:
        InstantRadiosity();
        ~InstantRadiosity() { }

        void initialize();
        Texture2D* render(Renderer* renderer, RenderableScene& renderableScene, const glm::uvec2& renderResolution);
        Texture2D* render(Renderer* renderer, RenderableScene& renderableScene, Texture2D* sceneDepthBuffer, Texture2D* sceneNormalBuffer, const glm::uvec2& renderResolution);
        void visualizeVPLs(Renderer* renderer, RenderTarget* renderTarget, RenderableScene& renderableScene);
    private:
        void generateVPLs(Renderer* renderer, RenderableScene& renderableScene, const glm::uvec2& renderResolution);
        void buildVPLShadowMaps(Renderer* renderer, RenderableScene& renderableScene);
        void buildVPLVSMs(Renderer* renderer, RenderableScene& renderableScene);
        void renderInternal(Renderer* renderer, RenderableScene& renderableScene, Texture2D* output);
        void renderInternal(Renderer* renderer, RenderableScene& renderableScene, Texture2D* sceneDepthBuffer, Texture2D* sceneNormalBuffer, Texture2D* output);
        void renderUI(Renderer* renderer, Texture2D* output);

        enum class VPLShadowAlgorithm : i32 {
            kBasic = 0,
            kVSM,
            kCount
        } m_shadowAlgorithm = VPLShadowAlgorithm::kBasic;

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
        i32 activeVPLs = 1;
        static const int kMaxNumVPLs = 1024;
        u32 numGeneratedVPLs = 0;
        VPL VPLs[kMaxNumVPLs];

        // basic VPL shadow
        GLuint VPLShadowCubemaps[kMaxNumVPLs];
        Texture2D* VPLOctShadowMaps[kMaxNumVPLs] = { 0 };
        u64 VPLShadowHandles[kMaxNumVPLs];
        
        // VPL shadow using vsm
        GLuint VPLVSMs[kMaxNumVPLs];
        Texture2D* VPLOctVSMs[kMaxNumVPLs] = { 0 };
        u64 VPLVSMHandles[kMaxNumVPLs];

        GLuint VPLShadowHandleBuffer;

        // VPL camera projection constants
        const f32 fov = 90.f;
        const f32 aspectRatio = 1.f;
        const f32 nearClippingPlane = 0.005f;
        const f32 farClippingPlane = 20.f;
        bool bIndirectVisibility = true;
        bool bRegenerateVPLs = true;
    };
}
