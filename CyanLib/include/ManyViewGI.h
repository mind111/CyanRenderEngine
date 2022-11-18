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

        struct Image {
            glm::uvec2 resolution = glm::uvec2(128, 72);
            Texture2DRenderable* irradiance = nullptr;
            Texture2DRenderable* radiance = nullptr;
            std::vector<Hemicube> hemicubes;
        } m_image;

        ManyViewGI(Renderer* renderer, GfxContext* ctx);
        virtual ~ManyViewGI() { }

        void initialize();
        void setup(const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer);
        void render(RenderTarget* sceneRenderTarget, const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer);

        virtual void customInitialize() { }
        virtual void customSetup(const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) { }
        virtual void customRender(const RenderableCamera& camera, RenderTarget* sceneRenderTarget, RenderTarget* visRenderTarget) { }
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
        glm::uvec2 irradianceAtlasRes = glm::uvec2(128, 72);
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
        static TextureCubeRenderable* m_sharedRadianceCubemap;
        bool bInitialized = false;
    };

    class PointBasedManyViewGI : public ManyViewGI {
    public:
        PointBasedManyViewGI(Renderer* renderer, GfxContext* gfxc);
        ~PointBasedManyViewGI() { }

        virtual void customInitialize() override;
        virtual void customSetup(const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) override;
        virtual void customRender(const RenderableCamera& camera, RenderTarget* sceneRenderTarget, RenderTarget* visRenderTarget) override;
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

    // todo: maybe implement Pixar's PBGI on cpu?
    // todo: view frustum culling
    // todo: address holes
    // todo: lighting
    // todo: instead of tracing spheres, trace disk at leaf nodes
    struct SoftwareMicroBuffer {
        SoftwareMicroBuffer(i32 microBufferRes);
        ~SoftwareMicroBuffer() { }

        // hybrid render
        void render(const RenderableCamera& inCamera, const SurfelBSH& surfelBSH);
        // raytrace render
        void raytrace(const RenderableCamera& inCamera, const SurfelBSH& surfelBSH);
        void visualize(Renderer* renderer, RenderTarget* visRenderTarget);
        Texture2DRenderable* getVisualization() { return visualization; }
    private:
        void clear();
        void traverseBSH(const SurfelBSH& surfelBSH, i32 nodeIndex, const RenderableCamera& camera);
        void postTraversal(const RenderableCamera& camera, const SurfelBSH& surfelBSH);
        void raytraceInternal(const glm::vec3& ro, const glm::vec3& rd, const SurfelBSH& surfelBSH, i32 nodeIndex, f32& tmin, i32& hitNode);
        f32 solidAngleOfSphere(const glm::vec3& p, const glm::vec3& q, f32 r);
        f32 calcCubemapTexelSolidAngle(const glm::vec3& d, f32 cubemapRes);
        bool isVisible() { }
        bool isInViewport(const SurfelBSH::Node& node, glm::vec2& outPixelCoord, const glm::mat4& view, const glm::mat4& projection);

        template <typename T>
        struct Buffer2D {
            Buffer2D() { }
            Buffer2D(i32 inRes)
                : resolution(inRes) {
                buffer = new T[inRes * inRes];
            }

            T* data() {
                return buffer;
            }

            T* operator[](i32 rowIndex) {
                assert(rowIndex < resolution);
                return &buffer[rowIndex * resolution];
            }

            i32 resolution;
            T* buffer = nullptr;
        };

        i32 resolution = 16;
        Buffer2D<glm::vec3> color;
        Buffer2D<f32> depth;
        Buffer2D<i32> indices;
        Buffer2D<i32> postTraversalBuffer;
        std::vector<SurfelBSH::Node> postTraversalList;

        Texture2DRenderable* gpuTexture = nullptr;
        Texture2DRenderable* visualization = nullptr;
    };

    // todo: implement this!
    struct HardwareMicroBuffer {

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
        virtual void customRender(const RenderableCamera& camera, RenderTarget* sceneRenderTarget, RenderTarget* visRenderTarget) override;
        virtual void customUI() override;

    private:
        virtual void generateWorldSpaceSurfels() override;
        void softwareMicroRendering(const RenderableCamera& camera, SurfelBSH& surfelBSH);
        void hardwareMicroRendering(const RenderableCamera& camera, SurfelBSH& surfelBSH);

        bool bVisualizeSurfelSampler = false;
        bool bVisualizeMicroBuffer = false;
        bool bVisualizeSurfelBSH = false;
        bool bInitialized = false;
        SurfelSampler m_surfelSampler;
        SurfelBSH m_surfelBSH;
        SoftwareMicroBuffer m_softwareMicroBuffer;
    };
}