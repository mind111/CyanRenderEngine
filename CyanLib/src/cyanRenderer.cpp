#include <iostream>
#include <fstream>
#include <queue>
#include <functional>

#include "glfw3.h"
#include "gtx/quaternion.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "json.hpp"
#include "glm.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "ImGuizmo/ImGuizmo.h"

#include "Common.h"
#include "AssetManager.h"
#include "CyanRenderer.h"
#include "Material.h"
#include "MathUtils.h"
#include "CyanUI.h"
#include "Ray.h"
#include "RenderableScene.h"

#define DRAW_SSAO_DEBUG_VIS 0

namespace Cyan
{
    Renderer* Singleton<Renderer>::singleton = nullptr;
    static Mesh* fullscreenQuad = nullptr;

    Renderer::Renderer(GfxContext* ctx, u32 windowWidth, u32 windowHeight)
        : Singleton<Renderer>(), 
        m_ctx(ctx),
        m_windowWidth(windowWidth),
        m_windowHeight(windowHeight),
        m_SSAAWidth(2u * m_windowWidth),
        m_SSAAHeight(2u * m_windowHeight),
        m_sceneColorTexture(nullptr),
        m_sceneColorRenderTarget(nullptr),
        m_sceneColorTextureSSAA(nullptr),
        m_sceneColorRTSSAA(nullptr),
        m_bloomOutTexture(nullptr),
        m_frameAllocator(1024 * 1024 * 32) // 32MB frame allocator 
    {
        m_rasterDirectShadowManager = std::make_unique<RasterDirectShadowManager>();
    }

    Texture2DRenderable* Renderer::getColorOutTexture()
    {
        return m_finalColorTexture;
    }

    void Renderer::Vctx::Voxelizer::init(u32 resolution)
    {
        ITextureRenderable::Spec voxelizeSpec = { };
        voxelizeSpec.width = resolution;
        voxelizeSpec.height = resolution;
        voxelizeSpec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R16G16B16;

        ITextureRenderable::Spec voxelizeSuperSampledSpec = { };
        voxelizeSuperSampledSpec.width = resolution * ssaaRes;
        voxelizeSuperSampledSpec.height = resolution * ssaaRes;
        voxelizeSuperSampledSpec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R16G16B16;
        Texture2DRenderable* voxelizeSSColorbuffer = AssetManager::createTexture2D("VoxelizeSS", voxelizeSuperSampledSpec);

        ssRenderTarget = createRenderTarget(resolution * ssaaRes, resolution * ssaaRes);
        ssRenderTarget->setColorBuffer(voxelizeSSColorbuffer, 0);

        colorBuffer = AssetManager::createTexture2D("VoxelizeDebug", voxelizeSpec);
        renderTarget = createRenderTarget(resolution, resolution);
        renderTarget->setColorBuffer(colorBuffer, 0);
        voxelizeShader = ShaderManager::createShader({ ShaderType::kVsGsPs, "VoxelizeShader", SHADER_SOURCE_PATH "voxelize_v.glsl", SHADER_SOURCE_PATH "voxelize_p.glsl", SHADER_SOURCE_PATH "voxelize_g.glsl" });
        filterVoxelGridShader = ShaderManager::createShader({ ShaderType::kCs, "VctxFilterShader", nullptr, nullptr, nullptr, SHADER_SOURCE_PATH "vctx_filter_c.glsl" });

        // opacity mask buffer
        glCreateBuffers(1, &opacityMaskSsbo);
        i32 buffSize = sizeof(i32) * pow(resolution *ssaaRes, 3);
        glNamedBufferData(opacityMaskSsbo, buffSize, nullptr, GL_DYNAMIC_COPY);
    }

    void Renderer::Vctx::Visualizer::init()
    {
        ITextureRenderable::Spec visSpec = { };
        visSpec.width = 1280;
        visSpec.height = 720;
        visSpec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R16G16B16;

        colorBuffer = AssetManager::createTexture2D("VoxelVis", visSpec);
        renderTarget = createRenderTarget(visSpec.width, visSpec.height);
        renderTarget->setColorBuffer(colorBuffer, 0u);
        voxelGridVisShader = ShaderManager::createShader({ ShaderType::kVsGsPs, "VoxelVisShader", SHADER_SOURCE_PATH "voxel_vis_v.glsl", SHADER_SOURCE_PATH "voxel_vis_p.glsl", SHADER_SOURCE_PATH "voxel_vis_g.glsl" });
        coneVisComputeShader = ShaderManager::createShader({ ShaderType::kCs, "ConeVisComputeShader", nullptr, nullptr, nullptr, SHADER_SOURCE_PATH "cone_vis_c.glsl" });
        coneVisDrawShader = ShaderManager::createShader({ ShaderType::kVsGsPs, "ConeVisGraphicsShader", SHADER_SOURCE_PATH "cone_vis_v.glsl", SHADER_SOURCE_PATH "cone_vis_p.glsl", SHADER_SOURCE_PATH "cone_vis_g.glsl" });

        ITextureRenderable::Spec depthNormSpec = { };
        depthNormSpec.width = 1280 * 2;
        depthNormSpec.height = 720 * 2;
        depthNormSpec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R32G32B32;
        cachedSceneDepth = AssetManager::createTexture2D("CachedSceneDepth", depthNormSpec);
        cachedSceneNormal = AssetManager::createTexture2D("CachedSceneNormal", depthNormSpec);

        struct IndirectDrawArgs
        {
            GLuint first;
            GLuint count;
        };

        // debug ssbo
        glCreateBuffers(1, &ssbo);
        glNamedBufferData(ssbo, sizeof(DebugBuffer), &debugConeBuffer, GL_DYNAMIC_COPY);

        glCreateBuffers(1, &idbo);
        glNamedBufferData(idbo, sizeof(IndirectDrawArgs), nullptr, GL_DYNAMIC_COPY);
    }

