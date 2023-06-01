#pragma once

#include <unordered_map>

#include <glm/glm.hpp>
#include <glew/glew.h>

#include "Common.h"
#include "RenderableScene.h"
#include "camera.h"
#include "SurfelBSH.h"
#include "SurfelSampler.h"

namespace Cyan {
    class Renderer;
    class GfxContext;
    struct GfxTexture2D;
    struct GfxTextureCube;
    struct RenderableScene;
    struct RenderTarget;

    /** todo: improve rendering quality
    * add direct skylight 
    * add direct lighting
    * noise
    */ 

    /** todo: improve speed
    * render to 5 faces in one pass
    */ 
    class ManyViewGI {
    public:
        struct Hemicube {
            glm::vec4 m_position;
            glm::vec4 normal;
        };

        struct Image 
        {
            Image(const glm::uvec2& resolution, u32 inFinalGatherRes);
            ~Image() { }

            void initialize();
            virtual void setup(const RenderableScene& inScene, GfxTexture2D* depthBuffer, GfxTexture2D* normalBuffer);
            virtual void clear() { }
            virtual void render(ManyViewGI* gi);

            void writeRadiance(GfxTextureCube* radianceCubemap, const glm::ivec2& texCoord);
            void writeIrradiance(GfxTextureCube* radianceCubemap, const Hemicube& hemicube, const glm::ivec2& texCoord);
            bool finished() { return nextHemicube >= hemicubes.size(); }
            void compose();
            void renderDirectLighting();
            void renderIndirectLighting();
            void visualizeHemicubes(RenderTarget* dstRenderTarget);
            void visualizeIrradiance(RenderTarget* dstRenderTarget);

            u32 finalGatherRes;
            u32 radianceRes;
            glm::uvec2 irradianceRes;
            glm::uvec2 radianceAtlasRes;
            GfxTexture2D* irradiance = nullptr;
            GfxTexture2D* radianceAtlas = nullptr;
            // "gBuffer"
            GfxTexture2D* normal = nullptr;
            GfxTexture2D* m_position = nullptr;
            GfxTexture2D* albedo = nullptr;
            GfxTexture2D* directLighting = nullptr;
            GfxTexture2D* indirectLighting = nullptr;
            GfxTexture2D* sceneColor = nullptr;
            GfxTexture2D* composed = nullptr;
        protected:
            struct Instance 
            {
                glm::mat4 transform;
                glm::ivec2 atlasTexCoord;
                glm::ivec2 padding;
            };
            ShaderStorageBuffer<DynamicSsboData<Instance>> hemicubeInstanceBuffer;

            struct Axis 
            {
                glm::mat4 v0;
                glm::mat4 v1;
                glm::vec4 albedo;
            };
            ShaderStorageBuffer<DynamicSsboData<Axis>> tangentFrameInstanceBuffer;

            u32 kMaxNumHemicubes = 0;
            u32 numGeneratedHemicubes = 0;
            GLuint hemicubeBuffer;
            std::vector<Hemicube> hemicubes;
            std::vector<glm::vec3> jitteredSampleDirections;
            std::unique_ptr<RenderableScene> scene;
        private:
            void generateHemicubes(GfxTexture2D* depthBuffer, GfxTexture2D* normalBuffer);
            void generateSampleDirections();

            u32 nextHemicube = 0;
            bool bInitialized = false;
        };

        ManyViewGI(Renderer* renderer, GfxContext* ctx);
        virtual ~ManyViewGI() { }

        void initialize();
        void setup(const RenderableScene& scene, GfxTexture2D* depthBuffer, GfxTexture2D* normalBuffer);
        void render(RenderTarget* sceneRenderTarget, const RenderableScene& scene, GfxTexture2D* depthBuffer, GfxTexture2D* normalBuffer);

        // extension points
        virtual void customInitialize();
        virtual void customSetup(const RenderableScene& scene, GfxTexture2D* depthBuffer, GfxTexture2D* normalBuffer) { }
        virtual void customRender(const RenderableScene::Camera& camera, RenderTarget* sceneRenderTarget, RenderTarget* visRenderTarget) { }
        virtual void customRenderScene(RenderableScene& scene, const Hemicube& hemicube, const PerspectiveCamera& camera);
        virtual void customUI() {}

