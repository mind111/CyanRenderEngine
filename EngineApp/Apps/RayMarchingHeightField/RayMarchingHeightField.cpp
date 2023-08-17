#include <random>

#include "stbi/stb_image_write.h"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "RayMarchingHeightField.h"
#include "World.h"
#include "SceneCameraEntity.h"
#include "SceneCameraComponent.h"
#include "CameraControllerComponent.h"
#include "LightEntities.h"
#include "Noise.h"
#include "AssetManager.h"
#include "Image.h"
#include "GfxModule.h"
#include "SceneRender.h"
#include "RenderPass.h"
#include "RenderingUtils.h"
#include "Texture.h"
#include "Material.h"
#include "StaticMesh.h"
#include "StaticMeshComponent.h"
#include "StaticMeshEntity.h"
#include "Shader.h"
#include "Color.h"
#include "glew/glew.h"

// todo: hierachical tracing
// todo: restir sky light
// todo: SVGF denoising
namespace Cyan
{
    class HeightField
    {
    public:
        HeightField()
            : m_worldSpacePosition(glm::vec3(0.f))
            , m_scale(glm::vec3(1.f))
            , m_aabb()
            , m_heightValues(size * size)
        {
            /**
             * Default dimension of the height field in its local space
             */
            m_aabb.bound(glm::vec3(-1.f, 0.f, -1.f));
            m_aabb.bound(glm::vec3( 1.f, 0.f,  1.f));
        }

        ~HeightField()
        {
        }

        void build()
        {
        }

        static constexpr i32 size = 256;

        glm::vec3 m_worldSpacePosition = glm::vec3(0.f);
        glm::vec3 m_scale = glm::vec3(1.f);
        AxisAlignedBoundingBox3D m_aabb;
        std::vector<f32> m_heightValues;
    };

#define APP_RESOURCE_PATH  "C:/dev/cyanRenderEngine/EngineApp/Apps/RayMarchingHeightField/Resources/"
#define APP_SHADER_PATH  "C:/dev/cyanRenderEngine/EngineApp/Apps/RayMarchingHeightField/Shader/"
#define APP_MATERIAL_PATH  "C:/dev/cyanRenderEngine/EngineApp/Apps/RayMarchingHeightField/Materials/"

    RayMarchingHeightFieldApp::RayMarchingHeightFieldApp(i32 windowWidth, i32 windowHeight)
        : App("RayMarchingHeightField", windowWidth, windowHeight)
    {

    }

    RayMarchingHeightFieldApp::~RayMarchingHeightFieldApp()
    {

    }