    void Renderer::initialize()
    {
        // shared global quad mesh
        {
            float quadVerts[24] = {
                -1.f, -1.f, 0.f, 0.f,
                 1.f,  1.f, 1.f, 1.f,
                -1.f,  1.f, 0.f, 1.f,

                -1.f, -1.f, 0.f, 0.f,
                 1.f, -1.f, 1.f, 0.f,
                 1.f,  1.f, 1.f, 1.f
            };

            auto assetManager = AssetManager::get();
            std::vector<ISubmesh*> submeshes;
            std::vector<Triangles::Vertex> vertices(6);
            vertices[0].pos = glm::vec3(-1.f, -1.f, 0.f); vertices[0].texCoord0 = glm::vec2(0.f, 0.f);
            vertices[1].pos = glm::vec3( 1.f,  1.f, 0.f); vertices[1].texCoord0 = glm::vec2(1.f, 1.f);
            vertices[2].pos = glm::vec3(-1.f,  1.f, 0.f); vertices[2].texCoord0 = glm::vec2(0.f, 1.f);
            vertices[3].pos = glm::vec3(-1.f, -1.f, 0.f); vertices[3].texCoord0 = glm::vec2(0.f, 0.f);
            vertices[4].pos = glm::vec3( 1.f, -1.f, 0.f); vertices[4].texCoord0 = glm::vec2(1.f, 0.f);
            vertices[5].pos = glm::vec3( 1.f,  1.f, 0.f); vertices[5].texCoord0 = glm::vec2(1.f, 1.f);
            std::vector<u32> indices({ 0, 1, 2, 3, 4, 5 });
            submeshes.push_back(assetManager->createSubmesh<Triangles>(vertices, indices));
            fullscreenQuad = assetManager->createMesh("FullScreenQuadMesh", submeshes);
        }

        // shadow
        m_rasterDirectShadowManager->initialize();
        m_rasterDirectShadowManager->initShadowmap(m_csm, glm::uvec2(4096u, 4096u));

        // set back-face culling
        m_ctx->setCullFace(FrontFace::CounterClockWise, FaceCull::Back);

        // scene render targets
        {
            enum class ColorBuffers
            {
                kColor = 0,
                kDepth,
                kNormal
            };

            // 4x super sampled buffers 
            ITextureRenderable::Spec sceneColorTextureSpec = { };
            sceneColorTextureSpec.width = m_SSAAWidth;
            sceneColorTextureSpec.height = m_SSAAHeight;
            sceneColorTextureSpec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R32G32B32;
            m_sceneColorTextureSSAA = AssetManager::createTexture2D("SceneColorTexSSAA", sceneColorTextureSpec);
            m_sceneColorRTSSAA = createRenderTarget(m_SSAAWidth, m_SSAAHeight);
            m_sceneColorRTSSAA->setColorBuffer(m_sceneColorTextureSSAA, static_cast<u32>(ColorBuffers::kColor));

            // non-aa buffers
            sceneColorTextureSpec.width = m_windowWidth;
            sceneColorTextureSpec.height = m_windowHeight;
            m_sceneColorTexture = AssetManager::createTexture2D("SceneColorTexture", sceneColorTextureSpec);
            m_sceneColorRenderTarget = createRenderTarget(m_windowWidth, m_windowHeight);
            m_sceneColorRenderTarget->setColorBuffer(m_sceneColorTexture, static_cast<u32>(ColorBuffers::kColor));

            // todo: it seems using rgb16f leads to precision issue when building ssao
            ITextureRenderable::Spec sceneDepthNormalTextureSpec = { };
            sceneDepthNormalTextureSpec.width = m_SSAAWidth;
            sceneDepthNormalTextureSpec.height = m_SSAAHeight;
            sceneDepthNormalTextureSpec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R32G32B32;

            m_sceneDepthTextureSSAA = AssetManager::createTexture2D("SceneDepthTexSSAA", sceneDepthNormalTextureSpec);
            m_sceneNormalTextureSSAA = AssetManager::createTexture2D("SceneNormalTextureSSAA", sceneDepthNormalTextureSpec);
            m_sceneColorRTSSAA->setColorBuffer(m_sceneDepthTextureSSAA, static_cast<u32>(ColorBuffers::kDepth));
            m_sceneColorRTSSAA->setColorBuffer(m_sceneNormalTextureSSAA, static_cast<u32>(ColorBuffers::kNormal));

            sceneDepthNormalTextureSpec.width = m_windowWidth;
            sceneDepthNormalTextureSpec.height = m_windowHeight;
            m_sceneNormalTexture = AssetManager::createTexture2D("SceneNormalTexture", sceneDepthNormalTextureSpec);
            m_sceneDepthTexture = AssetManager::createTexture2D("SceneDepthTexture", sceneDepthNormalTextureSpec);
            m_sceneColorRenderTarget->setColorBuffer(m_sceneDepthTexture, static_cast<u32>(ColorBuffers::kDepth));
            m_sceneColorRenderTarget->setColorBuffer(m_sceneNormalTexture, static_cast<u32>(ColorBuffers::kNormal));
            m_sceneDepthNormalShader = ShaderManager::createShader({ ShaderType::kVsPs, "SceneDepthNormalShader", SHADER_SOURCE_PATH "scene_depth_normal_v.glsl", SHADER_SOURCE_PATH "scene_depth_normal_p.glsl" });
        }

        // ssao
        {
            ITextureRenderable::Spec spec = { };
            spec.width = m_windowWidth;
            spec.height = m_windowHeight;
            spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R16G16B16;

            m_ssaoRenderTarget = createRenderTarget(spec.width, spec.height);
            m_ssaoTexture = AssetManager::createTexture2D("SSAOTexture", spec);
            m_ssaoRenderTarget->setColorBuffer(m_ssaoTexture, 0u);
            m_ssaoShader = ShaderManager::createShader({ ShaderType::kVsPs, "SSAOShader", SHADER_SOURCE_PATH "shader_ao.vs", SHADER_SOURCE_PATH "shader_ao.fs" });
        }

        // bloom
        {
            m_bloomSetupRenderTarget = createRenderTarget(m_windowWidth, m_windowHeight);
            ITextureRenderable::Spec spec = { };
            spec.width = m_windowWidth;
            spec.height = m_windowHeight;
            spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R16G16B16A16;
            ITextureRenderable::Parameter parameter = { };

            m_bloomSetupRenderTarget->setColorBuffer(AssetManager::createTexture2D("BloomSetupTexture", spec), 0);
            m_bloomSetupShader = ShaderManager::createShader({ ShaderType::kVsPs, "BloomSetupShader", SHADER_SOURCE_PATH "shader_bloom_preprocess.vs", SHADER_SOURCE_PATH "shader_bloom_preprocess.fs" });
            m_bloomDsShader = ShaderManager::createShader({ ShaderType::kVsPs, "BloomDownSampleShader", SHADER_SOURCE_PATH "shader_downsample.vs", SHADER_SOURCE_PATH "shader_downsample.fs" });
            m_bloomUsShader = ShaderManager::createShader({ ShaderType::kVsPs, "UpSampleShader", SHADER_SOURCE_PATH "shader_upsample.vs", SHADER_SOURCE_PATH "shader_upsample.fs" });
            m_gaussianBlurShader = ShaderManager::createShader({ ShaderType::kVsPs, "GaussianBlurShader", SHADER_SOURCE_PATH "shader_gaussian_blur.vs", SHADER_SOURCE_PATH "shader_gaussian_blur.fs" });

            u32 numBloomTextures = 0u;
            auto initBloomBuffers = [&](u32 index, ITextureRenderable::Spec& spec, const ITextureRenderable::Parameter& parameter) {
                m_bloomDsTargets[index].renderTarget = createRenderTarget(spec.width, spec.height);
                char buff[64];
                sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
                m_bloomDsTargets[index].src = AssetManager::createTexture2D(buff, spec, parameter);
                sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
                m_bloomDsTargets[index].scratch = AssetManager::createTexture2D(buff, spec, parameter);

                m_bloomUsTargets[index].renderTarget = createRenderTarget(spec.width, spec.height);
                sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
                m_bloomUsTargets[index].src = AssetManager::createTexture2D(buff, spec, parameter);
                sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
                m_bloomUsTargets[index].scratch = AssetManager::createTexture2D(buff, spec, parameter);
                spec.width /= 2;
                spec.height /= 2;
            };

            for (u32 i = 0u; i < kNumBloomPasses; ++i) {
                initBloomBuffers(i, spec, parameter);
            }
        }

        // composite
        {
            ITextureRenderable::Spec spec = { };
            spec.width = m_windowWidth;
            spec.height = m_windowHeight;
            spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R16G16B16A16;
            m_compositeColorTexture = AssetManager::createTexture2D("CompositeColorTexture", spec);
            m_compositeRenderTarget = createRenderTarget(m_compositeColorTexture->width, m_compositeColorTexture->height);
            m_compositeRenderTarget->setColorBuffer(m_compositeColorTexture, 0u);
            m_compositeShader = ShaderManager::createShader({ ShaderType::kVsPs, "CompositeShader", SHADER_SOURCE_PATH "composite_v.glsl", SHADER_SOURCE_PATH "composite_p.glsl" });
        }

        // voxel cone tracing
        {
            m_vctx.voxelizer.init(m_sceneVoxelGrid.resolution);
            m_vctx.visualizer.init();

            {
                ITextureRenderable::Spec spec = { };
                spec.width = m_sceneVoxelGrid.resolution;
                spec.height = m_sceneVoxelGrid.resolution;
                spec.depth = m_sceneVoxelGrid.resolution;
                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R8G8B8A8;
                ITextureRenderable::Parameter parameter = { };
                parameter.minificationFilter = ITextureRenderable::Parameter::Filtering::MIPMAP_LINEAR;
                parameter.magnificationFilter = ITextureRenderable::Parameter::Filtering::NEAREST;
                m_sceneVoxelGrid.normal = AssetManager::createTexture3D("VoxelGridNormal", spec, parameter);
                m_sceneVoxelGrid.emission = AssetManager::createTexture3D("VoxelGridEmission", spec, parameter);

                spec.numMips = std::log2(m_sceneVoxelGrid.resolution) + 1;
                m_sceneVoxelGrid.albedo = AssetManager::createTexture3D("VoxelGridAlbedo", spec, parameter);
                m_sceneVoxelGrid.radiance = AssetManager::createTexture3D("VoxelGridRadiance", spec, parameter);

                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R32F;
                m_sceneVoxelGrid.opacity = AssetManager::createTexture3D("VoxelGridOpacity", spec, parameter);
            }

            {
                using ColorBuffers = Vctx::ColorBuffers;

                ITextureRenderable::Spec spec = { };
                spec.width = 1280;
                spec.height = 720;
                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R16G16B16;

                m_vctx.occlusion = AssetManager::createTexture2D("VctxOcclusion", spec);
                m_vctx.irradiance = AssetManager::createTexture2D("VctxIrradiance", spec);
                m_vctx.reflection = AssetManager::createTexture2D("VctxReflection", spec);

                m_vctx.renderTarget = createRenderTarget(spec.width, spec.height);
                m_vctx.renderTarget->setColorBuffer(m_vctx.occlusion, (u32)ColorBuffers::kOcclusion);
                m_vctx.renderTarget->setColorBuffer(m_vctx.irradiance, (u32)ColorBuffers::kIrradiance);
                m_vctx.renderTarget->setColorBuffer(m_vctx.reflection, (u32)ColorBuffers::kReflection);

                m_vctx.renderShader = ShaderManager::createShader({ ShaderType::kVsPs, "VctxShader", SHADER_SOURCE_PATH "vctx_v.glsl", SHADER_SOURCE_PATH "vctx_p.glsl" });
                m_vctx.resolveShader = ShaderManager::createShader({ ShaderType::kCs, "VoxelizeResolveShader", SHADER_SOURCE_PATH "voxelize_resolve_c.glsl" });
            }

            // global ssbo holding vctx data
            glCreateBuffers(1, &m_vctxSsbo);
            glNamedBufferData(m_vctxSsbo, sizeof(VctxGpuData), &m_vctxGpuData, GL_DYNAMIC_DRAW);
        }

        // todo: load global glsl definitions and save them in a map <path, content>
    }

