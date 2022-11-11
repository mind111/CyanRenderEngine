#pragma once

#include <unordered_map>

#include "Common.h"
#include "glm/glm.hpp"
#include "glew.h"
#include "RenderableScene.h"
#include "camera.h"
#include "SurfelBSH.h"
#include "SurfelSampler.h"

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
        virtual ~ManyViewGI() { }

        void initialize();
        void setup(const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer);
        void render(RenderTarget* sceneRenderTarget, const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer);

        virtual void customInitialize() { }
        virtual void customSetup(const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) { }
        virtual void customRender(RenderTarget* sceneRenderTarget, RenderTarget* visRenderTarget) { }
        virtual void customRenderScene(const Hemicube& hemicube, const PerspectiveCamera& camera);
        virtual void customUI() {}
        virtual void reset();

        void gatherRadiance(const Hemicube& hemicube, u32 hemicubeIdx, bool jitter, const SceneRenderable& scene, TextureCubeRenderable* radianceCubemap);
        void placeHemicubes(Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer);
        void progressiveBuildRadianceAtlas(SceneRenderable& sceneRenderable);
        void resampleRadiance(TextureCubeRenderable* radianceCubemap, glm::uvec2 index);
        void convolveIrradiance(const glm::ivec2& hemicubeCoord, TextureCubeRenderable* radianceCubemap);
        void convolveIrradianceBatched();
        // visualizations
        void visualizeHemicubes(RenderTarget* dstRenderTarget);
        void visualizeIrradiance(RenderTarget* dstRenderTarget);

        // constants
        const i32 perFrameWorkload = 4;
        const u32 finalGatherRes = 16;
        const u32 resampledMicroBufferRes = 24;
        glm::uvec2 irradianceAtlasRes = glm::uvec2(64, 36);
        glm::uvec2 radianceAtlasRes = irradianceAtlasRes * resampledMicroBufferRes;

        struct VisualizationBuffers {
            glm::uvec2 resolution = glm::uvec2(1280, 720);
            Texture2DRenderable* shared = nullptr;
        } visualizations;

        struct Opts {
            bool bVisualizeHemicubes = false;
            bool bVisualizeIrradiance = false;
        } opts;

        GLuint hemicubeBuffer = -1;
        std::vector<Hemicube> hemicubes;
        std::vector<glm::vec3> jitteredSampleDirections;

        bool bBuilding = false;
        u32 renderedFrames = 0u;
        u32 maxNumHemicubes = 0u;

        std::vector<TextureCubeRenderable*> radianceCubemaps;
        RenderTarget* radianceAtlasRenderTarget = nullptr;
        Texture2DRenderable* radianceAtlas = nullptr;
        Texture2DRenderable* irradianceAtlas = nullptr;
    protected:
        Renderer* m_renderer = nullptr;
        GfxContext* m_gfxc = nullptr;
        std::unique_ptr<SceneRenderable> m_scene = nullptr;
    private:
        bool bInitialized = false;
    };

    class PointBasedManyViewGI : public ManyViewGI {
    public:
        PointBasedManyViewGI(Renderer* renderer, GfxContext* gfxc);
        ~PointBasedManyViewGI() { }

        virtual void customInitialize() override;
        virtual void customSetup(const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) override;
        virtual void customRender(RenderTarget* sceneRenderTarget, RenderTarget* visRenderTarget) override;
        virtual void customRenderScene(const Hemicube& hemicube, const PerspectiveCamera& camera) override;
        virtual void customUI() override;

        struct Visualizations {
            Texture2DRenderable* rasterizedSurfels = nullptr;
        } visualizations;

        struct Opts {
            bool bVisualizeSurfels = false;
            bool bVisualizeSurfelRendering = false;
        } opts;

    protected:
        void updateSurfelInstanceData();
        virtual void generateWorldSpaceSurfels();
        void cacheSurfelDirectLighting();

        std::vector<Surfel> surfels;
        ShaderStorageBuffer<DynamicSsboData<GpuSurfel>> surfelBuffer;
        ShaderStorageBuffer<DynamicSsboData<InstanceDesc>> surfelInstanceBuffer;
        bool bVisualizeSurfels = false;

    private:
        void rasterizeSurfelScene(Texture2DRenderable* outSceneColor, const RenderableCamera& camera);
        enum class VisMode : u32 {
            kAlbedo = 0,
            kRadiance,
            kDebugColor,
            kCount
        } m_visMode;
        void visualizeSurfels(RenderTarget* visRenderTarget);

        bool bInitialized = false;
        const f32 kSurfelRadius = 0.1f;
    };


    /** note - @min:
    * An implementation of the paper "Micro-Rendering for Scalable, Parallel Final Gathering"
    */
    class MicroRenderingGI : public PointBasedManyViewGI {
    public:
        MicroRenderingGI(Renderer* renderer, GfxContext* gfxc);
        ~MicroRenderingGI() { }

        virtual void customInitialize() override;
        virtual void customSetup(const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) override;
        virtual void customRender(RenderTarget* sceneRenderTarget, RenderTarget* visRenderTarget) override;
        virtual void customUI() override;

    private:
        virtual void generateWorldSpaceSurfels() override;
        void microRendering(const PerspectiveCamera& camera, SurfelBSH& surfelBSH);
        bool bVisualizeSurfelTree = false;

        bool bInitialized = false;
        SurfelSampler m_surfelSampler;
        SurfelBSH m_surfelBSH;
    };
}