        GfxTextureCube* finalGathering(const Hemicube& hemicube, RenderableScene& scene, bool jitter=false, const glm::vec3& jitteredSampleDirection=glm::vec3(0.f));

        struct VisualizationBuffers {
            glm::uvec2 resolution = glm::uvec2(640, 360);
            GfxTexture2D* shared = nullptr;
        } visualizations;

        struct Opts {
            bool bVisualizeHemicubes = false;
            bool bVisualizeIrradiance = false;
        } opts;

        const u32 kFinalGatherRes = 16;
        const glm::uvec2 kImageResolution = glm::uvec2(320, 180);
    protected:

        Renderer* m_renderer = nullptr;
        GfxContext* m_gfxc = nullptr;
        std::unique_ptr<RenderableScene> m_scene = nullptr;
        GfxTextureCube* m_sharedRadianceCubemap = nullptr;
        Image* m_image = nullptr;
    private:
        bool bInitialized = false;
    };

    class PointBasedManyViewGI : public ManyViewGI {
    public:
        struct Image : public ManyViewGI::Image {
            virtual void setup(const RenderableScene& inScene, GfxTexture2D* depthBuffer, GfxTexture2D* normalBuffer) override { }
        private: 
            std::vector<Surfel> surfels;
        };

        PointBasedManyViewGI(Renderer* renderer, GfxContext* gfxc);
        ~PointBasedManyViewGI() { }

        virtual void customInitialize() override;
        virtual void customSetup(const RenderableScene& scene, GfxTexture2D* depthBuffer, GfxTexture2D* normalBuffer) override;
        virtual void customRender(const RenderableScene::Camera& camera, RenderTarget* sceneRenderTarget, RenderTarget* visRenderTarget) override;
        virtual void customRenderScene(RenderableScene& scene, const Hemicube& hemicube, const PerspectiveCamera& camera) override;
        virtual void customUI() override;

        struct Visualizations {
            GfxTexture2D* rasterizedSurfels = nullptr;
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
        ShaderStorageBuffer<DynamicSsboData<Instance>> surfelInstanceBuffer;
        bool bVisualizeSurfels = false;

    private:
        void rasterizeSurfelScene(GfxTexture2D* outSceneColor, const RenderableScene::Camera& camera);
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
        void render(const RenderableScene::Camera& inCamera, const SurfelBSH& surfelBSH);
        // raytrace render
        void raytrace(const RenderableScene::Camera& inCamera, const SurfelBSH& surfelBSH);
        void visualize(Renderer* renderer, RenderTarget* visRenderTarget);
        GfxTexture2D* getVisualization() { return visualization; }
    private:
        void clear();
        void traverseBSH(const SurfelBSH& surfelBSH, i32 nodeIndex, const RenderableScene::Camera& camera);
        void postTraversal(const RenderableScene::Camera& camera, const SurfelBSH& surfelBSH);
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

        GfxTexture2D* gpuTexture = nullptr;
        GfxTexture2D* visualization = nullptr;
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
        virtual void customSetup(const RenderableScene& scene, GfxTexture2D* depthBuffer, GfxTexture2D* normalBuffer) override;
        virtual void customRender(const RenderableScene::Camera& camera, RenderTarget* sceneRenderTarget, RenderTarget* visRenderTarget) override;
        virtual void customUI() override;

    private:
        virtual void generateWorldSpaceSurfels() override;
        void softwareMicroRendering(const RenderableScene::Camera& camera, SurfelBSH& surfelBSH);
        void hardwareMicroRendering(const RenderableScene::Camera& camera, SurfelBSH& surfelBSH);

        bool bVisualizeSurfelSampler = false;
        bool bVisualizeMicroBuffer = false;
        bool bVisualizeSurfelBSH = false;
        bool bInitialized = false;
        SurfelSampler m_surfelSampler;
        SurfelBSH m_surfelBSH;
        SoftwareMicroBuffer m_softwareMicroBuffer;
    };
}