    void Renderer::finalize()
    {
        // imgui clean up
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void Renderer::setShaderLightingParameters(const SceneRenderable& sceneRenderable, Shader* shader)
    {
        for (auto light : sceneRenderable.lights)
        {
            light->setShaderParameters(shader);
        }
    }

    void Renderer::drawMeshInstance(SceneRenderable& sceneRenderable, RenderTarget* renderTarget, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers, bool clearRenderTarget, Viewport viewport, GfxPipelineState pipelineState, MeshInstance* meshInstance, i32 transformIndex)
    {
        Mesh* parent = meshInstance->parent;
        for (u32 i = 0; i < parent->numSubmeshes(); ++i)
        {
            IMaterial* material = meshInstance->getMaterial(i);
            if (material)
            {
                RenderTask task = { };
                task.renderTarget = renderTarget;
                task.drawBuffers = drawBuffers;
                task.viewport = viewport;
                task.shader = material->getMaterialShader();
                task.renderSetupLambda = [this, &sceneRenderable, transformIndex, material](RenderTarget* renderTarget, Shader* shader) {
                    material->setShaderMaterialParameters();
                    if (material->lit())
                    {
                        setShaderLightingParameters(sceneRenderable, shader);
                    }
                    shader->setUniform("transformIndex", transformIndex);
                };

                submitRenderTask(std::move(task));
            }
        }
    }

    void calcSceneVoxelGridAABB(const BoundingBox3D& aabb, glm::vec3& pmin, glm::vec3& pmax)
    {
        pmin = aabb.pmin;
        pmax = aabb.pmax;
        glm::vec3 center = (pmin + pmax) * .5f;
        f32 len = max(pmax.x - pmin.x, pmax.y - pmin.y);
        len = max(len, pmax.z - pmin.z);
        pmin = center - glm::vec3(len) * .5f;
        pmax = center + glm::vec3(len) * .5f;
        // shrink the size of AABB to gain some resolution for now
        pmin *= .2f;
        pmax *= .2f;
    }

    void Renderer::voxelizeScene(Scene* scene)
    {
        auto& voxelizer = m_vctx.voxelizer;

        // clear voxel grid textures
        glClearTexImage(m_sceneVoxelGrid.albedo->getGpuResource(), 0, GL_RGBA, GL_UNSIGNED_INT, 0);
        glClearTexImage(m_sceneVoxelGrid.normal->getGpuResource(), 0, GL_RGBA, GL_UNSIGNED_INT, 0);
        glClearTexImage(m_sceneVoxelGrid.radiance->getGpuResource(), 0, GL_RGBA, GL_UNSIGNED_INT, 0);
        glClearTexImage(m_sceneVoxelGrid.opacity->getGpuResource(), 0, GL_R, GL_FLOAT, 0);

        enum class ImageBindings
        {
            kAlbedo = 0,
            kNormal,
            kEmission,
            kRadiance,
            kOpacity
        };
        // bind 3D volume texture to image units
        glBindImageTexture(static_cast<u32>(ImageBindings::kAlbedo), m_sceneVoxelGrid.albedo->getGpuResource(), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(static_cast<u32>(ImageBindings::kNormal), m_sceneVoxelGrid.normal->getGpuResource(), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(static_cast<u32>(ImageBindings::kRadiance), m_sceneVoxelGrid.radiance->getGpuResource(), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(static_cast<u32>(ImageBindings::kOpacity), m_sceneVoxelGrid.opacity->getGpuResource(), 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32F);
        if (m_vctx.opts.useSuperSampledOpacity > 0.f)
        {
            enum class SsboBindings
            {
                kOpacityMask = (i32)SceneSsboBindings::kCount,
                kDebugTexcoord
            };
            auto index = glGetProgramResourceIndex(voxelizer.voxelizeShader->getGpuResource(), GL_SHADER_STORAGE_BLOCK, "OpacityData");
            glShaderStorageBlockBinding(voxelizer.voxelizeShader->getGpuResource(), index, (u32)SsboBindings::kOpacityMask);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)SsboBindings::kOpacityMask, voxelizer.opacityMaskSsbo);
            f32 clear = 0.f;
            glClearNamedBufferData(voxelizer.opacityMaskSsbo, GL_R32F, GL_R, GL_FLOAT, &clear);
        }
        enum class Steps
        {
            kXAxis = 0,
            kYAxis,
            kZAxis,
            kCount
        };

        // get a cube shape aabb enclosing the scene
        glm::vec3 pmin, pmax;
        calcSceneVoxelGridAABB(scene->aabb, pmin, pmax);


        auto renderTarget = m_vctx.opts.useSuperSampledOpacity > 0.f ? m_vctx.voxelizer.ssRenderTarget : m_vctx.voxelizer.renderTarget;
        renderTarget->clear({ { 0 } });
        glEnable(GL_NV_conservative_raster);
        glDisable(GL_CULL_FACE);

        for (i32 axis = (i32)Steps::kXAxis; axis < (i32)Steps::kCount; ++axis)
        {
            for (u32 i = 0; i < scene->entities.size(); ++i)
            {
                visitEntity(scene->entities[i], [this, voxelizer, pmin, pmax, axis, renderTarget](SceneComponent* node) {
                    if (MeshInstance* meshInstance = node->getAttachedMesh())
                    {
                        for (u32 i = 0; i < meshInstance->parent->numSubmeshes(); ++i)
                        {
                            auto sm = meshInstance->parent->getSubmesh(i);

                            RenderTask task = { };
                            task.renderTarget = renderTarget;
                            task.drawBuffers = { { 0 } };
                            task.viewport = { 0u, 0u, renderTarget->width, renderTarget->height };
                            GfxPipelineState pipelineState;
                            pipelineState.depth = DepthControl::kDisable;
                            task.pipelineState = pipelineState;
                            task.shader = voxelizer.voxelizeShader;
                            task.submesh = sm;
                            task.renderSetupLambda = [voxelizer, pmin, pmax, node, axis, meshInstance, i](RenderTarget* renderTarget, Shader* shader) {
                                glm::mat4 model = node->getWorldTransform().toMatrix();
                                shader->setUniform("model", model)
                                        .setUniform("aabbMin", pmin)
                                        .setUniform("aabbMax", pmax)
                                        .setUniform("axis", (u32)axis);

                                PBRMatl* matl = meshInstance->getMaterial<PBRMatl>(i);
                                if (matl)
                                {
                                    Texture2DRenderable* albedo = matl->parameter.albedo;
                                    Texture2DRenderable* normal = matl->parameter.normal;
                                    voxelizer.voxelizeShader->setTexture("albedoTexture", albedo)
                                                            .setTexture("normalTexture", normal);
                                    u32 flags = matl->parameter.getFlags();
                                    voxelizer.voxelizeShader->setUniform("matlFlag", flags);
                                }
                            };

                            submitRenderTask(std::move(task));
                        }
                    }
                });
            }
        }

        glEnable(GL_CULL_FACE);
        glDisable(GL_NV_conservative_raster);

        // compute pass for resovling super sampled scene opacity
        if (m_vctx.opts.useSuperSampledOpacity > 0.f)
        {
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            enum class SsboBindings
            {
                kOpacityMask = (i32)SceneSsboBindings::kCount,
            };
            auto index = glGetProgramResourceIndex(m_vctx.resolveShader->getGpuResource(), GL_SHADER_STORAGE_BLOCK, "OpacityData");
            glShaderStorageBlockBinding(m_vctx.resolveShader->getGpuResource(), index, (u32)SsboBindings::kOpacityMask);
            m_ctx->setShader(m_vctx.resolveShader);
            glDispatchCompute(m_sceneVoxelGrid.resolution, m_sceneVoxelGrid.resolution, m_sceneVoxelGrid.resolution);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }

        if (m_settings.autoFilterVoxelGrid)
        {
            glGenerateTextureMipmap(m_sceneVoxelGrid.albedo->getGpuResource());
            glGenerateTextureMipmap(m_sceneVoxelGrid.radiance->getGpuResource());
            glGenerateTextureMipmap(m_sceneVoxelGrid.opacity->getGpuResource());
        }
        // manually filter & down-sample to generate mipmap with anisotropic voxels
        else
        {
            m_ctx->setShader(voxelizer.filterVoxelGridShader);

            const i32 kNumDownsample = 1u;
            for (i32 i = 0; i < kNumDownsample; ++i)
            {
                i32 textureUnit = (i32)SceneTextureBindings::kCount;
                glm::ivec3 currGridDim = glm::ivec3(m_sceneVoxelGrid.resolution) / (i32)pow(2, i+1);
                voxelizer.filterVoxelGridShader->setUniform("srcMip", i);
                voxelizer.filterVoxelGridShader->setUniform("srcAlbedo", textureUnit);
                glBindTextureUnit(textureUnit++, m_sceneVoxelGrid.albedo->getGpuResource());

                voxelizer.filterVoxelGridShader->setUniform("dstAlbedo", textureUnit);
                glBindImageTexture(textureUnit++, m_sceneVoxelGrid.albedo->getGpuResource(), i+1, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
                glDispatchCompute(currGridDim.x, currGridDim.y, currGridDim.z);
            }
        }

        updateVctxData(scene);

        // sync
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void Renderer::updateVctxData(Scene* scene)
    {
        // bind textures
        glBindTextureUnit((u32)SceneTextureBindings::VoxelGridAlbedo, m_sceneVoxelGrid.albedo->getGpuResource());
        glBindTextureUnit((u32)SceneTextureBindings::VoxelGridNormal, m_sceneVoxelGrid.normal->getGpuResource());
        glBindTextureUnit((u32)SceneTextureBindings::VoxelGridRadiance, m_sceneVoxelGrid.radiance->getGpuResource());
        glBindTextureUnit((u32)SceneTextureBindings::VoxelGridOpacity, m_sceneVoxelGrid.opacity->getGpuResource());

        // update buffer data
        glm::vec3 pmin, pmax;
        calcSceneVoxelGridAABB(scene->aabb, pmin, pmax);
        m_vctxGpuData.localOrigin = glm::vec3(pmin.x, pmin.y, pmax.z);
        m_vctxGpuData.voxelSize = (pmax.x - pmin.x) / (f32)m_sceneVoxelGrid.resolution;
        m_vctxGpuData.visMode = 1 << static_cast<i32>(m_vctx.visualizer.mode);
        glNamedBufferSubData(m_vctxSsbo, 0, sizeof(VctxGpuData), &m_vctxGpuData);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)SceneSsboBindings::VctxGlobalData, m_vctxSsbo);
    }

    void Renderer::visualizeVoxelGrid()
    {
        glDisable(GL_CULL_FACE);
        {
            m_ctx->setRenderTarget(m_vctx.visualizer.renderTarget, { RenderTargetDrawBuffer{ 0 } });
            m_vctx.visualizer.renderTarget->clear({ RenderTargetDrawBuffer{ 0 } });
            m_ctx->setViewport({0u, 0u, m_vctx.visualizer.renderTarget->width, m_vctx.visualizer.renderTarget->height});

            m_ctx->setShader(m_vctx.visualizer.voxelGridVisShader);
            m_vctx.visualizer.voxelGridVisShader->setUniform("activeMip", m_vctx.visualizer.activeMip);
            m_ctx->setPrimitiveType(PrimitiveMode::Points);

            u32 currRes = m_sceneVoxelGrid.resolution / pow(2, m_vctx.visualizer.activeMip);
            m_ctx->drawIndexAuto(currRes * currRes * currRes);
        }
        m_ctx->setPrimitiveType(PrimitiveMode::TriangleList);
        glEnable(GL_CULL_FACE);
    }

    void Renderer::visualizeConeTrace(Scene* scene)
    {
        auto& visualizer = m_vctx.visualizer;

        // debug vis for voxel cone tracing
        auto sceneDepthTexture = m_settings.enableAA ? m_sceneDepthTextureSSAA : m_sceneDepthTexture;
        auto sceneNormalTexture = m_settings.enableAA ? m_sceneNormalTextureSSAA : m_sceneNormalTexture;
        if (!visualizer.cachedTexInitialized || visualizer.debugScreenPosMoved)
        {
            if (visualizer.cachedSceneDepth->width == sceneDepthTexture->width && visualizer.cachedSceneDepth->height == sceneDepthTexture->height &&
                visualizer.cachedSceneNormal->width == sceneNormalTexture->width && visualizer.cachedSceneNormal->height == sceneNormalTexture->height)
            {
                // update cached snapshot of scene depth/normal by copying texture data
                glCopyImageSubData(sceneDepthTexture->getGpuResource(), GL_TEXTURE_2D, 0, 0, 0, 0, 
                    visualizer.cachedSceneDepth->getGpuResource(), GL_TEXTURE_2D, 0, 0, 0, 0, sceneDepthTexture->width, sceneDepthTexture->height, 1);
                glCopyImageSubData(sceneNormalTexture->getGpuResource(), GL_TEXTURE_2D, 0, 0, 0, 0, 
                    visualizer.cachedSceneNormal->getGpuResource(), GL_TEXTURE_2D, 0, 0, 0, 0, sceneNormalTexture->width, sceneNormalTexture->height, 1);


        //       visualizer.cachedView = m_globalDrawData.view;
        //       visualizer.cachedProjection = m_globalDrawData.projection;

                if (!visualizer.cachedTexInitialized)
                {
                    visualizer.cachedTexInitialized = true;
                }
            }
            else
            {
                cyanError("Invalid texture copying operation due to mismatched texture dimensions!");
            }
        }

        // pass 0: run a compute pass to fill debug cone data and visualize cone directions
        m_ctx->setShader(visualizer.coneVisComputeShader);

        enum class SsboBindings
        {
            kIndirectDraw = 5,
            kConeData
        };

        auto index = glGetProgramResourceIndex(visualizer.coneVisComputeShader->getGpuResource(), GL_SHADER_STORAGE_BLOCK, "IndirectDrawArgs");
        glShaderStorageBlockBinding(visualizer.coneVisComputeShader->getGpuResource(), index, (u32)SsboBindings::kIndirectDraw);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)SsboBindings::kIndirectDraw, visualizer.idbo);

        index = glGetProgramResourceIndex(visualizer.coneVisComputeShader->getGpuResource(), GL_SHADER_STORAGE_BLOCK, "ConeTraceDebugData");
        glShaderStorageBlockBinding(visualizer.coneVisComputeShader->getGpuResource(), index, (u32)SsboBindings::kConeData);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)SsboBindings::kConeData, visualizer.ssbo);

        enum class TexBindings
        {
            kSceneDepth = 0,
            kSceneNormal
        };

        visualizer.coneVisComputeShader->setUniform("debugScreenPos", visualizer.debugScreenPos);
        visualizer.coneVisComputeShader->setUniform("cachedView", visualizer.cachedView);
        visualizer.coneVisComputeShader->setUniform("cachedProjection", visualizer.cachedProjection);
        glm::vec2 renderSize = glm::vec2(visualizer.renderTarget->width, visualizer.renderTarget->height);
        visualizer.coneVisComputeShader->setUniform("renderSize", renderSize);
        visualizer.coneVisComputeShader->setUniform("sceneDepthTexture", (i32)TexBindings::kSceneDepth);
        visualizer.coneVisComputeShader->setUniform("sceneNormalTexture", (i32)TexBindings::kSceneNormal);
        visualizer.coneVisComputeShader->setUniform("opts.occlusionScale", m_vctx.opts.occlusionScale);
        visualizer.coneVisComputeShader->setUniform("opts.coneOffset", m_vctx.opts.coneOffset);
        glBindTextureUnit((u32)TexBindings::kSceneDepth, m_vctx.visualizer.cachedSceneDepth->getGpuResource());
        glBindTextureUnit((u32)TexBindings::kSceneNormal, m_vctx.visualizer.cachedSceneNormal->getGpuResource());

        glDispatchCompute(1, 1, 1);

        // sync
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // visualize traced cones
        m_ctx->setRenderTarget(m_vctx.visualizer.renderTarget, { RenderTargetDrawBuffer{ 0 } });
        m_ctx->setViewport({ 0, 0, m_vctx.visualizer.renderTarget->width, m_vctx.visualizer.renderTarget->height });
        m_ctx->setShader(visualizer.coneVisDrawShader);