    void RayMarchingHeightFieldApp::update(World* world)
    {
        // these are only used on the render thread
        static glm::vec3 position = glm::vec3(0.f);
        static glm::vec3 scale = glm::vec3(5.f, 5.f, 5.f);
        static glm::vec3 groundAlbedo = glm::vec3(187.f, 106.f, 33.f) / 255.f;
        // noise shape control parameters
        static i32 rotationRandomSeed = 0;
        static f32 amplitude = 1.f;
        static f32 frequency = .5f;
        static i32 numOctaves = 16;
        static f32 persistence = .5f;

        static f32 snowLayerAmplitude = 1.f;
        static f32 snowLayerFrequency = .5f;
        static i32 snowLayerNumOctaves = 4;
        static f32 snowLayerPersistance = .5f;
        static f32 snowHeightBias = 0.0f;
        static f32 snowBlendMin = .5f;
        static f32 snowBlendMax = 0.9f;

        static f32 cloudAmplitude = 1.f;
        static f32 cloudFrequency = 4.f;
        static i32 cloudNumOctaves = 4;
        static f32 cloudPersistence = .5f;

        static i32 maxNumRayMarchingSteps = 512;

        static glm::vec3 debugRo = glm::vec3(0.f, 10.f, -2.f);
        static glm::vec3 debugRd = glm::vec3(0.f, -1.f, 0.f);

#if GPU_NOISE
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "GenerateProceduralHeightField", [this](Frame& frame) {
                GPU_DEBUG_SCOPE(marker, "Generate Procedural HeightField");

                // fix the random seed
                srand(rotationRandomSeed);
                for (i32 i = 0; i < 16; ++i)
                {
                    constexpr f32 pi = glm::pi<f32>();
                    f32 rng = ((f32)rand() / RAND_MAX);
                    octaveRandomRotationAngles[i] = rng * pi * 2.f;
                }

                bool found;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "GenerateHeightField", APP_SHADER_PATH "generate_heightfield_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);
                
                const auto& desc = m_gpuBaseLayerHeightMap->getDesc();
                RenderingUtils::renderScreenPass(
                    glm::uvec2(desc.width, desc.height),
                    [this](RenderPass& rp) {
                        rp.setRenderTarget(m_gpuBaseLayerHeightMap.get(), 0);
                        rp.setRenderTarget(m_gpuBaseLayerNormalMap.get(), 1);
                        rp.setRenderTarget(m_gpuCompositedHeightMap.get(), 4);
                        rp.setRenderTarget(m_gpuCompositedNormalMap.get(), 5);
                        rp.setRenderTarget(m_gpuCloudOpacityMap.get(), 6);
                    },
                    gfxp.get(),
                    [this](GfxPipeline* p) {
                        p->setUniform("amplitude", amplitude);
                        p->setUniform("frequency", frequency);
                        p->setUniform("numOctaves", numOctaves);
                        p->setUniform("persistence", persistence);

                        p->setUniform("u_snowBlendMin", snowBlendMin);
                        p->setUniform("u_snowBlendMax", snowBlendMax);
                        p->setUniform("snowLayerAmplitude", snowLayerAmplitude);
                        p->setUniform("snowLayerFrequency", snowLayerFrequency);
                        p->setUniform("snowLayerNumOctaves", snowLayerNumOctaves);
                        p->setUniform("snowLayerPersistence", snowLayerPersistance);
                        p->setUniform("snowHeightBias", snowHeightBias);

                        p->setUniform("u_cloudAmplitude", cloudAmplitude);
                        p->setUniform("u_cloudFrequency", cloudFrequency);
                        p->setUniform("u_cloudNumOctaves", cloudNumOctaves);
                        p->setUniform("u_cloudPersistence", cloudPersistence);

                        for (i32 i = 0; i < numOctaves; ++i)
                        {
                            std::string uniformName = "octaveRotationAngles";
                            uniformName += "[" + std::to_string(i) + "]";
                            p->setUniform(uniformName.c_str(), octaveRandomRotationAngles[i]);
                        }
                    }
                );
            });

        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "BuildHeightFieldQuadTree", 
            [this](Frame& frame) {
                GPU_DEBUG_SCOPE(marker, "Build HeightField QuadTree");

                // copy the height map to mip 0
                RenderingUtils::copyTexture(m_heightFieldQuadTree.get(), 0, m_gpuCompositedHeightMap.get(), 0);

                bool found;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "BuildHeightFieldQuadTree", APP_SHADER_PATH "build_heightfield_quadtree_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                const auto& desc = m_gpuCompositedHeightMap->getDesc();
                i32 numPasses = glm::log2(desc.width);
                for (i32 i = 0; i < numPasses; ++i)
                {
                    u32 srcMip = i;
                    u32 dstMip = srcMip + 1;
                    glm::ivec2 mipSize;
                    m_heightFieldQuadTree->getMipSize(mipSize.x, mipSize.y, dstMip);
                    RenderingUtils::renderScreenPass(
                        glm::uvec2(mipSize),
                        [i, this, dstMip](RenderPass& rp) {
                            RenderTarget rt(m_heightFieldQuadTree.get(), dstMip);
                            rp.setRenderTarget(rt, 0);
                        },
                        gfxp.get(),
                        [this, i](GfxPipeline* p) {
                            i32 srcMip = i;
                            p->setTexture("u_heightFieldQuadTreeTexture", m_heightFieldQuadTree.get());
                            p->setUniform("u_srcMipLevel", srcMip);
                        }
                    );
                }
            });
