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

    class ManyViewGI {
    public:
        struct Hemicube {
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
            const f32 aperture = 5; // in degrees
        } m_sampler;

        ManyViewGI(Renderer* renderer, GfxContext* ctx);
        ~ManyViewGI() { }

        virtual void initialize();
        virtual void setup(const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer);
        virtual void run(RenderTarget* outRenderTarget, const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer);
        virtual void render(RenderTarget* outRenderTarget);
        virtual void reset();

        void placeHemicubes(Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer);
        void progressiveBuildRadianceAtlas(SceneRenderable& sceneRenderable);
        void gatherRadiance(const Hemicube& hemicube, u32 hemicubeIdx, bool jitter, const SceneRenderable& scene, TextureCubeRenderable* radianceCubemap);
        void resampleRadiance(TextureCubeRenderable* radianceCubemap, glm::uvec2 index);
        void convolveIrradiance(const glm::ivec2& hemicubeCoord, TextureCubeRenderable* radianceCubemap);
        void convolveIrradianceBatched();
        void visualizeHemicubes(RenderTarget* outRenderTarget);
        void visualizeIrradiance();
        void hierarchicalRefinement();

        Texture2DRenderable* visualizeIrradianceBuffer = nullptr;

        bool bShowHemicubes = false;
        GLuint radianceCubeBuffer;
        Hemicube* hemicubes = nullptr;
        glm::vec3* jitteredSampleDirections = nullptr;

        bool bBuildingRadianceAtlas = false;
        u32 renderedFrames = 0u;
        u32 maxNumHemicubes = 0u;

        const u32 microBufferRes = 16;
        const u32 resampledMicroBufferRes = 24;
        glm::uvec2 irradianceAtlasRes = glm::uvec2(40, 22);
        TextureCubeRenderable** radianceCubemaps = nullptr;
        RenderTarget* radianceAtlasRenderTarget = nullptr;
        Texture2DRenderable* radianceAtlas = nullptr;
        Texture2DRenderable* positionAtlas = nullptr;
        Texture2DRenderable* irradianceAtlas = nullptr;

        const i32 perFrameWorkload = 4;
        bool bInitialized = false;
    protected:
        Renderer* m_renderer = nullptr;
        GfxContext* m_gfxc = nullptr;
        std::unique_ptr<SceneRenderable> m_scene = nullptr;
    };

    /** note - @min:
    * An implementation of hemicube GI using geometry shader and layered rendering
    */
    class ManyViewGIBatched : public ManyViewGI {
    public:
        ManyViewGIBatched(Renderer* renderer, GfxContext* ctx);
        ~ManyViewGIBatched() {}

        virtual void initialize() override;
        virtual void run(RenderTarget* outRenderTarget, const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) { }
        virtual void render(RenderTarget* outRenderTarget) override;
        virtual void reset() override;

        void sampleRadianceMultiView(const SceneRenderable& scene, u32 startIndex, i32 count);
        void debugDrawHemicubeMultiView(u32 batchId, u32 index, RenderTarget* outRenderTarget);
        void visualizeHemicubeMultiView(Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer, const SceneRenderable& scene, RenderTarget* outRenderTarget);

        ShaderStorageBuffer<DynamicSsboData<glm::mat4>> multiViewBuffer;
        TextureCubeRenderable* hemicubeMultiView = nullptr;
        Texture2DRenderable* frameHemicubeAtlas = nullptr;
        GLuint hemicubeMultiViewDepthBuffer;
        GLuint hemicubeMultiViewFramebuffer;
    };

    /** note - @min:
    * An implementation of the paper "Micro-Rendering for Scalable, Parallel Final Gathering"
    */
    class MicroRenderingGI {
    public:
        MicroRenderingGI(Renderer* renderer, const SceneRenderable& inScene);
        ~MicroRenderingGI() { }

        void generateWorldSpaceSurfels();
        void buildSurfelBVH() { }

    private:
        Renderer* m_renderer = nullptr;
        std::unique_ptr<SceneRenderable> m_scene = nullptr;
    };
}