#if 0
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
        // indirect draw to launch vs/gs
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_vctxVis.idbo);
        glDrawArraysIndirect(GL_POINTS, 0);
#else
        index = glGetProgramResourceIndex(visualizer.coneVisDrawShader->getGpuResource(), GL_SHADER_STORAGE_BLOCK, "ConeTraceDebugData");
        glShaderStorageBlockBinding(visualizer.coneVisDrawShader->getGpuResource(), index, (u32)SsboBindings::kConeData);

        struct IndirectDrawArgs
        {
            GLuint first;
            GLuint count;
        } command;
        glGetNamedBufferSubData(visualizer.idbo, 0, sizeof(IndirectDrawArgs), &command);
        m_ctx->setPrimitiveType(PrimitiveMode::Points);
        glDisable(GL_CULL_FACE);
        m_ctx->drawIndexAuto(command.count);
#endif
        m_ctx->setPrimitiveType(PrimitiveMode::TriangleList);
        glEnable(GL_CULL_FACE);
    }

    void Renderer::drawEntity(SceneRenderable& renderableScene, RenderTarget* renderTarget, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers, bool clearRenderTarget, Viewport viewport, GfxPipelineState pipelineState, Entity* entity)
    {
        std::queue<SceneComponent*> nodes;
        nodes.push(entity->m_sceneRoot);
        while (!nodes.empty())
        {
            auto node = nodes.front();
            nodes.pop();
            if (auto meshInst = node->getAttachedMesh())
            {
                drawMeshInstance(renderableScene, renderTarget, drawBuffers, clearRenderTarget, viewport, pipelineState, meshInst, node->globalTransform);
            }
            for (u32 i = 0; i < node->m_child.size(); ++i)
            {
                nodes.push(node->m_child[i]);
            }
        }
    }

    void Renderer::submitMesh(RenderTarget* renderTarget, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers, bool clearRenderTarget, Viewport viewport, GfxPipelineState pipelineState, Mesh* mesh, Shader* shader, const RenderSetupLambda& perMeshSetupLambda)
    {
        perMeshSetupLambda(renderTarget, shader);

        for (u32 i = 0; i < mesh->numSubmeshes(); ++i)
        {
            RenderTask task = { };
            task.renderTarget = renderTarget;
            task.drawBuffers = drawBuffers;
            task.clearRenderTarget = clearRenderTarget;
            task.viewport = viewport;
            task.shader = shader;
            task.submesh = mesh->getSubmesh(i);

            submitRenderTask(std::move(task));
        }
    }

    void Renderer::submitMaterialMesh(RenderTarget* renderTarget, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers, bool clearRenderTarget, Viewport viewport, GfxPipelineState pipelineState, Mesh* mesh, IMaterial* material, const RenderSetupLambda& perMeshSetupLambda)
    {
        if (material)
        {
            material->setShaderMaterialParameters();
            Shader* shader = material->getMaterialShader();
            perMeshSetupLambda(renderTarget, shader);

            for (u32 i = 0; i < mesh->numSubmeshes(); ++i)
            {
                RenderTask task = { };
                task.renderTarget = renderTarget;
                task.drawBuffers = drawBuffers;
                task.clearRenderTarget = clearRenderTarget;
                task.viewport = viewport;
                task.shader = shader;
                task.submesh = mesh->getSubmesh(i);

                submitRenderTask(std::move(task));
            }
        }
    }

    void Renderer::submitFullScreenPass(RenderTarget* renderTarget, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers, Shader* shader, RenderSetupLambda&& renderSetupLambda)
    {
        GfxPipelineState pipelineState;
        pipelineState.depth = DepthControl::kDisable;

        submitMesh(
            renderTarget,
            drawBuffers,
            false,
            { 0, 0, renderTarget->width, renderTarget->height },
            pipelineState,
            fullscreenQuad,
            shader,
            std::move(renderSetupLambda)
            );
    }

    void Renderer::submitRenderTask(RenderTask&& task)
    {
        m_ctx->setRenderTarget(task.renderTarget, task.drawBuffers);
        if (task.clearRenderTarget)
        {
            task.renderTarget->clear(task.drawBuffers);
        }
        m_ctx->setViewport(task.viewport);

        task.shader->commit(m_ctx);
        m_ctx->setShader(task.shader);

        // configure misc state
        m_ctx->setDepthControl(task.pipelineState.depth);
        m_ctx->setPrimitiveType(task.pipelineState.primitiveMode);

        // pre-draw setup
        task.renderSetupLambda(task.renderTarget, task.shader);

        // kick off the draw call
        auto va = task.submesh->getVertexArray();
        m_ctx->setVertexArray(va);
        if (va->hasIndexBuffer())
        {
            m_ctx->drawIndex(task.submesh->numIndices());
        }
        else
        {
            m_ctx->drawIndexAuto(task.submesh->numVertices());
        }
    }

    /*
        * render passes should be pushed after call to beginRender() 
    */
    void Renderer::beginRender()
    {
        // todo: broadcast beginRender event 

        // reset frame allocator
        m_frameAllocator.reset();

        // clear default render target
        m_ctx->setRenderTarget(nullptr, { });
        m_ctx->clear();
    }

    void Renderer::render(Scene* scene, const std::function<void()>& externDebugRender)
    {
        //-------------------------------------------------------------------
        /*
        * What composes a frame in Cyan ? 
        * pre main scene pass
            * render shadow maps for all the shadow casting light sources
            * render depth & normal pass
            * render effects that are dependent on depth & normal pass
            * voxel cone tracing preparation
        * main scene pass 
            * render the scene
        * post-processing pass
            * bloom
            * tone mapping
        */
        //-------------------------------------------------------------------

        auto renderTarget = m_settings.enableAA ? m_sceneColorRTSSAA : m_sceneColorRenderTarget;
        SceneView sceneView(*scene, scene->camera, renderTarget, { }, { 0u, 0u,  renderTarget->width, renderTarget->height }, EntityFlag_kVisible);
        // convert Scene instance to RenderableScene instance for rendering
        SceneRenderable sceneRenderable(scene, sceneView, m_frameAllocator);

        beginRender();
        {
            renderShadow(sceneRenderable);

            struct DrawBuffer
            {
                i32 binding;
                glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);
                glm::vec4 clearDepth = glm::vec4(0.f, 0.f, 0.f, 1.f);
            };

            // scene depth & normal pass
            bool depthNormalPrepassRequired = (m_settings.enableSSAO || m_settings.enableVctx);
            if (depthNormalPrepassRequired)
            {
                sceneView.drawBuffers = {
                    RenderTargetDrawBuffer{ (i32)(SceneColorBuffers::kDepth), glm::vec4(1.f) },
                    RenderTargetDrawBuffer{ (i32)(SceneColorBuffers::kNormal) }
                };
                renderSceneDepthNormal(sceneRenderable, sceneView);
            }

            // voxel cone tracing pass
            if (m_settings.enableVctx)
            {
                // todo: revoxelizing the scene every frame is causing flickering in the volume texture
                voxelizeScene(scene);
                renderVctx();
            }

            // ssao pass
            if (m_settings.enableSSAO)
            {
                Texture2DRenderable* sceneDepthTexture = m_settings.enableAA ? m_sceneDepthTextureSSAA : m_sceneDepthTexture;
                Texture2DRenderable* sceneNormalTexture = m_settings.enableAA ? m_sceneNormalTextureSSAA : m_sceneNormalTexture;
                ssao(sceneView, sceneDepthTexture, sceneNormalTexture);
            }

            // main scene pass
            sceneView.drawBuffers = {
                RenderTargetDrawBuffer{ (i32)(SceneColorBuffers::kSceneColor) }
            };
            renderScene(sceneRenderable, sceneView);

            // debug object pass
            renderDebugObjects(scene, externDebugRender);

            // post processing
            {
                // reset state
                m_bloomOutTexture = nullptr;
                m_finalColorTexture = m_settings.enableAA ? m_sceneColorTextureSSAA : m_sceneColorTexture;

                if (m_settings.enableBloom)
                {
                    bloom();
                }
                composite();
            }
        }
        endRender();
    }

    void Renderer::endRender()
    {
        // reset render target
        m_ctx->setRenderTarget(nullptr, {});
        m_ctx->flip();
    }

    void Renderer::updateSunShadow(const NewCascadedShadowmap& csm)
    {
#if 0
        m_globalDrawData.sunLightView   = csm.lightView;
        // bind sun light shadow map for this frame
        u32 textureUnitOffset = (u32)SceneTextureBindings::SunShadow;
        for (u32 i = 0; i < CascadedShadowmap::kNumCascades; ++i)
        {
            m_globalDrawData.sunShadowProjections[i] = csm.cascades[i].lightProjection;
            if (csm.technique == kVariance_Shadow)
                glBindTextureUnit(textureUnitOffset + i, csm.cascades[i].vsm.shadowmap->handle);
            else if (csm.technique == kPCF_Shadow)
                glBindTextureUnit(textureUnitOffset + i, csm.cascades[i].basicShadowmap.shadowmap->handle);
        }

        // upload data to gpu
        glNamedBufferSubData(gDrawDataBuffer, 0, sizeof(GlobalDrawData), &m_globalDrawData);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<u32>(SceneBufferBindings::kViewData), gDrawDataBuffer);