#endif
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "RayMarchingHeightField", [this](Frame& frame) {
                auto rayMarchingTask = [this](SceneView& view) {
                    GPU_DEBUG_SCOPE(marker, "RayMarching Height Field");

                    auto render = view.getRender();
                    auto sceneDepth = render->depth();

                    const auto& viewState = view.getState();
                    const auto& cameraInfo = view.getCameraInfo();

                    glm::vec3 cameraPosition = viewState.cameraPosition;
                    auto desc = m_rayMarchingOutTexture->getDesc();

                    bool found;
                    auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                    auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "RayMarchingHeightField", APP_SHADER_PATH "raymarching_heightfield_p.glsl");
                    auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                    Transform heightFieldTransform;
                    heightFieldTransform.translation = position;
                    heightFieldTransform.scale = scale;
                    glm::mat4 transformMatrix = heightFieldTransform.toMatrix();

                    RenderingUtils::renderScreenPass(
                        glm::uvec2(desc.width, desc.height),
                        [this, render](RenderPass& rp) {
                            rp.setRenderTarget(m_rayMarchingOutTexture.get(), 0);
                            rp.setDepthTarget(render->depth());
                        },
                        gfxp.get(),
                        [this, cameraInfo, viewState, desc, transformMatrix](GfxPipeline* p) {
                            p->setUniform("renderResolution", glm::ivec2(desc.width, desc.height));
                            p->setUniform("maxNumRayMarchingSteps", maxNumRayMarchingSteps);
                            p->setUniform("heightFieldTransformMatrix", transformMatrix);

                            p->setUniform("u_groundAlbedo", groundAlbedo);
                            p->setUniform("u_snowAlbedo", glm::vec3(1.f));
                            p->setUniform("u_snowBlendMin", snowBlendMin);
                            p->setUniform("u_snowBlendMax", snowBlendMax);

                            p->setUniform("viewMatrix", cameraInfo.viewMatrix());
                            p->setUniform("projectionMatrix", cameraInfo.projectionMatrix());
                            p->setUniform("cameraPosition", viewState.cameraPosition);
                            p->setUniform("cameraRight", viewState.cameraRight);
                            p->setUniform("cameraForward", viewState.cameraForward);
                            p->setUniform("cameraUp", viewState.cameraUp);
                            p->setUniform("n", cameraInfo.m_perspective.n);
                            p->setUniform("f", cameraInfo.m_perspective.f);
                            p->setUniform("fov", cameraInfo.m_perspective.fov);
                            p->setUniform("aspectRatio", (f32)desc.width / desc.height);
#if GPU_NOISE
                            p->setTexture("u_compositedHeightMap", m_gpuCompositedHeightMap.get());
                            p->setTexture("u_compositedNormalMap", m_gpuCompositedNormalMap.get());
                            p->setTexture("u_baseLayerHeightMap", m_gpuBaseLayerHeightMap.get());
                            p->setTexture("u_cloudOpacityMap", m_gpuCloudOpacityMap.get());
                            p->setTexture("u_heightFieldQuadTree", m_heightFieldQuadTree.get());
                            p->setUniform("u_numMipLevels", (i32)m_heightFieldQuadTree->getDesc().numMips);
#else
                            p->setTexture("heightMap", m_cpuHeightMap.get());
                            p->setTexture("normalMap", m_cpuNormalMap.get());
#endif
                        },
                        true
                    );
                };

                auto views = *(frame.views);
                auto currentActiveView = views[0];
                rayMarchingTask(*currentActiveView);

                auto debugHierarchicalMarchingTask = [this](SceneView& view) {
                    {
                        GPU_DEBUG_SCOPE(marker, "Record Hierarchical RayMarching Traces");

                        struct Trace
                        {
                            int stepID;
                            glm::vec3 padding;
                            float t;
                            glm::vec3 textureSpacePos;
                            int mipLevel;
                            float tQuadTreeCell;
                            float sceneHeight;
                            float tHeight;
                        };

                        // todo: implement shader storage buffer
                        struct TraceBuffer
                        {
                            TraceBuffer(u32 inNumElements) 
                                : numElements(inNumElements)
                            {
                                sizeInBytes = numElements * sizeof(Trace);
                                glCreateBuffers(1, &buffer);
                                glNamedBufferData(buffer, sizeInBytes, nullptr, GL_DYNAMIC_COPY);
                            }

                            void bind()
                            {
                                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);
                            }

                            void unbind()
                            {
                                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
                            }

                            void read(std::vector<Trace>& outTraces)
                            {
                                if (outTraces.size() < numElements)
                                {
                                    outTraces.resize(numElements);
                                }
                                glGetNamedBufferSubData(buffer, 0, sizeInBytes, outTraces.data());
                            }

                            GLuint buffer;
                            u32 sizeInBytes;
                            u32 numElements;
                        };

                        static TraceBuffer traceBuffer(64);

                        bool found = false;
                        auto cs = ShaderManager::findOrCreateShader<ComputeShader>(found, "DebugHierarchicalHeightFieldTracingCS", APP_SHADER_PATH "debug_hierarchical_raymarching_c.glsl");
                        auto cp = ShaderManager::findOrCreateComputePipeline(found, cs);

                        cp->bind();
                        Transform heightFieldTransform;
                        heightFieldTransform.translation = position;
                        heightFieldTransform.scale = scale;
                        glm::mat4 transformMatrix = heightFieldTransform.toMatrix();
                        cp->setUniform("u_heightFieldTransfromMat", transformMatrix);
                        cp->setTexture("u_heightFieldQuadTree", m_heightFieldQuadTree.get());
                        cp->setUniform("u_numMipLevels", (i32)m_heightFieldQuadTree->getDesc().numMips);
                        cp->setUniform("u_debugRo", debugRo);
                        cp->setUniform("u_debugRd", debugRd);
                        cp->unbind();
                    }

                    {
                        GPU_DEBUG_SCOPE(marker, "Visualize Hierarchical RayMarching Traces");

                        auto render = view.getRender();
                        const auto cameraInfo = view.getCameraInfo();
                        RenderingUtils::drawWorldSpaceLine(
                            m_rayMarchingOutTexture.get(),
                            render->depth(),
                            cameraInfo.viewMatrix(),
                            cameraInfo.projectionMatrix(),
                            debugRo,
                            debugRo + debugRd * 10.f,
                            glm::vec4(Color::navy, 1.f)
                        );
                    }
                };
                debugHierarchicalMarchingTask(*currentActiveView);
            });

        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPostRenderToBackBuffer,
            "RenderRayMarchingOutputToBackBuffer", 
            [this](Frame& frame) {
                glm::uvec2 windowSize = GfxModule::get()->getWindowSize();
                RenderingUtils::renderToBackBuffer(m_rayMarchingOutTexture.get());
                const i32 resolution = 256;
#if GPU_NOISE
                RenderingUtils::renderToBackBuffer(m_gpuBaseLayerHeightMap.get(), Viewport { 0, (i32)windowSize.y - resolution * 1, resolution, resolution });
                RenderingUtils::renderToBackBuffer(m_gpuBaseLayerNormalMap.get(), Viewport { 0, (i32)windowSize.y - resolution * 2, resolution, resolution });
                RenderingUtils::renderToBackBuffer(m_gpuCompositedHeightMap.get(), Viewport { 0, (i32)windowSize.y - resolution * 3, resolution, resolution });
                RenderingUtils::renderToBackBuffer(m_gpuCloudOpacityMap.get(), Viewport { 0, (i32)windowSize.y - resolution * 4, resolution, resolution });
#else
                RenderingUtils::renderToBackBuffer(m_cpuHeightMap.get(), Viewport { 0, (i32)windowSize.y - resolution, resolution, resolution });
                RenderingUtils::renderToBackBuffer(m_cpuNormalMap.get(), Viewport { 0, (i32)windowSize.y - resolution * 2, resolution, resolution });
#endif
            });

