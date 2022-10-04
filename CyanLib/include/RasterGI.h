#pragma once

#include "Common.h"
#include "glm/glm.hpp"
#include "glew.h"
#include "RenderableScene.h"

namespace Cyan {
    class Renderer;
    class GfxContext;
    struct Texture2DRenderable;
    struct TextureCubeRenderable;
    struct SceneRenderable;
    struct RenderTarget;

    class RasterGI {
    public:
        struct RadianceCube {
            glm::vec4 position;
            glm::vec4 normal;
        };

        // generate random directions with in a cone centered around the normal vector
        struct RandomizedCone {
            glm::vec3 sample(const glm::vec3& n) {
                glm::vec2 st = glm::vec2(uniformSampleZeroToOne(), uniformSampleZeroToOne());
                f32 coneRadius = glm::atan(glm::radians(aperture)) * h;
                // a point on disk
                f32 r = sqrt(st.x) * coneRadius;
                f32 theta = st.y * 2.f * M_PI;
                // pp is an uniform random point on the bottom of a cone centered at the normal vector in the tangent frame
                glm::vec3 pp = glm::vec3(glm::vec2(r * cos(theta), r * sin(theta)), 1.f);
                pp = glm::normalize(pp);
                // transform pp back to the world space
                return tangentToWorld(n) * pp;
            }

            const f32 h = 1.0;
            const f32 aperture = 30; // in degrees
        } m_sampler;

        RasterGI(Renderer* renderer, GfxContext* ctx);
        ~RasterGI() { }

        void initialize();
        void setup(const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer);
        void render(RenderTarget* outRenderTarget);
        void reset();

        void placeRadianceCubes(Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer);
        void progressiveBuildRadianceAtlas(SceneRenderable& sceneRenderable);
        void buildIrradianceAtlas();
        void sampleRadiance(const RadianceCube& radianceCube, u32 radianceCubeIdx, bool jitter, const SceneRenderable& scene, TextureCubeRenderable* radianceCubemap);
        void resampleRadianceCube(TextureCubeRenderable* radianceCubemap, glm::uvec2 index);
        void visualizeRadianceCube(Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer, const SceneRenderable& scene, RenderTarget* outRenderTarget);
        void debugDrawOneRadianceCube(const RadianceCube& radianceCube, TextureCubeRenderable* radianceCubemap, RenderTarget* outRenderTarget);
        void debugDrawRadianceCubes(RenderTarget* outRenderTarget);

        GLuint radianceCubeBuffer;
        RadianceCube* radianceCubes = nullptr;
        glm::vec3* jitteredSampleDirections = nullptr;

        bool bBuildingRadianceAtlas = false;
        u32 renderedFrames = 0u;
        u32 maxNumRadianceCubes = 0u;

        const u32 microBufferRes = 12;
        const u32 resampledMicroBufferRes = microBufferRes * 2;
        glm::uvec2 irradianceAtlasRes = glm::uvec2(160, 90);
        TextureCubeRenderable** radianceCubemaps = nullptr;
        RenderTarget* radianceAtlasRenderTarget = nullptr;
        Texture2DRenderable* radianceAtlas = nullptr;
        Texture2DRenderable* positionAtlas = nullptr;
        Texture2DRenderable* irradianceAtlas = nullptr;

        // a debug radiance cube for visualization purpose 
        glm::vec2 debugRadianceCubeScreenCoord = glm::vec2(.5f);
        RadianceCube debugRadianceCube;
        TextureCubeRenderable* debugRadianceCubemap = nullptr;

        bool bLockDebugRadianceCube = false;
        bool bInitialized = false;
    private:
        Renderer* m_renderer = nullptr;
        GfxContext* m_gfxc = nullptr;
        std::unique_ptr<SceneRenderable> m_scene = nullptr;
    };
}