#endif
    }

    void Renderer::renderSunShadow(Scene* scene, const std::vector<Entity*>& shadowCasters)
    {
#if 0
        auto& sunLight = scene->dLights[0];
        m_rasterDirectShadowManager->render(m_csm, scene, sunLight, shadowCasters);
        updateSunShadow(m_csm);
#endif
    }

    void Renderer::renderShadow(SceneRenderable& sceneRenderable)
    {
        for (auto light : sceneRenderable.lights)
        {
            light->renderShadow(*(sceneRenderable.scene), *this);
        }
    }

    void Renderer::renderSceneMeshOnly(SceneRenderable& sceneRenderable, const SceneView& sceneView, Shader* shader)
    {
        sceneRenderable.submitSceneData(m_ctx);

        u32 transformIndex = 0u;
        for (auto& meshInst : sceneRenderable.meshInstances)
        {
            submitMesh(
                sceneView.renderTarget, // renderTarget
                sceneView.drawBuffers,
                false,
                { 0u, 0u, sceneView.renderTarget->width, sceneView.renderTarget->height }, // viewport
                GfxPipelineState(), // pipeline state
                meshInst->parent, // mesh
                shader, // shader
                [&transformIndex](RenderTarget* renderTarget, Shader* shader) { // renderSetupLambda
                    shader->setUniform("transformIndex", transformIndex++);
                });
        }
    }

    // todo: refactor this and 
    void Renderer::renderSceneDepthOnly(SceneRenderable& sceneRenderable, const SceneView& sceneView)
    {
        sceneRenderable.submitSceneData(m_ctx);

        u32 transformIndex = 0u;
        for (auto& meshInst : sceneRenderable.meshInstances)
        {
            submitMesh(
                sceneView.renderTarget, // renderTarget
                sceneView.drawBuffers,
                false,
                { 0u, 0u, sceneView.renderTarget->width, sceneView.renderTarget->height }, // viewport
                GfxPipelineState(), // pipeline state
                meshInst->parent, // mesh
                m_sceneDepthNormalShader, // shader
                [&transformIndex](RenderTarget* renderTarget, Shader* shader) { // renderSetupLambda
                    shader->setUniform("transformIndex", transformIndex++);
                });
        }
    }

    void Renderer::renderSceneDepthNormal(SceneRenderable& renderableScene, const SceneView& sceneView)
    {
        renderableScene.submitSceneData(m_ctx);

        u32 transformIndex = 0u;
        for (auto& meshInst : renderableScene.meshInstances)
        {
            submitMesh(
                sceneView.renderTarget, // renderTarget
                sceneView.drawBuffers,
                false,
                { 0u, 0u, sceneView.renderTarget->width, sceneView.renderTarget->height }, // viewport
                GfxPipelineState(), // pipeline state
                meshInst->parent, // mesh
                m_sceneDepthNormalShader, // shader
                [&transformIndex](RenderTarget* renderTarget, Shader* shader) { // renderSetupLambda
                    shader->setUniform("transformIndex", transformIndex++);
                });
        }
    }

    void Renderer::renderScene(SceneRenderable& sceneRenderable, const SceneView& sceneView)
    {
        sceneRenderable.submitSceneData(m_ctx);

        // render skybox
        if (sceneRenderable.skybox)
        {
            sceneRenderable.skybox->render();
        }

        // render mesh instances
        u32 transformIndex = 0u;
        for (auto meshInst : sceneRenderable.meshInstances)
        {
            drawMeshInstance(
                sceneRenderable,
                sceneView.renderTarget, 
                sceneView.drawBuffers, 
                false,
                sceneView.viewport, 
                GfxPipelineState(), 
                meshInst, 
                transformIndex++
            );
        }
    }

    void Renderer::ssao(const SceneView& sceneView, Texture2DRenderable* sceneDepthTexture, Texture2DRenderable* sceneNormalTexture)
    {
        submitFullScreenPass(
            sceneView.renderTarget,
            const_cast<std::initializer_list<RenderTargetDrawBuffer>&&>(sceneView.drawBuffers),
            m_ssaoShader, 
            [this, sceneDepthTexture, sceneNormalTexture, &sceneView](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("normalTexture", sceneNormalTexture)
                    .setTexture("depthTexture", sceneDepthTexture)
                    .setUniform("cameraPos", sceneView.camera.position)
                    .setUniform("view", sceneView.camera.view)
                    .setUniform("projection", sceneView.camera.projection);
            });
    }

    void Renderer::bloom()
    {
        // be aware that these functions modify 'renderTarget''s state
        auto downsample = [this](Texture2DRenderable* dst, Texture2DRenderable* src, RenderTarget* renderTarget) {
            enum class ColorBuffer
            {
                kDst = 0
            };

            if (!dst || !src)
            {
                return;
            }

            renderTarget->setColorBuffer(dst, static_cast<u32>(ColorBuffer::kDst));
            submitFullScreenPass(
                renderTarget, 
                { { (i32)ColorBuffer::kDst } },
                m_bloomDsShader, 
                [this, src](RenderTarget* renderTarget, Shader* shader) {
                    shader->setTexture("srcImage", src);
                });
        };

        auto upscale = [this](Texture2DRenderable* dst, Texture2DRenderable* src, Texture2DRenderable* blend, RenderTarget* renderTarget, u32 stageIndex) {
            enum class ColorBuffer
            {
                kDst = 0
            };

            if (!dst || !src)
            {
                return;
            }

            renderTarget->setColorBuffer(dst, static_cast<u32>(ColorBuffer::kDst));

            submitFullScreenPass(
                renderTarget, 
                { { (i32)ColorBuffer::kDst } },
                m_bloomUsShader, 
                [this, src, blend, stageIndex](RenderTarget* renderTarget, Shader* shader) {
                    shader->setTexture("srcImage", src)
                        .setTexture("blendImage", blend)
                        .setUniform("stageIndex", stageIndex);
                });
        };

        // user have to make sure that renderTarget is compatible with 'dst', 'src', and 'scratch'
        auto gaussianBlur = [this](Texture2DRenderable* dst, Texture2DRenderable* src, Texture2DRenderable* scratch, RenderTarget* renderTarget, i32 kernelIndex, i32 radius) {

            enum class ColorBuffer
            {
                kSrc = 0,
                kScratch,
                kDst
            };

            renderTarget->setColorBuffer(src, static_cast<u32>(ColorBuffer::kSrc));
            renderTarget->setColorBuffer(scratch, static_cast<u32>(ColorBuffer::kScratch));
            if (dst)
            {
                renderTarget->setColorBuffer(dst, static_cast<u32>(ColorBuffer::kDst));
            }

            // horizontal pass (blur 'src' into 'scratch')
            {
                submitFullScreenPass(
                    renderTarget, 
                    { { static_cast<u32>(ColorBuffer::kScratch) } },
                    m_gaussianBlurShader, 
                    [this, src, kernelIndex, radius](RenderTarget* renderTarget, Shader* shader) {
                        m_gaussianBlurShader->setTexture("srcTexture", src)
                                            .setUniform("horizontal", 1.f)
                                            .setUniform("kernelIndex", kernelIndex)
                                            .setUniform("radius", radius);
                    });
            }

            // vertical pass
            {
                std::initializer_list<RenderTargetDrawBuffer> drawBuffers;
                if (dst)
                {
                    drawBuffers = { { (i32)ColorBuffer::kDst } };
                }
                else
                {
                    drawBuffers = { { (i32)ColorBuffer::kSrc } };
                }

                submitFullScreenPass(
                    renderTarget, 
                    std::move(drawBuffers),
                    m_gaussianBlurShader, 
                    [this, scratch, kernelIndex, radius](RenderTarget* renderTarget, Shader* shader) {
                        shader->setTexture("srcTexture", scratch)
                                .setUniform("horizontal", 0.f)
                                .setUniform("kernelIndex", kernelIndex)
                                .setUniform("radius", radius);
                    });
            }
        };

        auto* src = m_settings.enableAA ? m_sceneColorTextureSSAA : m_sceneColorTexture;

        // bloom setup pass
        submitFullScreenPass(
            m_bloomSetupRenderTarget, 
            { { 0u } },
            m_bloomSetupShader, 
            [this, src](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcTexture", src);
            });

        // downsample
        i32 kernelRadii[6] = { 3, 4, 6, 7, 8, 9};
        downsample(m_bloomDsTargets[0].src, dynamic_cast<Texture2DRenderable*>(m_bloomSetupRenderTarget->getColorBuffer(0u)), m_bloomDsTargets[0].renderTarget);
        gaussianBlur(nullptr, m_bloomDsTargets[0].src, m_bloomDsTargets[0].scratch, m_bloomDsTargets[0].renderTarget, 0, kernelRadii[0]);
        for (u32 i = 0u; i < kNumBloomPasses - 1; ++i) 
        {
            downsample(m_bloomDsTargets[i+1].src, m_bloomDsTargets[i].src, m_bloomDsTargets[i+1].renderTarget);
            gaussianBlur(nullptr, m_bloomDsTargets[i+1].src, m_bloomDsTargets[i+1].scratch, m_bloomDsTargets[i+1].renderTarget, (i32)(i+1), kernelRadii[i+1]);
        }

        // upscale
        u32 start = kNumBloomPasses - 2, end = 0;
        upscale(m_bloomUsTargets[start].src, m_bloomDsTargets[kNumBloomPasses - 1].src, nullptr, m_bloomUsTargets[start].renderTarget, 0u);
        gaussianBlur(nullptr, m_bloomDsTargets[start].src, m_bloomDsTargets[start].scratch, m_bloomDsTargets[start].renderTarget, start, kernelRadii[start]);

        // final output will be written to m_bloomUsTarget[0].src
        for (u32 i = start; i > end ; --i) 
        {
            u32 srcIndex = i, dstIndex = i - 1;
            upscale(m_bloomUsTargets[dstIndex].src, m_bloomUsTargets[srcIndex].src, m_bloomDsTargets[dstIndex].src, m_bloomUsTargets[dstIndex].renderTarget, start - i + 1u);
            gaussianBlur(nullptr, m_bloomUsTargets[dstIndex].src, m_bloomUsTargets[dstIndex].scratch, m_bloomUsTargets[dstIndex].renderTarget, (i32)(dstIndex), kernelRadii[dstIndex]);
        }

        m_bloomOutTexture = m_bloomUsTargets[0].src;
    }

    void Renderer::composite()
    {
        submitFullScreenPass(
            m_compositeRenderTarget,
            { { 0 } },
            m_compositeShader,
            [this](RenderTarget* renderTarget, Shader* shader) {
                shader->setUniform("exposure", m_settings.exposure)
                    .setUniform("bloom", m_settings.enableBloom ? 1.f : 0.f)
                    .setUniform("bloomIntensity", m_settings.bloomIntensity)
                    .setTexture("bloomOutTexture", m_bloomOutTexture)
                    .setTexture("sceneColorTexture", m_settings.enableAA ? m_sceneColorTextureSSAA : m_sceneColorTexture);
            });

        m_finalColorTexture = m_compositeColorTexture;
    }
    
    void Renderer::renderUI(const std::function<void()>& callback)
    {
        // set to default render target
        m_ctx->setRenderTarget(nullptr, { });

        // begin imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        // draw widgets defined in app
        callback();

        // draw built-in widgets used for configure the renderer
        ImGuiWindowFlags flags = ImGuiWindowFlags_None | ImGuiWindowFlags_NoResize;
        ImGui::SetNextWindowSize(ImVec2(500.f, 900.f));
        ImGui::SetNextWindowPos(ImVec2(430.f, 30.f));
        ImGui::SetNextWindowBgAlpha(0.5f);
        ImGui::Begin("VCTX", nullptr, flags);
        {
            // pipelineState for VCTX
            ImGui::AlignTextToFramePadding();

            ImGui::Text("VisMode "); ImGui::SameLine();
            ImGui::Combo("##VisMode", reinterpret_cast<i32*>(&m_vctx.visualizer.mode), m_vctx.visualizer.vctxVisModeNames, (u32)Vctx::Visualizer::Mode::kCount);

            ImGui::Text("Mip "); ImGui::SameLine();
            ImGui::SliderInt("##Mip", &m_vctx.visualizer.activeMip, 0, log2(m_sceneVoxelGrid.resolution));

            ImGui::Text("Offset "); ImGui::SameLine();
            ImGui::SliderFloat("##Offset", &m_vctx.opts.coneOffset, 0.f, 5.f, "%.2f");

            ImGui::Text("Opacity Scale "); ImGui::SameLine();
            ImGui::SliderFloat("##Opacity Scale", &m_vctx.opts.opacityScale, 1.f, 5.f, "%.2f");

            ImGui::Text("Ao Scale "); ImGui::SameLine();
            ImGui::SliderFloat("##Ao Scale", &m_vctx.opts.occlusionScale, 1.f, 5.f, "%.2f");

            ImGui::Text("Indirect Scale "); ImGui::SameLine();
            ImGui::SliderFloat("##Indirect Scale", &m_vctx.opts.indirectScale, 1.f, 5.f, "%.2f");

            ImGui::Text("Debug ScreenPos"); ImGui::SameLine();
            f32 v[2] = { m_vctx.visualizer.debugScreenPos.x, m_vctx.visualizer.debugScreenPos.y };
            m_vctx.visualizer.debugScreenPosMoved = ImGui::SliderFloat2("##Debug ScreenPos", v, -1.f, 1.f, "%.2f");
            m_vctx.visualizer.debugScreenPos = glm::vec2(v[0], v[1]);

            ImGui::Separator();
            ImVec2 visSize(480, 270);
            ImGui::Text("VoxelGrid Vis "); 
            ImGui::Image((ImTextureID)m_vctx.visualizer.colorBuffer->getGpuResource(), visSize, ImVec2(0, 1), ImVec2(1, 0));

            ImGui::Separator();
            ImGui::Text("Vctx Occlusion");
            ImGui::Image((ImTextureID)m_vctx.occlusion->getGpuResource(), visSize, ImVec2(0, 1), ImVec2(1, 0));

            ImGui::Separator();
            ImGui::Text("Vctx Irradiance");
            ImGui::Image((ImTextureID)m_vctx.irradiance->getGpuResource(), visSize, ImVec2(0, 1), ImVec2(1, 0));
        }
        ImGui::End();

        // end imgui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // todo: refactor this, this should really just be renderScene()
    void Renderer::renderSceneToLightProbe(Scene* scene, LightProbe* probe, RenderTarget* renderTarget)
    {
#if 0 
        // updateTransforms(scene);

        // only capture static objects
        std::vector<Entity*> staticObjects;
        for (u32 i = 0; i < scene->entities.size(); ++i)
        {
            if (scene->entities[i]->m_static)
            {
                staticObjects.push_back(scene->entities[i]);
            }
        }

        // render sun light shadow
        renderSunShadow(scene, staticObjects);

        // turn off ssao
        m_settings.enableSSAO = false;

        // render scene into each face of the cubemap
        Camera camera = { };
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
        camera.n = 0.1f;
        camera.f = 100.f;
        camera.fov = glm::radians(90.f);

        for (u32 f = 0; f < (sizeof(LightProbeCameras::cameraFacingDirections)/sizeof(LightProbeCameras::cameraFacingDirections[0])); ++f)
        {
            camera.position = probe->position;
            camera.lookAt = camera.position + LightProbeCameras::cameraFacingDirections[f];
            camera.worldUp = LightProbeCameras::worldUps[f];
            camera.update();

            // updateFrameGlobalData(scene, camera);
            // draw skybox
            if (scene->m_skybox)
            {
                scene->m_skybox->render();
            }
            renderTarget->clear({ { (i32)f } });
            // draw entities 
            for (u32 i = 0; i < staticObjects.size(); ++i)
            {
                drawEntity(
                    renderTarget,
                    { { (i32)f}, {}, {}, {} },
                    false,
                    { 0u, 0u, probe->sceneCapture->width, probe->sceneCapture->height }, 
                    GfxPipelineState(), 
                    staticObjects[i]);
            }
        }

        m_settings.enableSSAO = true;
#endif
    }

    void Renderer::renderVctx()
    {
        using ColorBuffers = Vctx::ColorBuffers;

        m_ctx->setRenderTarget(m_vctx.renderTarget, { 
            { (i32)ColorBuffers::kOcclusion },
            { (i32)ColorBuffers::kIrradiance }, 
            { (i32)ColorBuffers::kReflection }
            });
        m_ctx->setViewport({ 0u, 0u, m_vctx.renderTarget->width, m_vctx.renderTarget->height });
        m_ctx->setShader(m_vctx.renderShader);

        enum class TexBindings
        {
            kDepth = 0,
            kNormal
        };
        glm::vec2 renderSize = glm::vec2(m_vctx.renderTarget->width, m_vctx.renderTarget->height);
        m_vctx.renderShader->setUniform("renderSize", renderSize);
        m_vctx.renderShader->setUniform("sceneDepthTexture", (i32)TexBindings::kDepth);
        m_vctx.renderShader->setUniform("sceneNormalTexture", (i32)TexBindings::kNormal);
        m_vctx.renderShader->setUniform("opts.coneOffset", m_vctx.opts.coneOffset);
        m_vctx.renderShader->setUniform("opts.occlusionScale", m_vctx.opts.occlusionScale);
        m_vctx.renderShader->setUniform("opts.indirectScale", m_vctx.opts.indirectScale);
        m_vctx.renderShader->setUniform("opts.opacityScale", m_vctx.opts.opacityScale);
        auto depthTexture = m_settings.enableAA ? m_sceneDepthTextureSSAA : m_sceneDepthTexture;
        auto normalTexture = m_settings.enableAA ? m_sceneNormalTextureSSAA : m_sceneNormalTexture;
        glBindTextureUnit((u32)TexBindings::kDepth, depthTexture->getGpuResource());
        glBindTextureUnit((u32)TexBindings::kNormal, normalTexture->getGpuResource());
        m_ctx->setPrimitiveType(PrimitiveMode::TriangleList);
        m_ctx->setDepthControl(DepthControl::kDisable);
        m_ctx->drawIndexAuto(6u);
        m_ctx->setDepthControl(DepthControl::kEnable);

        // bind textures for rendering
        glBindTextureUnit((u32)SceneTextureBindings::VctxOcclusion, m_vctx.occlusion->getGpuResource());
        glBindTextureUnit((u32)SceneTextureBindings::VctxIrradiance, m_vctx.irradiance->getGpuResource());
        glBindTextureUnit((u32)SceneTextureBindings::VctxReflection, m_vctx.reflection->getGpuResource());
    }

    void Renderer::renderDebugObjects(Scene* scene, const std::function<void()>& externDebugRender)
    {
        // renderer internal debug draw calls
        {
            visualizeVoxelGrid();
            visualizeConeTrace(scene);
        }

        // external debug draw calls defined by the application
        externDebugRender();
    }
}