#define IMGUI_SLIDER_FLOAT(label, ptr, vmin, vmax)       \
    ImGui::Text(label); ImGui::SameLine();              \
    ImGui::SliderFloat("##" label, ptr, vmin, vmax);    \

#define IMGUI_SLIDER_INT(label, ptr, vmin, vmax)       \
    ImGui::Text(label); ImGui::SameLine();              \
    ImGui::SliderInt("##" label, ptr, vmin, vmax);    \

        Engine::get()->enqueueUICommand([this](ImGuiContext* imguiCtx) {
            /**
             * This SetCurrentContext() call is necessary here since I'm using DLL, and different DLLs has different
             * ImGuiContext.
             */
            ImGui::SetCurrentContext(imguiCtx);
            ImGui::Begin("RayMarchingHeightField");
            {
                ImGui::Text("Max Num RayMarching Steps:"); ImGui::SameLine();
                ImGui::SliderInt("##MaxNumRayMarchingSteps", &maxNumRayMarchingSteps, 16, 128);

                float p[3];
                p[0] = position.x; p[1] = position.y; p[2] = position.z;
                ImGui::Text("Position"); ImGui::SameLine();
                ImGui::SliderFloat3("##Position", p, -10.f, 10.f);
                position.x = p[0]; position.y = p[1]; position.z = p[2];

                float s[3];
                s[0] = scale.x; s[1] = scale.y; s[2] = scale.z;
                ImGui::Text("Scale"); ImGui::SameLine();
                ImGui::SliderFloat3("##Scale", s, 0.f, 10.f);
                scale.x = s[0]; scale.y = s[1]; scale.z = s[2];

                float c[3];
                c[0] = groundAlbedo.r; c[1] = groundAlbedo.g; c[2] = groundAlbedo.b;
                ImGui::ColorPicker3("Albedo", c);
                groundAlbedo.r = c[0]; groundAlbedo.g = c[1]; groundAlbedo.b = c[2];

                ImGui::Text("Base Layer:");
                IMGUI_SLIDER_INT("Rotation Random Seed", &rotationRandomSeed, 1, 16)
                IMGUI_SLIDER_FLOAT("Amplitude", &amplitude, 0.f, 16.f)
                IMGUI_SLIDER_FLOAT("Frequency", &frequency, 0.f, 16.f)
                IMGUI_SLIDER_INT("Num Octaves", &numOctaves, 1, 16)
                IMGUI_SLIDER_FLOAT("Persistance", &persistence, 0.f, 1.f)

                ImGui::Text("Snow Layer:");
                IMGUI_SLIDER_FLOAT("Snow Blend Min", &snowBlendMin, 0.f, 1.f)
                IMGUI_SLIDER_FLOAT("Snow Blend Max", &snowBlendMax, 0.f, 1.f)
                IMGUI_SLIDER_FLOAT("Snow Amplitude", &snowLayerAmplitude, 0.f, 16.f)
                IMGUI_SLIDER_FLOAT("Snow Frequency", &snowLayerFrequency, 0.f, 16.f)
                IMGUI_SLIDER_INT("Snow Num Octaves", &snowLayerNumOctaves, 1, 16)
                IMGUI_SLIDER_FLOAT("Snow Persistance", &snowLayerPersistance, 0.f, 1.f)
                IMGUI_SLIDER_FLOAT("Snow Height Bias", &snowHeightBias, 0.f, 1.f)

                ImGui::Text("Cloud Layers:");
                IMGUI_SLIDER_FLOAT("Cloud Amplitude", &cloudAmplitude, 0.f, 1.f)
                IMGUI_SLIDER_FLOAT("Cloud Frequency", &cloudFrequency, 0.f, 16.f)
                IMGUI_SLIDER_INT("Cloud Num Octaves", &cloudNumOctaves, 1, 16)
                IMGUI_SLIDER_FLOAT("Cloud Persistence", &cloudPersistence, 0.f, 1.f)
            }
            ImGui::End();
        });
    }

    // todo:
    static void buildPerlinNoiseHeightField()
    {
        const i32 heightFieldSize = 256;
    }

    void RayMarchingHeightFieldApp::customInitialize(World* world)
    {
        const char* sceneFilePath = "C:/dev/cyanRenderEngine/EngineApp/Apps/RayMarchingHeightField/Resources/Scene.glb";

        // add a camera
        Transform cameraTransform = { };
        cameraTransform.translation = glm::vec3(0.f, 7.f, 5.f); 
        SceneCameraEntity* ce = world->createEntity<SceneCameraEntity>("TestCamera_0", cameraTransform).get();
        auto cc = ce->getSceneCameraComponent();
        cc->setResolution(glm::uvec2(1920, 1080));
        cc->setRenderMode(SceneCamera::RenderMode::kSceneAlbedo);
        auto cameraControllerComponent = std::make_shared<CameraControllerComponent>("CameraControllerComponent", cc);
        ce->addComponent(cameraControllerComponent);

        // add a directional light
        DirectionalLightEntity* dle = world->createEntity<DirectionalLightEntity>("SunLight", Transform()).get();
        // generate a 2D height map using Perlin noise 
        // todo: what parameters can be used to control the shape of the perlin noise
        const i32 heightMapSize = 1024;

#if !GPU_NOISE
        auto pixelCoordToNoiseSpaceCoord = [](i32 x, i32 y, i32 heightMapSize) -> glm::dvec2 {
            glm::dvec2 p(x, y);
            p = (p + .5) / (f64)heightMapSize;
            p.x = p.x * 2.f - 1.f;
            p.y = p.y * 2.f - 1.f;
            return p;
        };

        /**
         * This buffer stores value starting from the bottom left corner of the height map
         */
        // todo: random rotation for each octave
        f64 tiling = 256.f;
        f64 frequency = 1.f;
        f64 amplitude = 1024.f;
        i32 numOctaves = 8;
        f64 persistence = .5;
        std::vector<f32> heights(heightMapSize * heightMapSize);
        for (i32 y = 0; y < heightMapSize; ++y)
        {
            for (i32 x = 0; x < heightMapSize; ++x)
            {
                glm::dvec3 p(pixelCoordToNoiseSpaceCoord(x, y, heightMapSize), 0);

                i32 outX = x;
                i32 outY = y;
                i32 outIndex = heightMapSize * outY + x;

                heights[outIndex] = PerlinNoise(p, 256, frequency, amplitude, numOctaves, persistence);
            }
        }
        // bake normals
        std::vector<glm::vec3> normals(heightMapSize * heightMapSize);
        for (i32 y = 0; y < heightMapSize; ++y)
        {
            for (i32 x = 0; x < heightMapSize; ++x)
            {
                glm::dvec3 p(pixelCoordToNoiseSpaceCoord(x, y, heightMapSize), 0);
                p.z = heights[y * heightMapSize + x];

                i32 windingOrder = 0; // 0 for cross(ppx, ppy), 1 for cross(ppy, ppx)

                glm::dvec3 px, py;
                if (x + 1 < heightMapSize)
                {
                    px = glm::dvec3(pixelCoordToNoiseSpaceCoord(x + 1, y, heightMapSize), 0);
                    px.z = heights[y * heightMapSize + (x + 1)];
                }
                else
                {
                    px = glm::dvec3(pixelCoordToNoiseSpaceCoord(x - 1, y, heightMapSize), 0);
                    px.z = heights[y * heightMapSize + (x - 1)];
                    // toggle the winding order
                    windingOrder = 1 - windingOrder;
                }

                if (y + 1 < heightMapSize)
                {
                    py = glm::dvec3(pixelCoordToNoiseSpaceCoord(x, y + 1, heightMapSize), 0);
                    py.z = heights[(y + 1) * heightMapSize + x];
                }
                else
                {
                    py = glm::dvec3(pixelCoordToNoiseSpaceCoord(x, y - 1, heightMapSize), 0);
                    py.z = heights[(y - 1) * heightMapSize + x];
                    // toggle the winding order
                    windingOrder = 1 - windingOrder;
                }

                glm::dvec3 ppx = normalize(px - p);
                glm::dvec3 ppy = normalize(py - p);
                glm::dvec3 normal;
                if (windingOrder == 0)
                {
                    normal = glm::normalize(glm::cross(ppx, ppy));
                }
                else if (windingOrder == 1)
                { 
                    normal = glm::normalize(glm::cross(ppy, ppx));
                }

                normals[y * heightMapSize + x] = glm::vec3(normal.x, normal.z, -normal.y) * .5f + .5f;
            }
        }
#endif

        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "CreateTextures", 
#if !GPU_NOISE
            [this, heights, normals](Frame& frame) mutable {
#else
            [this](Frame& frame) mutable {
#endif
                // height map
                {
#if GPU_NOISE
                    auto desc = GHTexture2D::Desc::create(heightMapSize, heightMapSize, 1, PF_R32F);
#else
                    auto desc = GHTexture2D::Desc::create(heightMapSize, heightMapSize, 1, PF_R32F, heights.data());
#endif
                    GHSampler2D sampler2D;
                    sampler2D.setFilteringModeMin(SAMPLER2D_FM_BILINEAR);
                    sampler2D.setFilteringModeMag(SAMPLER2D_FM_BILINEAR);
                    sampler2D.setAddressingModeX(SAMPLER2D_AM_CLAMP);
                    sampler2D.setAddressingModeY(SAMPLER2D_AM_CLAMP);
#if GPU_NOISE
                    m_gpuBaseLayerHeightMap = std::move(GHTexture2D::create(desc, sampler2D));
                    m_gpuCompositedHeightMap = std::move(GHTexture2D::create(desc, sampler2D));

                    {
                        GHSampler2D sampler2D;

                        sampler2D.setFilteringModeMin(SAMPLER2D_FM_POINT);
                        sampler2D.setFilteringModeMag(SAMPLER2D_FM_POINT);
                        sampler2D.setAddressingModeX(SAMPLER2D_AM_CLAMP);
                        sampler2D.setAddressingModeY(SAMPLER2D_AM_CLAMP);
                        // make sure the dimension of src height map is power of 2
                        assert(isPowerOf2(desc.width) && isPowerOf2(desc.height) && desc.width == desc.height);
                        i32 numMips = glm::log2(desc.width) + 1;
                        desc.numMips = numMips;
                        m_heightFieldQuadTree = std::move(GHTexture2D::create(desc, sampler2D));
                    }
#else
                    m_cpuHeightMap = std::move(GHTexture2D::create(desc, sampler2D));
#endif
                }
                // normal map
                {
#if GPU_NOISE
                    auto desc = GHTexture2D::Desc::create(heightMapSize, heightMapSize, 1, PF_RGB32F);
#else
                    auto desc = GHTexture2D::Desc::create(heightMapSize, heightMapSize, 1, PF_RGB32F, normals.data());
#endif
                    GHSampler2D sampler2D;
                    sampler2D.setFilteringModeMin(SAMPLER2D_FM_BILINEAR);
                    sampler2D.setFilteringModeMag(SAMPLER2D_FM_BILINEAR);
                    sampler2D.setAddressingModeX(SAMPLER2D_AM_CLAMP);
                    sampler2D.setAddressingModeY(SAMPLER2D_AM_CLAMP);
#if GPU_NOISE
                    m_gpuBaseLayerNormalMap = std::move(GHTexture2D::create(desc, sampler2D));
                    m_gpuCompositedNormalMap = std::move(GHTexture2D::create(desc, sampler2D));
#else
                    m_cpuNormalMap = std::move(GHTexture2D::create(desc, sampler2D));
#endif
                }
                // albedo map
                {
#if GPU_NOISE
                    auto desc = GHTexture2D::Desc::create(heightMapSize, heightMapSize, 1, PF_R32F);
                    GHSampler2D sampler2D;
                    sampler2D.setFilteringModeMin(SAMPLER2D_FM_BILINEAR);
                    sampler2D.setFilteringModeMag(SAMPLER2D_FM_BILINEAR);
                    sampler2D.setAddressingModeX(SAMPLER2D_AM_WRAP);
                    sampler2D.setAddressingModeY(SAMPLER2D_AM_WRAP);
                    m_gpuCloudOpacityMap = std::move(GHTexture2D::create(desc, sampler2D));
#endif
                }
                {
                    glm::uvec2 windowSize = GfxModule::get()->getWindowSize();
                    auto desc = GHTexture2D::Desc::create(windowSize.x, windowSize.y, 1, PF_RGB16F);
                    GHSampler2D sampler2D;
                    sampler2D.setAddressingModeX(SAMPLER2D_AM_CLAMP);
                    sampler2D.setAddressingModeY(SAMPLER2D_AM_CLAMP);
                    m_rayMarchingOutTexture = std::move(GHTexture2D::create(desc, sampler2D));
                }
        });
    }
}
