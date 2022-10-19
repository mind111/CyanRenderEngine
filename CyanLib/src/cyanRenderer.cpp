#include <iostream>
#include <fstream>
#include <queue>
#include <functional>

#include "glfw3.h"
#include "gtx/quaternion.hpp"
#include "glm/gtc/integer.hpp"
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
#include "LightRenderable.h"
#include "RayTracingScene.h"
#include "IOSystem.h"

#define GPU_RAYTRACING 0

namespace Cyan
{
#if 0
    void RenderTexture2D::createResource(RenderQueue& renderQueue)
    { 
        if (!texture)
        {
            texture = renderQueue.createTexture2DInternal(tag, spec);
        }
    }
#endif
    
    Renderer* Singleton<Renderer>::singleton = nullptr;
    static Mesh* fullscreenQuad = nullptr;

    Renderer::Renderer(GfxContext* ctx, u32 windowWidth, u32 windowHeight)
        : Singleton<Renderer>(), 
        m_ctx(ctx),
        m_windowSize(windowWidth, windowHeight),
        m_frameAllocator(1024 * 1024 * 32) // 32MB frame allocator
    {
        m_manyViewGI = std::make_unique<ManyViewGI>(this, m_ctx);
        m_microRenderingGI = std::make_unique<MicroRenderingGI>(this, m_ctx);
    }

    void Renderer::Vctx::Voxelizer::init(u32 resolution)
    {
        ITextureRenderable::Spec voxelizeSpec = { };
        voxelizeSpec.width = resolution;
        voxelizeSpec.height = resolution;
        voxelizeSpec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB16F;

        ITextureRenderable::Spec voxelizeSuperSampledSpec = { };
        voxelizeSuperSampledSpec.width = resolution * ssaaRes;
        voxelizeSuperSampledSpec.height = resolution * ssaaRes;
        voxelizeSuperSampledSpec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB16F;
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
        visSpec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB16F;

        colorBuffer = AssetManager::createTexture2D("VoxelVis", visSpec);
        renderTarget = createRenderTarget(visSpec.width, visSpec.height);
        renderTarget->setColorBuffer(colorBuffer, 0u);
        voxelGridVisShader = ShaderManager::createShader({ ShaderType::kVsGsPs, "VoxelVisShader", SHADER_SOURCE_PATH "voxel_vis_v.glsl", SHADER_SOURCE_PATH "voxel_vis_p.glsl", SHADER_SOURCE_PATH "voxel_vis_g.glsl" });
        coneVisComputeShader = ShaderManager::createShader({ ShaderType::kCs, "ConeVisComputeShader", nullptr, nullptr, nullptr, SHADER_SOURCE_PATH "cone_vis_c.glsl" });
        coneVisDrawShader = ShaderManager::createShader({ ShaderType::kVsGsPs, "ConeVisGraphicsShader", SHADER_SOURCE_PATH "cone_vis_v.glsl", SHADER_SOURCE_PATH "cone_vis_p.glsl", SHADER_SOURCE_PATH "cone_vis_g.glsl" });

        ITextureRenderable::Spec depthNormSpec = { };
        depthNormSpec.width = 1280 * 2;
        depthNormSpec.height = 720 * 2;
        depthNormSpec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB32F;
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

        // voxel cone tracing
        {
            m_vctx.voxelizer.init(m_sceneVoxelGrid.resolution);
            m_vctx.visualizer.init();

            {
                ITextureRenderable::Spec spec = { };
                spec.width = m_sceneVoxelGrid.resolution;
                spec.height = m_sceneVoxelGrid.resolution;
                spec.depth = m_sceneVoxelGrid.resolution;
                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGBA8;
                ITextureRenderable::Parameter parameter = { };
                parameter.minificationFilter = ITextureRenderable::Parameter::Filtering::LINEAR_MIPMAP_LINEAR;
                parameter.magnificationFilter = ITextureRenderable::Parameter::Filtering::NEAREST;
                // m_sceneVoxelGrid.normal = AssetManager::createTexture3D("VoxelGridNormal", depthSpec, parameter);
                // m_sceneVoxelGrid.emission = AssetManager::createTexture3D("VoxelGridEmission", depthSpec, parameter);

                spec.numMips = std::log2(m_sceneVoxelGrid.resolution) + 1;
                // m_sceneVoxelGrid.albedo = AssetManager::createTexture3D("VoxelGridAlbedo", depthSpec, parameter);
                // m_sceneVoxelGrid.radiance = AssetManager::createTexture3D("VoxelGridRadiance", depthSpec, parameter);

                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R32F;
                // m_sceneVoxelGrid.opacity = AssetManager::createTexture3D("VoxelGridOpacity", depthSpec, parameter);
            }

            {
                using ColorBuffers = Vctx::ColorBuffers;

                ITextureRenderable::Spec spec = { };
                spec.width = 1280;
                spec.height = 720;
                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB16F;

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

        // taa
        for (i32 i = 0; i < ARRAY_COUNT(TAAJitterVectors); ++i)
        {
#if 0
            TAAJitterVectors[i].x = (f32)std::rand() / RAND_MAX;
            TAAJitterVectors[i].y = (f32)std::rand() / RAND_MAX;
#else
            TAAJitterVectors[i] = halton23(i);
#endif
            TAAJitterVectors[i] = TAAJitterVectors[i] * 2.0f - glm::vec2(1.0f);
            reconstructionWeights[i] = glm::exp(-2.29 * (TAAJitterVectors[i].x * TAAJitterVectors[i].x + TAAJitterVectors[i].y * TAAJitterVectors[i].y));
            TAAJitterVectors[i] *= 0.5f / 1440.f;
        }

        ITextureRenderable::Spec spec = { };
        spec.type = TEX_2D;
        spec.pixelFormat = PF_RGB16F;
        spec.width = 2560;
        spec.height = 1440;
        m_rtxPingPongBuffer[0] = AssetManager::createTexture2D("RayTracingPingPong_0", spec);
        m_rtxPingPongBuffer[1] = AssetManager::createTexture2D("RayTracingPingPong_1", spec);

        glCreateBuffers(1, &indirectDrawBuffer.buffer);
        glNamedBufferStorage(indirectDrawBuffer.buffer, indirectDrawBuffer.sizeInBytes, nullptr, GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
        indirectDrawBuffer.data = glMapNamedBufferRange(indirectDrawBuffer.buffer, 0, indirectDrawBuffer.sizeInBytes, GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);

        m_instantRadiosity.initialize();
        m_manyViewGI->initialize();
        m_microRenderingGI->initialize();
    };

    void Renderer::finalize()
    {
        // imgui clean up
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void Renderer::appendToRenderingTab(const std::function<void()>& command) {
        ImGui::Begin("Cyan", nullptr);
        {
            ImGui::BeginTabBar("##Views");
            {
                if (ImGui::BeginTabItem("Rendering"))
                {
                    ImGui::BeginChild("##Rendering Settings", ImVec2(0, 0), true); 
                    {
                        command();
                        ImGui::EndChild();
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    void Renderer::drawMeshInstance(const SceneRenderable& sceneRenderable, RenderTarget* renderTarget, Viewport viewport, GfxPipelineState pipelineState, MeshInstance* meshInstance, i32 transformIndex)
    {
        Mesh* parent = meshInstance->parent;
        for (u32 i = 0; i < parent->numSubmeshes(); ++i)
        {
            IMaterial* material = meshInstance->getMaterial(i);
            if (material)
            {
                RenderTask task = { };
                task.renderTarget = renderTarget;
                task.viewport = viewport;
                task.shader = material->getMaterialShader();
                task.submesh = parent->getSubmesh(i);
                task.renderSetupLambda = [this, &sceneRenderable, transformIndex, material](RenderTarget* renderTarget, Shader* shader) {
                    material->setShaderMaterialParameters();
                    if (material->isLit())
                    {
                        for (auto light : sceneRenderable.lights)
                        {
                            light->setShaderParameters(shader);
                        }
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
        f32 len = std::max<float>(pmax.x - pmin.x, pmax.y - pmin.y);
        len = std::max<float>(len, pmax.z - pmin.z);
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
                scene->entities[i]->visit([this, voxelizer, pmin, pmax, axis, renderTarget](SceneComponent* node) {
                    if (MeshInstance* meshInstance = node->getAttachedMesh())
                    {
                        for (u32 i = 0; i < meshInstance->parent->numSubmeshes(); ++i)
                        {
                            auto sm = meshInstance->parent->getSubmesh(i);

                            RenderTask task = { };
                            task.renderTarget = renderTarget;
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

                                PBRMaterial* matl = meshInstance->getMaterial<PBRMaterial>(i);
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

        // compute pass for resolving super sampled scene opacity
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

        // upload buffer data
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
                // upload cached snapshot of scene depth/normal by copying texture data
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

    void Renderer::submitMesh(RenderTarget* renderTarget, Viewport viewport, GfxPipelineState pipelineState, Mesh* mesh, Shader* shader, const RenderSetupLambda& perMeshSetupLambda)
    {
        perMeshSetupLambda(renderTarget, shader);

        for (u32 i = 0; i < mesh->numSubmeshes(); ++i)
        {
            RenderTask task = { };
            task.renderTarget = renderTarget;
            task.viewport = viewport;
            task.pipelineState = pipelineState;
            task.shader = shader;
            task.submesh = mesh->getSubmesh(i);

            submitRenderTask(std::move(task));
        }
    }

    void Renderer::submitMaterialMesh(RenderTarget* renderTarget, Viewport viewport, GfxPipelineState pipelineState, Mesh* mesh, IMaterial* material, const RenderSetupLambda& perMeshSetupLambda)
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
                task.viewport = viewport;
                task.shader = shader;
                task.submesh = mesh->getSubmesh(i);

                submitRenderTask(std::move(task));
            }
        }
    }

    void Renderer::submitFullScreenPass(RenderTarget* renderTarget, Shader* shader, RenderSetupLambda&& renderSetupLambda)
    {
        GfxPipelineState pipelineState;
        pipelineState.depth = DepthControl::kDisable;

        submitMesh(
            renderTarget,
            { 0, 0, renderTarget->width, renderTarget->height },
            pipelineState,
            fullscreenQuad,
            shader,
            std::move(renderSetupLambda)
            );
    }

    void Renderer::submitScreenQuadPass(RenderTarget* renderTarget, Viewport viewport, Shader* shader, RenderSetupLambda&& renderSetupLambda) {
        GfxPipelineState pipelineState;
        pipelineState.depth = DepthControl::kDisable;

        submitMesh(
            renderTarget,
            viewport,
            pipelineState,
            fullscreenQuad,
            shader,
            std::move(renderSetupLambda)
            );
    }

    void Renderer::submitRenderTask(RenderTask&& task)
    {
        // set render target assuming that render target is alreay properly setup
        m_ctx->setRenderTarget(task.renderTarget);
        m_ctx->setViewport(task.viewport);

        // set shader
        m_ctx->setShader(task.shader);

        // set graphics pipeline state
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

    void Renderer::beginRender()
    {
        // reset frame allocator
        m_frameAllocator.reset();
    }

    void Renderer::render(Scene* scene, const SceneView& sceneView) {
        beginRender();
        {
            // shared render target for this frame
            glm::uvec2 renderResolution = m_windowSize;
            if (m_settings.enableAA) {
                renderResolution = m_windowSize * 2u;
            }
            std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(renderResolution.x, renderResolution.y));

            // convert Scene instance to SceneRenderable instance for rendering
            SceneRenderable sceneRenderable(scene, sceneView, m_frameAllocator);

            // shadow
            renderShadowMaps(*scene, sceneRenderable);

            // scene depth & normal pass
            ITextureRenderable::Spec depthNormalSpec = { };
            depthNormalSpec.type = TEX_2D;
            depthNormalSpec.width = renderResolution.x;
            depthNormalSpec.height = renderResolution.y;
            depthNormalSpec.pixelFormat = PF_RGB32F;
            static Texture2DRenderable* sceneDepthBuffer = new Texture2DRenderable("SceneDepthBuffer", depthNormalSpec);
            static Texture2DRenderable* sceneNormalBuffer = new Texture2DRenderable("SceneNormalBuffer", depthNormalSpec);
            renderSceneDepthNormal(sceneRenderable, renderTarget.get(), sceneDepthBuffer, sceneNormalBuffer);

            // many view global illumination
#if 0
            m_manyViewGI->run(renderTarget.get(), sceneRenderable, sceneDepthBuffer, sceneNormalBuffer);
            // ssgi
            // auto ssgi = screenSpaceRayTracing(zPrepass.depthBuffer, zPrepass.normalBuffer, renderResolution);
            // instant radiosity
            // auto radiosity = m_instantRadiosity.render(this, scene, zPrepass.depthBuffer, zPrepass.normalBuffer, renderResolution);
#endif
            m_microRenderingGI->run(sceneRenderable);

            // main scene pass
            ITextureRenderable::Spec spec = { };
            spec.width = renderTarget->width;
            spec.height = renderTarget->height;
            spec.type = TEX_2D;
            spec.pixelFormat = PF_RGB16F;
            static Texture2DRenderable* sceneColor = new Texture2DRenderable("SceneColor", spec);
#if BINDLESS_TEXTURE
            renderSceneMultiDraw(sceneRenderable, renderTarget.get(), sceneColor, { });
#else
            auto sceneColor = renderScene(sceneRenderable, sceneView, renderResolution);
#endif
            if (m_settings.enableTAA) {
                taa();
            }

            // post processing
            // todo: bloom pass is buggy! it breaks the crappy transient render resource system
            // auto bloom = bloom(sceneColorTexture);
            if (m_settings.bPostProcessing) {
                composite(sceneView.renderTexture, sceneColor, nullptr, m_windowSize);
            }
            // todo: blit offscreen color buffer into output render texture
            else {

            }
        } 
        endRender();
    }

    void Renderer::renderToScreen(Texture2DRenderable* inTexture)
    {
        Shader* fullscreenBlitShader = ShaderManager::createShader({
            ShaderSource::Type::kVsPs,
            "BlitQuadShader",
            SHADER_SOURCE_PATH "blit_v.glsl",
            SHADER_SOURCE_PATH "blit_p.glsl"
            });

        // final blit to default framebuffer
        submitFullScreenPass(
            RenderTarget::getDefaultRenderTarget(m_windowSize.x, m_windowSize.y),
            fullscreenBlitShader,
            [this, inTexture](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcTexture", inTexture);
            });
    }

    void Renderer::endRender()
    {
        m_numFrames++;
    }

    void Renderer::renderShadowMaps(const Scene& scene, const SceneRenderable& renderableScene)
    {
        // construct a scene view for all shadow casting lights 
        SceneView shadowView(scene, scene.camera->getCamera(), EntityFlag_kVisible | EntityFlag_kCastShadow, nullptr, { });
        SceneRenderable shadowScene(&scene, shadowView, m_frameAllocator);

        for (auto light : renderableScene.lights)
        {
            light->renderShadowMaps(scene, shadowScene, *this);
        }
    }

    void Renderer::renderSceneMeshOnly(SceneRenderable& sceneRenderable, RenderTarget* dstRenderTarget, Shader* shader)
    {
        sceneRenderable.submitSceneData(m_ctx);

        u32 transformIndex = 0u;
        for (auto& meshInst : sceneRenderable.meshInstances)
        {
            submitMesh(
                dstRenderTarget,
                { 0u, 0u, dstRenderTarget->width, dstRenderTarget->height },
                GfxPipelineState(), 
                meshInst->parent, 
                shader,
                [transformIndex](RenderTarget* renderTarget, Shader* shader) {
                    shader->setUniform("transformIndex", (i32)transformIndex);
                });
            transformIndex++;
        }
    }

    void Renderer::renderSceneDepthOnly(SceneRenderable& renderableScene, Texture2DRenderable* outDepthTexture)
    {
        Shader* depthOnlyShader = ShaderManager::createShader({ 
            ShaderSource::Type::kVsPs, 
            "DepthOnlyShader", 
            SHADER_SOURCE_PATH "depth_only_v.glsl", 
            SHADER_SOURCE_PATH "depth_only_p.glsl",
            });

        // create render target
        std::unique_ptr<RenderTarget> depthRenderTargetPtr(createDepthOnlyRenderTarget(outDepthTexture->width, outDepthTexture->height));
        depthRenderTargetPtr->setDepthBuffer(reinterpret_cast<DepthTexture2D*>(outDepthTexture));
        depthRenderTargetPtr->clear({ { 0u } });

        renderableScene.submitSceneData(m_ctx);

        u32 transformIndex = 0u;
        for (auto& meshInst : renderableScene.meshInstances)
        {
            submitMesh(
                depthRenderTargetPtr.get(),
                { 0u, 0u, depthRenderTargetPtr->width, depthRenderTargetPtr->height },
                GfxPipelineState(),
                meshInst->parent,
                depthOnlyShader,
                [transformIndex](RenderTarget* renderTarget, Shader* shader) {
                    shader->setUniform("transformIndex", (i32)transformIndex);
                });
            transformIndex++;
        }
    }

    void Renderer::renderSceneDepthNormal(SceneRenderable& renderableScene, RenderTarget* outRenderTarget, Texture2DRenderable* outDepthBuffer, Texture2DRenderable* outNormalBuffer) {
        renderableScene.submitSceneData(m_ctx);
        outRenderTarget->setColorBuffer(outDepthBuffer, 0);
        outRenderTarget->setColorBuffer(outNormalBuffer, 1);
        outRenderTarget->setDrawBuffers({ 0, 1 });
        outRenderTarget->clearDrawBuffer(0, glm::vec4(1.f));
        outRenderTarget->clearDrawBuffer(1, glm::vec4(0.f, 0.f, 0.f, 1.f));

        Shader* sceneDepthNormalShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "SceneDepthNormalShader", SHADER_SOURCE_PATH "scene_depth_normal_v.glsl", SHADER_SOURCE_PATH "scene_depth_normal_p.glsl" });
        u32 transformIndex = 0u;
        for (auto& meshInst : renderableScene.meshInstances)
        {
            submitMesh(
                outRenderTarget,
                {0u, 0u, outRenderTarget->width, outRenderTarget->height},
                GfxPipelineState(),
                meshInst->parent,
                sceneDepthNormalShader,
                [&transformIndex, this](RenderTarget* renderTarget, Shader* shader) {
                    shader->setUniform("transformIndex", (i32)transformIndex++);
                });
        }

        addUIRenderCommand([this, outDepthBuffer, outNormalBuffer]() {
            appendToRenderingTab(
                [this, outDepthBuffer, outNormalBuffer]() {
                    if (ImGui::CollapsingHeader("Depth Prepass")) {
                        ImGui::Image((ImTextureID)outDepthBuffer->getGpuResource(), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));
                        ImGui::Image((ImTextureID)outNormalBuffer->getGpuResource(), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));
                    }
                }
            );
        });
    }

    Renderer::SSGITextures Renderer::screenSpaceRayTracing(Texture2DRenderable* sceneDepthTexture, Texture2DRenderable* sceneNormalTexture, const glm::uvec2& renderResolution)
    {
        ITextureRenderable::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = renderResolution.x;
        spec.height = renderResolution.y;
        spec.pixelFormat = PF_RGB16F;
        ITextureRenderable::Parameter parameter = { };
        parameter.minificationFilter = FM_POINT;
        parameter.magnificationFilter = FM_POINT;
        parameter.wrap_r = WM_CLAMP;
        parameter.wrap_s = WM_CLAMP;
        parameter.wrap_t = WM_CLAMP;
        static Texture2DRenderable* ssao = new Texture2DRenderable("SceneDepthBuffer", spec, parameter);
        static Texture2DRenderable* ssbn = new Texture2DRenderable("SceneNormalBuffer", spec, parameter);
        static Texture2DRenderable* ssr = new Texture2DRenderable("SceneNormalBuffer", spec, parameter);

        static f32 kRoughness = .2f;
        static Texture2DRenderable* temporalSsaoBuffer[2] = {
            new Texture2DRenderable("TemporalSSAO_0", spec, parameter),
            new Texture2DRenderable("TemporalSSAO_1", spec, parameter),
        };
        static Texture2DRenderable* temporalSsaoCountBuffer[2] = {
            new Texture2DRenderable("TemporalSSAOSampleCount_0", spec, parameter),
            new Texture2DRenderable("TemporalSSAOSampleCount_1", spec, parameter),
        };

        u32 src = m_numFrames > 0 ? (m_numFrames - 1) % 2 : 1;
        u32 dst = m_numFrames % 2;

        std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(ssao->width, ssao->height));
        renderTarget->setColorBuffer(ssao, 0);
        renderTarget->setColorBuffer(temporalSsaoBuffer[dst], 0);
        renderTarget->setColorBuffer(ssbn, 1);
        renderTarget->setColorBuffer(ssr, 2);
        renderTarget->setColorBuffer(temporalSsaoCountBuffer[dst], 3);
        renderTarget->setDrawBuffers({ 0, 1, 2, 3 });
        renderTarget->clear({ 
            { 0, glm::vec4(0.f, 0.f, 0.f, 1.f) },
            { 1, glm::vec4(0.f, 0.f, 0.f, 1.f) },
            { 2, glm::vec4(0.f, 0.f, 0.f, 1.f) },
            { 3, glm::vec4(0.f, 0.f, 0.f, 1.f) },
        });

        Shader* ssrtShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "ScreenSpaceRayTracingShader", SHADER_SOURCE_PATH "screenspace_raytracing.glsl", SHADER_SOURCE_PATH "screenspace_raytracing.glsl" });
        submitFullScreenPass(
            renderTarget.get(),
            ssrtShader,
            [this, src, sceneDepthTexture, sceneNormalTexture](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("depthTexture", sceneDepthTexture);
                shader->setTexture("normalTexture", sceneNormalTexture);
                auto blueNoiseTexture = AssetManager::getAsset<Texture2DRenderable>("BlueNoise_1024x1024");
                shader->setTexture("blueNoiseTexture", blueNoiseTexture);
                // ssao
                shader->setUniform("numFrames", m_numFrames);
                if (m_numFrames > 0) {
                    shader->setTexture("temporalSsaoBuffer", temporalSsaoBuffer[src]);
                    shader->setTexture("temporalSsaoCountBuffer", temporalSsaoCountBuffer[src]);
                }
                // ssr
                shader->setUniform("kRoughness", kRoughness);
                auto skybox = AssetManager::getAsset<TextureCubeRenderable>("Skybox");
                shader->setTexture("reflectionProbe", skybox);
            });

        // screen space bent normal

        // screen space diffuse inter-reflection

        // screen space reflection

        addUIRenderCommand([this, dst]() {
            appendToRenderingTab([=]() {
                if (ImGui::CollapsingHeader("SSGI")) {
                    ImGui::Checkbox("Bent Normal", &m_settings.useBentNormal);
                    ImGui::SliderFloat("kRoughness", &kRoughness, 0.f, 1.f);
                    enum class Visualization
                    {
                        kAmbientOcclusion,
                        kBentNormal,
                        kReflection,
                        kCount
                    };
                    static i32 visualization = (i32)Visualization::kAmbientOcclusion;
                    const char* visualizationNames[(i32)Visualization::kCount] = { "Ambient Occlusion", "Bent Normal", "Reflection" };
                    ImVec2 imageSize(320, 180);
                    ImGui::Combo("Visualization", &visualization, visualizationNames, (i32)Visualization::kCount);
                    if (visualization == (i32)Visualization::kAmbientOcclusion) {
                        // ImGui::Image((ImTextureID)(ssao->getGpuResource()), ImVec2(480, 270), ImVec2(0, 1), ImVec2(1, 0));
                        ImGui::Image((ImTextureID)(temporalSsaoBuffer[dst]->getGpuResource()), imageSize, ImVec2(0, 1), ImVec2(1, 0));
                    }
                    if (visualization == (i32)Visualization::kBentNormal) {
                        ImGui::Image((ImTextureID)(ssbn->getGpuResource()), imageSize, ImVec2(0, 1), ImVec2(1, 0));
                    }
                    if (visualization == (i32)Visualization::kReflection) {
                        ImGui::Image((ImTextureID)(ssr->getGpuResource()), imageSize, ImVec2(0, 1), ImVec2(1, 0));
                    }
                }
            });
        });

        return { temporalSsaoBuffer[dst], ssbn };
    }

#if 0
    void Renderer::gpuRayTracing(RayTracingScene& rtxScene, RenderTexture2D* outputBuffer, RenderTexture2D* sceneDepthBuffer, RenderTexture2D* sceneNormalBuffer)
    {
#define POSITION_BUFFER_BINDING 20
#define NORMAL_BUFFER_BINDING 21
#define MATERIAL_BUFFER_BINDING 22
        ITextureRenderable::Spec spec = { };
        spec.width = 2560;
        spec.height = 1440;
        spec.type = TEX_2D;
        spec.pixelFormat = PF_RGB16F;

        ITextureRenderable::Parameter parameter = { };
        parameter.minificationFilter = FM_POINT;
        parameter.magnificationFilter = FM_POINT;
        parameter.wrap_r = WM_CLAMP;
        parameter.wrap_s = WM_CLAMP;
        parameter.wrap_t = WM_CLAMP;

        // reservoir buffers
        static Texture2DRenderable* temporalReservoirPosition[2] = {
            AssetManager::createTexture2D("TemporalReservoirPosition_0", spec, parameter),
            AssetManager::createTexture2D("TemporalReservoirPosition_1", spec, parameter)
        };

        static Texture2DRenderable* temporalReservoirNormal[2] = {
            AssetManager::createTexture2D("TemporalReservoirNormal_0", spec, parameter),
            AssetManager::createTexture2D("TemporalReservoirNormal_1", spec, parameter)
        };

        static Texture2DRenderable* temporalReservoirRadiance[2] = {
            AssetManager::createTexture2D("TemporalReservoirRadiance_0", spec, parameter),
            AssetManager::createTexture2D("TemporalReservoirRadiance_1", spec, parameter)
        };

        static Texture2DRenderable* temporalReservoirSamplePosition[2] = {
            AssetManager::createTexture2D("TemporalReservoirSamplePosition_0", spec, parameter),
            AssetManager::createTexture2D("TemporalReservoirSamplePosition_1", spec, parameter)
        };

        static Texture2DRenderable* temporalReservoirSampleNormal[2] = {
            AssetManager::createTexture2D("TemporalReservoirSampleNormal_0", spec, parameter),
            AssetManager::createTexture2D("TemporalReservoirSampleNormal_1", spec, parameter)
        };

        static Texture2DRenderable* temporalReservoirWM[2] = {
            AssetManager::createTexture2D("TemporalReservoirWM_0", spec, parameter),
            AssetManager::createTexture2D("TemporalReservoirWM_1", spec, parameter),
        };

        static Texture2DRenderable* dbgOutput[2] = {
            AssetManager::createTexture2D("DbgOutputTexture_0", spec, parameter),
            AssetManager::createTexture2D("DbgOutputTexture_1", spec, parameter),
        };

        u32 src = (m_numFrames - 1) % 2;
        u32 dst = m_numFrames % 2;

        rtxScene.positionSsbo.bind(POSITION_BUFFER_BINDING);
        rtxScene.normalSsbo.bind(NORMAL_BUFFER_BINDING);
        rtxScene.materialSsbo.bind(MATERIAL_BUFFER_BINDING);
        m_renderQueue.addPass(
            "RayTracingPass",
            [this, outputBuffer, sceneDepthBuffer, sceneNormalBuffer](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addOutput(outputBuffer);
                pass->addInput(sceneDepthBuffer);
                pass->addInput(sceneNormalBuffer);
            },
            [this, outputBuffer, &rtxScene, sceneDepthBuffer, sceneNormalBuffer, src, dst]() { // renderSetupLambda
                Shader* raytracingShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "RayTracingShader", SHADER_SOURCE_PATH "raytracing_v.glsl", SHADER_SOURCE_PATH "raytracing_p.glsl" });
                auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(outputBuffer->spec.width, outputBuffer->spec.height));
                renderTarget->setColorBuffer(outputBuffer->getTextureResource(), 0);
                renderTarget->setColorBuffer(temporalReservoirPosition[dst], 1);
                renderTarget->setColorBuffer(temporalReservoirNormal[dst], 2);
                renderTarget->setColorBuffer(temporalReservoirRadiance[dst], 3);
                renderTarget->setColorBuffer(temporalReservoirSamplePosition[dst], 4);
                renderTarget->setColorBuffer(temporalReservoirSampleNormal[dst], 5);
                renderTarget->setColorBuffer(temporalReservoirWM[dst], 6);
                renderTarget->setColorBuffer(dbgOutput[dst], 7);

                submitFullScreenPass(
                    renderTarget.get(),
                    { { 0 },  { 1 },  { 2 },  { 3 }, { 4 }, { 5 }, { 6 }, { 7 } },
                    raytracingShader,
                    [&rtxScene, this,  sceneDepthBuffer, sceneNormalBuffer, src](RenderTarget* renderTarget, Shader* shader) {
                        shader->setUniform("numTriangles", (i32)rtxScene.surfaces.numTriangles());
                        shader->setUniform("outputSize", glm::vec2(renderTarget->width, renderTarget->height));
                        shader->setUniform("camera.position", rtxScene.camera.position);
                        shader->setUniform("camera.lookAt", rtxScene.camera.lookAt);
                        shader->setUniform("camera.forward", rtxScene.camera.forward());
                        shader->setUniform("camera.up", rtxScene.camera.up());
                        shader->setUniform("camera.right", rtxScene.camera.right());
                        shader->setUniform("camera.n", rtxScene.camera.n);
                        shader->setUniform("camera.f", rtxScene.camera.f);
                        shader->setUniform("camera.fov", rtxScene.camera.fov);
                        shader->setUniform("camera.aspectRatio", rtxScene.camera.aspectRatio);
                        shader->setUniform("numFrames", m_numFrames);
                        auto blueNoiseTexture = AssetManager::getAsset<Texture2DRenderable>("BlueNoise_1024x1024");
                        shader->setTexture("blueNoiseTexture", blueNoiseTexture);
                        if (m_numFrames > 0)
                        {
                            shader->setTexture("temporalReservoirPosition", temporalReservoirPosition[src]);
                            shader->setTexture("temporalReservoirNormal", temporalReservoirNormal[src]);
                            shader->setTexture("temporalReservoirRadiance", temporalReservoirRadiance[src]);
                            shader->setTexture("temporalReservoirSamplePosition", temporalReservoirSamplePosition[src]);
                            shader->setTexture("temporalReservoirSampleNormal", temporalReservoirSampleNormal[src]);
                            shader->setTexture("temporalReservoirWM", temporalReservoirWM[src]);
                            shader->setTexture("debugOutput", dbgOutput[src]);
                        }
                        shader->setTexture("sceneDepthBuffer", sceneDepthBuffer->getTextureResource());
                        shader->setTexture("sceneNormalBuffer", sceneNormalBuffer->getTextureResource());
                    });
            }
        );

        addUIRenderCommand([dst]() {
            ImGui::Begin("Gpu Ray Tracing");
            enum class Visualization
            {
                kReservoirPosition,
                kReservoirRadiance,
                kReservoirSamplePosition,
                kReservoirSampleNormal,
                kReservoirWM,
                kDebugOutput,
                kCount
            };
            static i32 visualization = (i32)Visualization::kReservoirRadiance;
            const char* visualizationNames[(i32)Visualization::kCount] = { 
                "ReservoirPosition", "ReservoirRadiance", "ReservoirSamplePosition", "ReservoirSampleNormal", "ReservoirWM", "DebugOutput"};
            ImGui::Combo("Visualization", &visualization, visualizationNames, (i32)Visualization::kCount);
            switch ((Visualization)visualization) {
                case Visualization::kReservoirPosition:
                    ImGui::Image((ImTextureID)(temporalReservoirPosition[dst]->getGpuResource()), ImVec2(480, 270), ImVec2(0, 1), ImVec2(1, 0));
                    break;
                case Visualization::kReservoirRadiance:
                    ImGui::Image((ImTextureID)(temporalReservoirRadiance[dst]->getGpuResource()), ImVec2(480, 270), ImVec2(0, 1), ImVec2(1, 0));
                    break;
                case Visualization::kReservoirSamplePosition:
                    ImGui::Image((ImTextureID)(temporalReservoirSamplePosition[dst]->getGpuResource()), ImVec2(480, 270), ImVec2(0, 1), ImVec2(1, 0));
                    break;
                case Visualization::kReservoirSampleNormal:
                    ImGui::Image((ImTextureID)(temporalReservoirSampleNormal[dst]->getGpuResource()), ImVec2(480, 270), ImVec2(0, 1), ImVec2(1, 0));
                    break;
                case Visualization::kReservoirWM:
                    ImGui::Image((ImTextureID)(temporalReservoirWM[dst]->getGpuResource()), ImVec2(480, 270), ImVec2(0, 1), ImVec2(1, 0));
                    break;
                case Visualization::kDebugOutput:
                    ImGui::Image((ImTextureID)(dbgOutput[dst]->getGpuResource()), ImVec2(480, 270), ImVec2(0, 1), ImVec2(1, 0));
                    break;
                default:
                    break;
            }
            ImGui::End();
        });
    }
#endif

    Texture2DRenderable* Renderer::renderScene(SceneRenderable& renderableScene, const SceneView& sceneView, const glm::uvec2& outputResolution)
    {
        ITextureRenderable::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = outputResolution.x;
        spec.height = outputResolution.y;
        spec.pixelFormat = PF_RGB16F;
        static Texture2DRenderable* outSceneColor = new Texture2DRenderable("SceneColor", spec);

        std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(outSceneColor->width, outSceneColor->height));
        renderTarget->setColorBuffer(outSceneColor, 0);
        renderTarget->clear({ { 0 } });

        renderableScene.submitSceneData(m_ctx);

        // render mesh instances
        u32 transformIndex = 0u;
        for (auto meshInst : renderableScene.meshInstances)
        {
            drawMeshInstance(
                renderableScene,
                renderTarget.get(),
                { 0u, 0u, outSceneColor->width, outSceneColor->height },
                GfxPipelineState(), 
                meshInst,
                transformIndex++
            );
        }

        // render skybox
        if (renderableScene.skybox) { 
            renderableScene.skybox->render(renderTarget.get());
        }

        return outSceneColor;
    }

    void Renderer::submitSceneMultiDrawIndirect(const SceneRenderable& renderableScene) {
        struct IndirectDrawArrayCommand
        {
            u32  count;
            u32  instanceCount;
            u32  first;
            i32  baseInstance;
        };
        // build indirect draw commands
        auto ptr = reinterpret_cast<IndirectDrawArrayCommand*>(indirectDrawBuffer.data);
        for (u32 draw = 0; draw < renderableScene.drawCalls->getNumElements() - 1; ++draw)
        {
            IndirectDrawArrayCommand& command = ptr[draw];
            command.first = 0;
            u32 instance = (*renderableScene.drawCalls)[draw];
            u32 submesh = (*renderableScene.instances)[instance].submesh;
            command.count = SceneRenderable::packedGeometry->submeshes[submesh].numIndices;
            command.instanceCount = (*renderableScene.drawCalls)[draw + 1] - (*renderableScene.drawCalls)[draw];
            command.baseInstance = 0;
        }

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectDrawBuffer.buffer);
        // dispatch multi draw indirect
        // one sub-drawcall per instance
        u32 drawCount = (u32)renderableScene.drawCalls->getNumElements() - 1;
        glMultiDrawArraysIndirect(GL_TRIANGLES, 0, drawCount, 0);
    }

    void Renderer::renderSceneMultiDraw(SceneRenderable& sceneRenderable, RenderTarget* outRenderTarget, Texture2DRenderable* outSceneColor, const SSGITextures& SSGIOutput) {
        Shader* scenePassShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "ScenePassShader", SHADER_SOURCE_PATH "scene_pass_v.glsl", SHADER_SOURCE_PATH "scene_pass_p.glsl" });
        sceneRenderable.submitSceneData(m_ctx);

        m_ctx->setShader(scenePassShader);
        outRenderTarget->setColorBuffer(outSceneColor, 0);
        outRenderTarget->setDrawBuffers({ 0 });
        outRenderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f));
        m_ctx->setRenderTarget(outRenderTarget);
        m_ctx->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });
        m_ctx->setDepthControl(DepthControl::kEnable);

        // setup ssao
        if (SSGIOutput.ao) {
            auto ssao = SSGIOutput.ao->glHandle;
            if (glIsTextureHandleResidentARB(ssao) == GL_FALSE)
            {
                glMakeTextureHandleResidentARB(ssao);
            }
            scenePassShader->setUniform("SSAO", ssao);
            scenePassShader->setUniform("useSSAO", 1.f);
        }
        else {
            scenePassShader->setUniform("useSSAO", 0.f);
        }

        // setup ssbn
        if (SSGIOutput.bentNormal) {
            if (m_settings.useBentNormal) {
                auto ssbn = SSGIOutput.bentNormal->glHandle;
                scenePassShader->setUniform("useBentNormal", 1.f);
                if (glIsTextureHandleResidentARB(ssbn) == GL_FALSE)
                {
                    glMakeTextureHandleResidentARB(ssbn);
                }
                scenePassShader->setUniform("SSBN", ssbn);
            }
            else {
                scenePassShader->setUniform("useBentNormal", 0.f);
            }
        }

        // todo: refactor the following
        // sky light
        auto BRDFLookupTexture = ReflectionProbe::getBRDFLookupTexture()->glHandle;
        /* note
        * seamless cubemap doesn't work with bindless textures that's accessed using a texture handle,
        * so falling back to normal way of binding textures here.
        */
        if (glIsTextureHandleResidentARB(BRDFLookupTexture) == GL_FALSE)
        {
            glMakeTextureHandleResidentARB(BRDFLookupTexture);
        }
        scenePassShader->setTexture("sceneLights.skyLight.irradiance", sceneRenderable.irradianceProbe->m_convolvedIrradianceTexture);
        scenePassShader->setTexture("sceneLights.skyLight.reflection", sceneRenderable.reflectionProbe->m_convolvedReflectionTexture);
        scenePassShader->setUniform("sceneLights.BRDFLookupTexture", BRDFLookupTexture);

        for (auto light : sceneRenderable.lights)
        {
            light->setShaderParameters(scenePassShader);
        }

        submitSceneMultiDrawIndirect(sceneRenderable);

        // render skybox
        if (sceneRenderable.skybox) {
            sceneRenderable.skybox->render(outRenderTarget);
        }

        // m_instantRadiosity.visualizeVPLs(this, renderTarget.get(), scene);

        addUIRenderCommand([this]() {
            appendToRenderingTab([this]() {
                ImGui::Checkbox("Fullscreen", &bFullscreenRadiosity);
                });
            });
    }

#if 0
    Texture2DRenderable* Renderer::renderSceneMultiDraw(SceneRenderable& sceneRenderable, const SceneView& sceneView, const glm::uvec2& outputResolution, const SSGITextures& SSGIOutput) {
        ITextureRenderable::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = outputResolution.x;
        spec.height = outputResolution.y;
        spec.pixelFormat = PF_RGB16F;
        static Texture2DRenderable* outSceneColor = new Texture2DRenderable("SceneColor", spec);

        Shader* scenePassShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "ScenePassShader", SHADER_SOURCE_PATH "scene_pass_v.glsl", SHADER_SOURCE_PATH "scene_pass_p.glsl" });
        std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(outSceneColor->width, outSceneColor->height));
        renderTarget->setColorBuffer(outSceneColor, 0);
        renderTarget->clear({ { 0 } });

        sceneRenderable.submitSceneData(m_ctx);

        m_ctx->setShader(scenePassShader);
        m_ctx->setRenderTarget(renderTarget.get(), { { 0 } });
        m_ctx->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
        m_ctx->setDepthControl(DepthControl::kEnable);

        // setup ssao
        if (SSGIOutput.ao) {
            auto ssao = SSGIOutput.ao->glHandle;
            if (glIsTextureHandleResidentARB(ssao) == GL_FALSE)
            {
                glMakeTextureHandleResidentARB(ssao);
            }
            scenePassShader->setUniform("SSAO", ssao);
            scenePassShader->setUniform("useSSAO", 1.f);
        } else {
            scenePassShader->setUniform("useSSAO", 0.f);
        }

        // setup ssbn
        if (SSGIOutput.bentNormal) {
            if (m_settings.useBentNormal) {
                auto ssbn = SSGIOutput.bentNormal->glHandle;
                scenePassShader->setUniform("useBentNormal", 1.f);
                if (glIsTextureHandleResidentARB(ssbn) == GL_FALSE)
                {
                    glMakeTextureHandleResidentARB(ssbn);
                }
                scenePassShader->setUniform("SSBN", ssbn);
            } else {
                scenePassShader->setUniform("useBentNormal", 0.f);
            }
        }

        // todo: refactor the following
        // sky light
        auto BRDFLookupTexture = ReflectionProbe::getBRDFLookupTexture()->glHandle;
        /* note
        * seamless cubemap doesn't work with bindless textures that's accessed using a texture handle,
        * so falling back to normal way of binding textures here.
        */
        if (glIsTextureHandleResidentARB(BRDFLookupTexture) == GL_FALSE)
        {
            glMakeTextureHandleResidentARB(BRDFLookupTexture);
        }
        scenePassShader->setTexture("sceneLights.skyLight.irradiance", sceneRenderable.irradianceProbe->m_convolvedIrradianceTexture);
        scenePassShader->setTexture("sceneLights.skyLight.reflection", sceneRenderable.reflectionProbe->m_convolvedReflectionTexture);
        scenePassShader->setUniform("sceneLights.BRDFLookupTexture", BRDFLookupTexture);

        for (auto light : sceneRenderable.lights)
        {
            light->setShaderParameters(scenePassShader);
        }

        submitSceneMultiDrawIndirect(sceneRenderable);

        // render skybox
        if (sceneRenderable.skybox) {
            sceneRenderable.skybox->render(renderTarget.get());
        }

        // m_instantRadiosity.visualizeVPLs(this, renderTarget.get(), scene);

#if 0
        // debug visualize raster cube positions
        {
            struct DebugCube {
                glm::mat4 transform;
                glm::vec4 color;
            };
            static ShaderStorageBuffer<DynamicSsboStruct<DebugCube>> debugCubes;

            struct DebugLine {
                glm::mat4 transform;
                glm::vec4 color;
            };
            static ShaderStorageBuffer<DynamicSsboStruct<DebugLine>> debugLines;

            // debugCubes.data.array.clear();
            debugCubes.ssboStruct.dynamicArray.resize(maxNumRasterCubes);
            // debugLines.data.array.clear();
            debugLines.ssboStruct.dynamicArray.resize(maxNumRasterCubes * 2);
            for (i32 i = 0; i < maxNumRasterCubes; ++i) {
                /* note - @min:
                * if the raster cube at i is not filled, then hemicubes[i].position should be vec4(0.f, 0.f, 0.f, 0.f);
                * otherwise the w component will be 1.f, thus using the w component here to help "nullify" rendering some empty raster cubes
                */
                glm::mat4 transform = glm::translate(glm::mat4(rasterCubes[i].position.w), vec4ToVec3(rasterCubes[i].position));
                transform = glm::scale(transform, glm::vec3(0.05));
                glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
                if (rasterCubes[i].normal.y > 0.99) {
                    worldUp = glm::vec3(0.f, 0.f, 1.f);
                }
                glm::vec3 forward = vec4ToVec3(rasterCubes[i].normal);
                glm::vec3 right = glm::cross(forward, worldUp);
                glm::vec3 up = glm::cross(right, forward);
                glm::mat4 localFrame = {
                    glm::vec4(right, 0.f),
                    glm::vec4(forward, 0.f),
                    glm::vec4(up, 0.f),
                    glm::vec4(0.f, 0.f, 0.f, 1.f),
                };
                transform *= localFrame;
                debugCubes[i].transform = transform;
                // debugCubes[i].color = glm::vec4(1.f, 0.8f, 0.5f, 1.f);
                glm::vec3 normal = vec4ToVec3(rasterCubes[i].normal);
                debugCubes[i].color = glm::vec4(normal * .5f + .5f, 1.f);

                // debug line
                glm::vec3 v0 = vec4ToVec3(rasterCubes[i].position);
                glm::vec3 v1 = v0 + vec4ToVec3(rasterCubes[i].normal) * 0.1f;
                glm::mat4 t0 = glm::translate(glm::mat4(1.f), v0);
                glm::mat4 t1 = glm::translate(glm::mat4(1.f), v1);
                debugLines[i * 2 + 0].transform = t0;
                debugLines[i * 2 + 0].color = glm::vec4(0.f, 0.f, 1.f, 1.f);
                debugLines[i * 2 + 1].transform = t1;
                debugLines[i * 2 + 1].color = glm::vec4(0.f, 0.f, 1.f, 1.f);
            }
            auto cube = AssetManager::getAsset<Mesh>("UnitCubeMesh");
            Shader* debugDrawCubeShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawShader", SHADER_SOURCE_PATH "debug_draw_v.glsl", SHADER_SOURCE_PATH "debug_draw_p.glsl" });
            m_ctx->setShader(debugDrawCubeShader);
            debugCubes.update();
            debugCubes.bind(53);
            m_ctx->setVertexArray(cube->getSubmesh(0)->getVertexArray());

            glDisable(GL_CULL_FACE);
            glDrawArraysInstanced(GL_TRIANGLES, 0, cube->getSubmesh(0)->numVertices(), maxNumRasterCubes);
            glEnable(GL_CULL_FACE);

            Shader* debugDrawLineShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawLineShader", SHADER_SOURCE_PATH "debug_draw_line_v.glsl", SHADER_SOURCE_PATH "debug_draw_p.glsl" });
            m_ctx->setShader(debugDrawLineShader);
            debugLines.update();
            debugLines.bind(53);
            glDrawArraysInstanced(GL_LINES, 0, debugLines.ssboStruct.dynamicArray.size(), maxNumRasterCubes);
        }
#endif
        struct DebugCube {
            glm::mat4 transform;
            glm::vec4 color;
        };
        static ShaderStorageBuffer<DynamicSsboStruct<DebugCube>> debugCubes;

        struct DebugLine {
            glm::mat4 transform;
            glm::vec4 color;
        };
        static ShaderStorageBuffer<DynamicSsboStruct<DebugLine>> debugLines;

        i32 numCubes = 1;
        debugCubes.ssboStruct.dynamicArray.resize(numCubes);
        // debugLines.data.array.clear();
        debugLines.ssboStruct.dynamicArray.resize(numCubes * 2 * 3);
        glm::mat4 transform = glm::translate(glm::mat4(debugRadianceCube.position.w), vec4ToVec3(debugRadianceCube.position));
        glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
        glm::vec3 up = debugRadianceCube.normal;
        glm::vec3 forward;
        glm::vec3 right;
        if (abs(up.y) > 0.98) {
            worldUp = glm::vec3(0.f, 0.f, -1.f);
        }
        right = glm::cross(worldUp, up);
        forward = glm::cross(up, right);
        glm::mat4 tangentFrame = {
            glm::vec4(right, 0.f),
            glm::vec4(up, 0.f),
            glm::vec4(-forward, 0.f),
            glm::vec4(0.f, 0.f, 0.f, 1.f),
        };
        transform = transform * tangentFrame;
        transform = glm::scale(transform, glm::vec3(0.1));
        debugCubes[0].transform = transform;
        // debugCubes[i].color = glm::vec4(1.f, 0.8f, 0.5f, 1.f);
        glm::vec3 normal = vec4ToVec3(debugRadianceCube.normal);
        debugCubes[0].color = glm::vec4(normal * .5f + .5f, 1.f);

        // todo: visualize debug tangent frame of the radiance cube
        {
            // right
            glm::vec3 v0 = vec4ToVec3(debugRadianceCube.position);
            glm::vec3 v1 = v0 + debugTangentFrame[0] * 0.2f;
            glm::mat4 t0 = glm::translate(glm::mat4(1.f), v0);
            glm::mat4 t1 = glm::translate(glm::mat4(1.f), v1);
            debugLines[1 * 2 + 0].transform = t0;
            debugLines[1 * 2 + 0].color = glm::vec4(1.f, 0.f, 0.f, 1.f);
            debugLines[1 * 2 + 1].transform = t1;
            debugLines[1 * 2 + 1].color = glm::vec4(1.f, 0.f, 0.f, 1.f);
        }
        {
            // forward
            glm::vec3 v0 = vec4ToVec3(debugRadianceCube.position);
            glm::vec3 v1 = v0 + debugTangentFrame[1] * 0.2f;
            glm::mat4 t0 = glm::translate(glm::mat4(1.f), v0);
            glm::mat4 t1 = glm::translate(glm::mat4(1.f), v1);
            debugLines[0 * 2 + 0].transform = t0;
            debugLines[0 * 2 + 0].color = glm::vec4(0.f, 1.f, 0.f, 1.f);
            debugLines[0 * 2 + 1].transform = t1;
            debugLines[0 * 2 + 1].color = glm::vec4(0.f, 1.f, 0.f, 1.f);
        }
        {
            // up
            glm::vec3 v0 = vec4ToVec3(debugRadianceCube.position);
            glm::vec3 v1 = v0 + debugTangentFrame[2] * 0.2f;
            glm::mat4 t0 = glm::translate(glm::mat4(1.f), v0);
            glm::mat4 t1 = glm::translate(glm::mat4(1.f), v1);
            debugLines[2 * 2 + 0].transform = t0;
            debugLines[2 * 2 + 0].color = glm::vec4(0.f, 0.f, 1.f, 1.f);
            debugLines[2 * 2 + 1].transform = t1;
            debugLines[2 * 2 + 1].color = glm::vec4(0.f, 0.f, 1.f, 1.f);
        }

        auto cube = AssetManager::getAsset<Mesh>("UnitCubeMesh");
        Shader* debugDrawRadianceCubeShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawRadianceCubeShader", SHADER_SOURCE_PATH "debug_draw_radiance_cube_v.glsl", SHADER_SOURCE_PATH "debug_draw_radiance_cube_p.glsl" });
        m_ctx->setShader(debugDrawRadianceCubeShader);
        glBindTextureUnit(129, radianceCubemaps[maxNumRasterCubes - 1]);
        debugDrawRadianceCubeShader->setUniform("debugRadianceCube", 129);
        debugCubes.update();
        debugCubes.bind(53);
        m_ctx->setVertexArray(cube->getSubmesh(0)->getVertexArray());

        glDisable(GL_CULL_FACE);
        glDrawArraysInstanced(GL_TRIANGLES, 0, cube->getSubmesh(0)->numVertices(), 1);
        glEnable(GL_CULL_FACE);

        Shader* debugDrawLineShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawLineShader", SHADER_SOURCE_PATH "debug_draw_line_v.glsl", SHADER_SOURCE_PATH "debug_draw_line_p.glsl" });
        m_ctx->setShader(debugDrawLineShader);
        debugLines.update();
        debugLines.bind(53);
        glDrawArraysInstanced(GL_LINES, 0, debugLines.ssboStruct.dynamicArray.size(), 3);

        addUIRenderCommand([this](){
            appendToRenderingTab([this]() {
                    ImGui::Checkbox("Fullscreen", &bFullscreenRadiosity);
                }); 
        });
        return outSceneColor;
    }
#endif

    /*
    Texture2DRenderable* Renderer::downsample(Texture2DRenderable* inTexture) {
        Shader* downsampleShader = ShaderManager::createShader({ ShaderType::kVsPs, "BloomDownsampleShader", SHADER_SOURCE_PATH "bloom_downsample_v.glsl", SHADER_SOURCE_PATH "bloom_downsample_p.glsl" });
        ITextureRenderable::Spec depthSpec = inTexture->getTextureSpec();
        depthSpec.width = max(1, depthSpec.width / 2);
        depthSpec.height = max(1, depthSpec.height / 2);
        auto outTexture = new
        std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(depthSpec.width, depthSpec.height));
        renderTarget->setColorBuffer(, 0);

        submitFullScreenPass(
            renderTarget.get(),
            downsampleShader, 
            [inTexture](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcImage", inTexture);
            });
    }
    */

#if 0
    Renderer::DownsampleChain Renderer::downsample(RenderTexture2D* inTexture, u32 inNumStages)
    {
        u32 numStages = Min(glm::log2(inTexture->spec.width), inNumStages);
        //todo: using a vector here is an obvious overkill
        std::vector<RenderTexture2D*> downsampleChain((u64)numStages + 1);

        downsampleChain[0] = inTexture;
        for (u32 i = 1; i <= numStages; ++i)
        {
            u32 input = i - 1;
            u32 output = i;

            ITextureRenderable::Spec spec = downsampleChain[input]->spec;
            spec.width /= 2;
            spec.height /= 2;
            downsampleChain[output] = m_renderQueue.createTexture2D("DownsamplePass", spec);

            m_renderQueue.addPass(
                "DownsamplePass__",
                [downsampleChain, input, output](RenderQueue& renderQueue, RenderPass* pass) {
                    pass->addInput(downsampleChain[input]);
                    pass->addInput(downsampleChain[output]);
                    pass->addOutput(downsampleChain[output]);
                },
                [this, downsampleChain, input, output]() {
                    Shader* downsampleShader = ShaderManager::createShader({ ShaderType::kVsPs, "BloomDownsampleShader", SHADER_SOURCE_PATH "bloom_downsample_v.glsl", SHADER_SOURCE_PATH "bloom_downsample_p.glsl" });
                    std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(downsampleChain[output]->spec.width, downsampleChain[output]->spec.height));
                    renderTarget->setColorBuffer(downsampleChain[output]->getTextureResource(), 0);

                    submitFullScreenPass(
                        renderTarget.get(),
                        { { 0 } },
                        downsampleShader, 
                        [downsampleChain, input](RenderTarget* renderTarget, Shader* shader) {
                            shader->setTexture("srcImage", downsampleChain[input]->getTextureResource());
                        });
                }
            );
        }
        return { downsampleChain, numStages + 1 };
    }

    RenderTexture2D* Renderer::bloom(RenderTexture2D* inTexture)
    {
        const u32 numDownsamplePasses = 6u;

        // bloom setup pass
        struct BloomSetupPassData
        {
            RenderTexture2D* input = nullptr;
            RenderTexture2D* output = nullptr;
        } bloomSetupPassData;
        bloomSetupPassData.input = inTexture;
        ITextureRenderable::Spec spec = inTexture->spec;
        spec.width /= 2;
        spec.height /= 2;
        bloomSetupPassData.output = m_renderQueue.createTexture2D("BloomSetupOutput", spec);
        std::shared_ptr<RenderTarget> renderTarget = std::shared_ptr<RenderTarget>(createRenderTarget(bloomSetupPassData.output->spec.width, bloomSetupPassData.output->spec.height));

        m_renderQueue.addPass(
            /*passName=*/"BloomSetupPass",
            [bloomSetupPassData](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addInput(bloomSetupPassData.input);
                pass->addInput(bloomSetupPassData.output);
                pass->addOutput(bloomSetupPassData.output);
            },
            [this, bloomSetupPassData, renderTarget]() {
                Shader* bloomSetupShader = ShaderManager::createShader({ ShaderType::kVsPs, "BloomSetupShader", SHADER_SOURCE_PATH "bloom_setup_v.glsl", SHADER_SOURCE_PATH "bloom_setup_p.glsl" });
                // std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(bloomSetupPassData.output->depthSpec.width, bloomSetupPassData.output->depthSpec.height));
                renderTarget->setColorBuffer(bloomSetupPassData.output->getTextureResource(), 0);

                submitFullScreenPass(
                    renderTarget.get(),
                    { { 0u } },
                    bloomSetupShader, 
                    [this, bloomSetupPassData](RenderTarget* renderTarget, Shader* shader) {
                        shader->setTexture("srcTexture", bloomSetupPassData.input->getTextureResource());
                    });
            }
        );

        // downsample
        DownsampleChain downsampleChain = downsample(bloomSetupPassData.output, numDownsamplePasses);

        const u32 kMinGaussianKernelRadius = 2u;
        std::vector<RenderTexture2D*> upscaleChain(downsampleChain.stages.size());
        upscaleChain[0] = downsampleChain.stages[downsampleChain.stages.size() - 1];
        for (u32 i = downsampleChain.stages.size() - 1, input = 0; i > 0; --i, ++input)
        {
            u32 output = input + 1;
            u32 blend = i - 1;

            // Gaussian blur pass
            downsampleChain.stages[blend] = gaussianBlur(downsampleChain.stages[blend], i, (f32)i * .5f);

            // upscale & blend pass
            ITextureRenderable::Spec spec = upscaleChain[input]->spec;
            spec.width = downsampleChain.stages[blend]->spec.width;
            spec.height = downsampleChain.stages[blend]->spec.height;
            upscaleChain[output] = m_renderQueue.createTexture2D("Upscaled", spec);
            m_renderQueue.addPass(
                "Upscale",
                [downsampleChain, &upscaleChain, input, blend, output](RenderQueue& renderQueue, RenderPass* pass) {
                    pass->addInput(downsampleChain.stages[blend]);
                    pass->addInput(upscaleChain[input]);
                    pass->addInput(upscaleChain[output]);
                    pass->addOutput(upscaleChain[output]);
                },
                [this, downsampleChain, blend, upscaleChain, input, output]() {
                    Shader* upscaleShader = ShaderManager::createShader({ ShaderType::kVsPs, "BloomUpscaleShader", SHADER_SOURCE_PATH "bloom_upscale_v.glsl", SHADER_SOURCE_PATH "bloom_upscale_p.glsl" });
                    auto upscaledOutput = upscaleChain[output];
                    std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(upscaledOutput->spec.width, upscaledOutput->spec.height));
                    renderTarget->setColorBuffer(upscaleChain[output]->getTextureResource(), 0);

                    submitFullScreenPass(
                        renderTarget.get(),
                        { { 0u } },
                        upscaleShader,
                        [this, downsampleChain, upscaleChain, input, blend](RenderTarget* renderTarget, Shader* shader) {
                            shader->setTexture("srcTexture", upscaleChain[input]->getTextureResource())
                                .setTexture("blendTexture", downsampleChain.stages[blend]->getTextureResource());
                        });
                }
            );
        }

        return upscaleChain[upscaleChain.size() - 1];
    }
#endif

    void Renderer::localToneMapping()
    {
        // step 0: build 3 fusion candidates using 3 different exposure settings
        // step 1: build lightness map of each fusion candidate
        // step 2: build fusion weight map and a according Gaussian pyramid
        // step 3: build Laplacian pyramid for each fusion candidate
    }

    void Renderer::taa()
    {
        if (!m_TAAPingPongRenderTarget[0])
        {
            m_TAAPingPongRenderTarget[0] = createRenderTarget(m_sceneColorTextureSSAA->width, m_sceneColorTextureSSAA->height);
            m_TAAPingPongRenderTarget[1] = createRenderTarget(m_sceneColorTextureSSAA->width, m_sceneColorTextureSSAA->height);
            ITextureRenderable::Spec depthSpec = m_sceneColorTextureSSAA->getTextureSpec();
            m_TAAPingPongTextures[0] = AssetManager::createTexture2D("TAAPingTexture", depthSpec);
            m_TAAPingPongTextures[1] = AssetManager::createTexture2D("TAAPongTexture", depthSpec);
            m_TAAPingPongRenderTarget[0]->setColorBuffer(m_TAAPingPongTextures[0], 0);
            m_TAAPingPongRenderTarget[1]->setColorBuffer(m_TAAPingPongTextures[1], 0);
        }

        Shader* TAAShader = ShaderManager::createShader({ ShaderType::kVsPs, "TAAShader", SHADER_SOURCE_PATH "taa_v.glsl", SHADER_SOURCE_PATH "taa_p.glsl" });
        u32 src = Max((m_numFrames - 1), 0u) % 2;
        u32 dst = m_numFrames % 2;
        submitFullScreenPass(
            m_TAAPingPongRenderTarget[dst],
            TAAShader,
            [this, src](RenderTarget* renderTarget, Shader* shader) {
                shader->setUniform("numFrames", m_numFrames);
                shader->setUniform("sampleWeight", reconstructionWeights[m_numFrames % ARRAY_COUNT(TAAJitterVectors)]);
                shader->setTexture("currentColorTexture", m_sceneColorTextureSSAA);
                shader->setTexture("accumulatedColorTexture", m_TAAPingPongTextures[src]);
            }
        );
        m_TAAOutput = m_TAAPingPongTextures[dst];
    }

    void Renderer::composite(Texture2DRenderable* composited,Texture2DRenderable* inSceneColor, Texture2DRenderable* inBloomColor, const glm::uvec2& outputResolution)
    {
        // add a reconstruction pass using Gaussian filter
        gaussianBlur(inSceneColor, 2u, 1.0f);

        Shader* compositeShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "CompositeShader", SHADER_SOURCE_PATH "composite_v.glsl", SHADER_SOURCE_PATH "composite_p.glsl" });
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(composited->width, composited->height));
        renderTarget->setColorBuffer(composited, 0);

        submitFullScreenPass(
            renderTarget.get(),
            compositeShader,
            [this, inBloomColor, inSceneColor](RenderTarget* renderTarget, Shader* shader) {
                shader->setUniform("enableTonemapping", m_settings.enableTonemapping ? 1.f : 0.f);
                shader->setUniform("tonemapOperator", m_settings.tonemapOperator);
                if (inBloomColor && m_settings.enableBloom) {
                    shader->setUniform("enableBloom", 1.f);
                } else {
                    shader->setUniform("enableBloom", 0.f);
                }
                shader->setUniform("exposure", m_settings.exposure)
                    .setUniform("colorTempreture", m_settings.colorTempreture)
                    .setUniform("bloomIntensity", m_settings.bloomIntensity)
                    .setTexture("bloomTexture", inBloomColor)
                    .setTexture("sceneColorTexture", inSceneColor);
            });
    }

    void Renderer::gaussianBlur(Texture2DRenderable* inoutTexture, u32 inRadius, f32 inSigma) {
        static const u32 kMaxkernelRadius = 10;
        static const u32 kMinKernelRadius = 2;

        struct GaussianKernel
        {
            GaussianKernel(u32 inRadius, f32 inSigma, LinearAllocator& allocator)
                : radius(Max(kMinKernelRadius, Min(inRadius, kMaxkernelRadius))), sigma(inSigma)
            {
                u32 kernelSize = radius;
                // todo: allocate weights from stack or frame allocator instead
                weights = new f32[kMaxkernelRadius];

                // calculate kernel weights
                f32 totalWeight = 0.f;
                const f32 step = 1.f;
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    f32 x = (f32)i * step;
                    weights[i] = calcWeight(x);
                    totalWeight += weights[i];
                }

                // tweak the weights so that the boundry sample weight goes to 0
                f32 correction = calcWeight(step * kernelSize);
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    weights[i] -= correction;
                    totalWeight -= correction;
                }

                // normalize the weights to 1
                totalWeight = totalWeight * 2.f - weights[0];
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    weights[i] /= totalWeight;
                }
            };

            ~GaussianKernel()
            {
                delete[] weights;
            }

            f32 calcWeight(f32 x)
            {
                f32 coef = 1.f / (sigma * glm::sqrt(2.0f * M_PI));
                return coef * glm::exp(-.5f  * pow(x / sigma, 2.0));
            }

            u32 radius = 0u;
            f32 mean = 0.f;
            f32 sigma = 0.5f;
            f32* weights = nullptr;
        };

        Shader* gaussianBlurShader = ShaderManager::createShader({ ShaderType::kVsPs, "GaussianBlurShader", SHADER_SOURCE_PATH "gaussian_blur_v.glsl", SHADER_SOURCE_PATH "gaussian_blur_p.glsl" });

        // create scratch buffer for storing intermediate output
        ITextureRenderable::Parameter params = { };
        params.wrap_s = WM_CLAMP;
        params.wrap_r = WM_CLAMP;
        params.wrap_t = WM_CLAMP;
        auto scratchBuffer = std::make_shared<Texture2DRenderable>("GaussianBlurH", inoutTexture->getTextureSpec(), params);
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(inoutTexture->width, inoutTexture->height));
        renderTarget->setColorBuffer(scratchBuffer.get(), 0);

        GaussianKernel kernel(inRadius, inSigma, m_frameAllocator);

        // horizontal pass
        submitFullScreenPass(
            renderTarget.get(),
            gaussianBlurShader,
            [inoutTexture, &kernel](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcTexture", inoutTexture);
                shader->setUniform("kernelRadius", kernel.radius);
                shader->setUniform("pass", 1.f);
                for (u32 i = 0; i < kMaxkernelRadius; ++i)
                {
                    char name[32] = { };
                    sprintf_s(name, "weights[%d]", i);
                    shader->setUniform(name, kernel.weights[i]);
                }
            }
        );

        // vertical pass
        renderTarget->setColorBuffer(inoutTexture, 0);
        // execute
        submitFullScreenPass(
            renderTarget.get(),
            gaussianBlurShader,
            [scratchBuffer, &kernel](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcTexture", scratchBuffer.get());
                shader->setUniform("kernelRadius", kernel.radius);
                shader->setUniform("pass", 0.f);
                for (u32 i = 0; i < kMaxkernelRadius; ++i)
                {
                    char name[32] = { };
                    sprintf_s(name, "weights[%d]", i);
                    shader->setUniform(name, kernel.weights[i]);
                }
            }
        );
    }

#if 0
    void Renderer::gaussianBlurImmediate(Texture2DRenderable* inoutTexture, u32 inRadius, f32 inSigma) {
        static const u32 kMaxkernelRadius = 10;
        static const u32 kMinKernelRadius = 2;

        struct GaussianKernel
        {
            GaussianKernel(u32 inRadius, f32 inSigma, LinearAllocator& allocator)
                : radius(Max(kMinKernelRadius, Min(inRadius, kMaxkernelRadius))), sigma(inSigma)
            {
                u32 kernelSize = radius;
                // todo: allocate weights from stack or frame allocator instead
                weights = new f32[kMaxkernelRadius];

                // calculate kernel weights
                f32 totalWeight = 0.f;
                const f32 step = 1.f;
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    f32 x = (f32)i * step;
                    weights[i] = calcWeight(x);
                    totalWeight += weights[i];
                }

                // tweak the weights so that the boundry sample weight goes to 0
                f32 correction = calcWeight(step * kernelSize);
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    weights[i] -= correction;
                    totalWeight -= correction;
                }

                // normalize the weights to 1
                totalWeight = totalWeight * 2.f - weights[0];
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    weights[i] /= totalWeight;
                }
            };

            ~GaussianKernel()
            {
                delete[] weights;
            }

            f32 calcWeight(f32 x)
            {
                f32 coef = 1.f / (sigma * glm::sqrt(2.0f * M_PI));
                return coef * glm::exp(-.5f  * pow(x / sigma, 2.0));
            }

            u32 radius = 0u;
            f32 mean = 0.f;
            f32 sigma = 0.5f;
            f32* weights = nullptr;
        };

        Shader* gaussianBlurShader = ShaderManager::createShader({ ShaderType::kVsPs, "GaussianBlurShader", SHADER_SOURCE_PATH "gaussian_blur_v.glsl", SHADER_SOURCE_PATH "gaussian_blur_p.glsl" });

        // create scratch buffer for storing intermediate output
        ITextureRenderable::Parameter params = { };
        params.wrap_s = WM_CLAMP;
        params.wrap_r = WM_CLAMP;
        params.wrap_t = WM_CLAMP;
        auto scratchBuffer = std::make_shared<Texture2DRenderable>("GaussianBlurH", inoutTexture->getTextureSpec(), params);
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(inoutTexture->width, inoutTexture->height));
        renderTarget->setColorBuffer(scratchBuffer.get(), 0);

        GaussianKernel kernel(inRadius, inSigma, m_frameAllocator);

        // horizontal pass
        submitFullScreenPass(
            renderTarget.get(),
            { { 0 } },
            gaussianBlurShader,
            [inoutTexture, &kernel](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcTexture", inoutTexture);
                shader->setUniform("kernelRadius", kernel.radius);
                shader->setUniform("pass", 1.f);
                for (u32 i = 0; i < kMaxkernelRadius; ++i)
                {
                    char name[32] = { };
                    sprintf_s(name, "weights[%d]", i);
                    shader->setUniform(name, kernel.weights[i]);
                }
            }
        );

        // vertical pass
        renderTarget->setColorBuffer(inoutTexture, 0);
        // execute
        submitFullScreenPass(
            renderTarget.get(),
            { { 0 } },
            gaussianBlurShader,
            [scratchBuffer, &kernel](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcTexture", scratchBuffer.get());
                shader->setUniform("kernelRadius", kernel.radius);
                shader->setUniform("pass", 0.f);
                for (u32 i = 0; i < kMaxkernelRadius; ++i)
                {
                    char name[32] = { };
                    sprintf_s(name, "weights[%d]", i);
                    shader->setUniform(name, kernel.weights[i]);
                }
            }
        );
    }

    RenderTexture2D* Renderer::gaussianBlur(RenderTexture2D* inTexture, u32 inRadius, f32 inSigma)
    {
        static const u32 kMaxkernelRadius = 10;
        static const u32 kMinKernelRadius = 2;

        struct GaussianKernel
        {
            GaussianKernel(u32 inRadius, f32 inSigma, LinearAllocator& allocator)
                : radius(Max(kMinKernelRadius, Min(inRadius, kMaxkernelRadius))), sigma(inSigma)
            {
                u32 kernelSize = radius;
                // todo: allocate weights from stack or frame allocator instead
                weights = new f32[kMaxkernelRadius];

                // calculate kernel weights
                f32 totalWeight = 0.f;
                const f32 step = 1.f;
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    f32 x = (f32)i * step;
                    weights[i] = calcWeight(x);
                    totalWeight += weights[i];
                }

                // tweak the weights so that the boundry sample weight goes to 0
                f32 correction = calcWeight(step * kernelSize);
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    weights[i] -= correction;
                    totalWeight -= correction;
                }

                // normalize the weights to 1
                totalWeight = totalWeight * 2.f - weights[0];
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    weights[i] /= totalWeight;
                }
            };

            ~GaussianKernel()
            {
                delete[] weights;
            }

            f32 calcWeight(f32 x)
            {
                f32 coef = 1.f / (sigma * glm::sqrt(2.0f * M_PI));
                return coef * glm::exp(-.5f  * pow(x / sigma, 2.0));
            }

            u32 radius = 0u;
            f32 mean = 0.f;
            f32 sigma = 0.5f;
            f32* weights = nullptr;
        };

        // horizontal pass
        auto outTextureH = m_renderQueue.createTexture2D("OutGaussianTextureH", inTexture->spec);
        m_renderQueue.addPass(
            "GaussianBlurHPass",
            [inTexture, outTextureH](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addInput(inTexture);
                pass->addInput(outTextureH);
                pass->addOutput(outTextureH);
            },
            [inTexture, outTextureH, inRadius, inSigma, this]() {
                Shader* gaussianBlurShader = ShaderManager::createShader({ ShaderType::kVsPs, "GaussianBlurShader", SHADER_SOURCE_PATH "gaussian_blur_v.glsl", SHADER_SOURCE_PATH "gaussian_blur_p.glsl" });

                auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(outTextureH->spec.width, outTextureH->spec.height));
                renderTarget->setColorBuffer(outTextureH->getTextureResource(), 0);

                GaussianKernel kernel(inRadius, inSigma, m_frameAllocator);

                // execute
                submitFullScreenPass(
                    renderTarget.get(),
                    { { 0 } },
                    gaussianBlurShader,
                    [inTexture, &kernel](RenderTarget* renderTarget, Shader* shader) {
                        shader->setTexture("srcTexture", inTexture->getTextureResource());
                        shader->setUniform("kernelRadius", kernel.radius);
                        shader->setUniform("pass", 1.f);
                        for (u32 i = 0; i < kMaxkernelRadius; ++i)
                        {
                            char name[32] = { };
                            sprintf_s(name, "weights[%d]", i);
                            shader->setUniform(name, kernel.weights[i]);
                        }
                    }
                );
            }
        );

        auto outTextureV = m_renderQueue.createTexture2D("OutGaussianTextureV", inTexture->spec);
        m_renderQueue.addPass(
            "GaussianBlurVPass", 
            [outTextureH, outTextureV](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addInput(outTextureH);
                pass->addInput(outTextureV);
                pass->addOutput(outTextureV);
            },
            [this, outTextureH, outTextureV, inRadius, inSigma]() {
                Shader* gaussianBlurShader = ShaderManager::createShader({ ShaderType::kVsPs, "GaussianBlurShader", SHADER_SOURCE_PATH "gaussian_blur_v.glsl", SHADER_SOURCE_PATH "gaussian_blur_p.glsl" });

                auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(outTextureV->spec.width, outTextureV->spec.height));
                renderTarget->setColorBuffer(outTextureV->getTextureResource(), 0);

                GaussianKernel kernel(inRadius, inSigma, m_frameAllocator);

                // execute
                submitFullScreenPass(
                    renderTarget.get(),
                    { { 0 } },
                    gaussianBlurShader,
                    [outTextureH, &kernel](RenderTarget* renderTarget, Shader* shader) {
                        shader->setTexture("srcTexture", outTextureH->getTextureResource());
                        shader->setUniform("kernelRadius", kernel.radius);
                        shader->setUniform("pass", 0.f);
                        for (u32 i = 0; i < kMaxkernelRadius; ++i)
                        {
                            char name[32] = { };
                            sprintf_s(name, "weights[%d]", i);
                            shader->setUniform(name, kernel.weights[i]);
                        }
                    }
                );
            });
        return outTextureV;
    }
#endif

    void Renderer::addUIRenderCommand(const std::function<void()>& UIRenderCommand)
    {
        m_UIRenderCommandQueue.push(UIRenderCommand);
    }

    void Renderer::renderUI()
    {
        // set to default render target
        m_ctx->setRenderTarget(nullptr, { });

        // begin imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        while (!m_UIRenderCommandQueue.empty())
        {
            const auto& command = m_UIRenderCommandQueue.front();
            command();
            m_UIRenderCommandQueue.pop();
        }

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

        // turn off ssao(
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

    void Renderer::debugDrawSphere(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::mat4& view, const glm::mat4& projection) {
        Shader* debugDrawShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawShader", SHADER_SOURCE_PATH "debug_draw_v.glsl", SHADER_SOURCE_PATH "debug_draw_p.glsl" });
        submitMesh(
            renderTarget,
            viewport,
            GfxPipelineState(),
            AssetManager::getAsset<Mesh>("Sphere"),
            debugDrawShader,
            [this, position, scale, view, projection](RenderTarget* renderTarget, Shader* shader) {
                glm::mat4 mvp(1.f);
                mvp = glm::translate(mvp, position);
                mvp = glm::scale(mvp, scale);
                mvp = projection * view * mvp;
                shader->setUniform("mvp", mvp);
                shader->setUniform("color", glm::vec3(1.f, 0.f, 0.f));
            }
        );
    }

    void Renderer::debugDrawCubeImmediate(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::mat4& view, const glm::mat4& projection) {
        Shader* debugDrawShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawImmediateShader", SHADER_SOURCE_PATH "debug_draw_immediate_v.glsl", SHADER_SOURCE_PATH "debug_draw_immediate_p.glsl" });
        submitMesh(
            renderTarget,
            viewport,
            GfxPipelineState(),
            AssetManager::getAsset<Mesh>("UnitCubeMesh"),
            debugDrawShader,
            [this, position, scale, view, projection](RenderTarget* renderTarget, Shader* shader) {
                glm::mat4 mvp(1.f);
                mvp = glm::translate(mvp, position);
                mvp = glm::scale(mvp, scale);
                mvp = projection * view * mvp;
                shader->setUniform("mvp", mvp);
                shader->setUniform("color", glm::vec3(1.f, 0.f, 0.f));
            }
        );
    }

    void Renderer::debugDrawCubeBatched(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& facingDir, const glm::vec4& color, const glm::mat4& view, const glm::mat4& projection) {

    }

    void Renderer::debugDrawLineImmediate(const glm::vec3& v0, const glm::vec3& v1)
    {

    }

    void Renderer::debugDrawCubemap(TextureCubeRenderable* cubemap) {
        static PerspectiveCamera camera(
            glm::vec3(0.f, 1.f, 2.f),
            glm::vec3(0.f, 0.f, 0.f),
            glm::vec3(0.f, 1.f, 0.f),
            90.f,
            0.1f,
            16.f,
            16.f / 9.f
        );

        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(320, 180));
        ITextureRenderable::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = 320;
        spec.height = 180;
        spec.pixelFormat = PF_RGB16F;

        static Texture2DRenderable* outTexture = new Texture2DRenderable("CubemapViewerTexture", spec);
        renderTarget->setColorBuffer(outTexture, 0);
        renderTarget->clearDrawBuffer(0, glm::vec4(0.2, 0.2, 0.2, 1.f));
        
        Shader* shader = ShaderManager::createShader({ 
                ShaderSource::Type::kVsPs, 
                "DebugDrawCubemapShader", 
                SHADER_SOURCE_PATH "debug_draw_cubemap_v.glsl", 
                SHADER_SOURCE_PATH "debug_draw_cubemap_p.glsl" 
        });
        glDisable(GL_CULL_FACE);
        submitMesh(
            renderTarget.get(),
            {0, 0, renderTarget->width, renderTarget->height },
            GfxPipelineState(),
            AssetManager::getAsset<Mesh>("UnitCubeMesh"),
            shader,
            [cubemap](RenderTarget* renderTarget, Shader* shader) {
                shader->setUniform("model", glm::mat4(1.f));
                shader->setUniform("view", camera.view());
                shader->setUniform("projection", camera.projection());
                shader->setTexture("cubemap", cubemap);
            }
        );
        glEnable(GL_CULL_FACE);

        // draw the output texture to the cubemap viewer
        addUIRenderCommand([]() {
            ImGui::Begin("Debug Viewer"); {

                ImGui::Image((ImTextureID)outTexture->getGpuResource(), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));

                // todo: implement a proper mouse input mechanism for this viewer
                // todo: abstract this into a debug viewer widget or something
                ImVec2 rectMin = ImGui::GetWindowContentRegionMin();
                ImVec2 rectMax = ImGui::GetWindowContentRegionMax();
                if (ImGui::IsMouseHoveringRect(rectMin, rectMax)) {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                        glm::dvec2 mouseCursorChange = IOSystem::get()->getMouseCursorChange();
                        const f32 C = 0.02f;
                        f32 phi = mouseCursorChange.x * C;
                        f32 theta = mouseCursorChange.y * C;
                        /** note - @min:
                        * copied from Camera.cpp CameraEntity::orbit();
                        */
                        glm::vec3 p = camera.position - camera.lookAt;
                        glm::quat quat(cos(.5f * -phi), sin(.5f * -phi) * camera.worldUp);
                        quat = glm::rotate(quat, -theta, camera.right());
                        glm::mat4 model(1.f);
                        model = glm::translate(model, camera.lookAt);
                        glm::mat4 rot = glm::toMat4(quat);
                        glm::vec4 pPrime = rot * glm::vec4(p, 1.f);
                        camera.position = glm::vec3(pPrime.x, pPrime.y, pPrime.z) + camera.lookAt;
                    }
                }
            } ImGui::End();
        });
    }

    void Renderer::debugDrawCubemap(GLuint cubemap) {
        static PerspectiveCamera camera(
            glm::vec3(0.f, 1.f, 2.f),
            glm::vec3(0.f, 0.f, 0.f),
            glm::vec3(0.f, 1.f, 0.f),
            90.f,
            0.1f,
            16.f,
            16.f / 9.f
        );

        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(320, 180));
        ITextureRenderable::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = 320;
        spec.height = 180;
        spec.pixelFormat = PF_RGB16F;

        static Texture2DRenderable* outTexture = new Texture2DRenderable("CubemapViewerTexture", spec);
        renderTarget->setColorBuffer(outTexture, 0);
        renderTarget->clearDrawBuffer(0, glm::vec4(0.2, 0.2, 0.2, 1.f));
        
        Shader* shader = ShaderManager::createShader({ 
                ShaderSource::Type::kVsPs, 
                "DebugDrawCubemapShader", 
                SHADER_SOURCE_PATH "debug_draw_cubemap_v.glsl", 
                SHADER_SOURCE_PATH "debug_draw_cubemap_p.glsl" 
        });
        glDisable(GL_CULL_FACE);
        submitMesh(
            renderTarget.get(),
            {0, 0, renderTarget->width, renderTarget->height },
            GfxPipelineState(),
            AssetManager::getAsset<Mesh>("UnitCubeMesh"),
            shader,
            [cubemap](RenderTarget* renderTarget, Shader* shader) {
                shader->setUniform("model", glm::mat4(1.f));
                shader->setUniform("view", camera.view());
                shader->setUniform("projection", camera.projection());
                shader->setUniform("cubemap", 128);
                glBindTextureUnit(128, cubemap);
                // shader->setTexture("cubemap", cubemap);
            }
        );
        glEnable(GL_CULL_FACE);

        // draw the output texture to the cubemap viewer
        addUIRenderCommand([]() {
            ImGui::Begin("Debug Viewer"); {

                ImGui::Image((ImTextureID)outTexture->getGpuResource(), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));

                // todo: implement a proper mouse input mechanism for this viewer
                // todo: abstract this into a debug viewer widget or something
                ImVec2 rectMin = ImGui::GetWindowContentRegionMin();
                ImVec2 rectMax = ImGui::GetWindowContentRegionMax();
                if (ImGui::IsMouseHoveringRect(rectMin, rectMax)) {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                        glm::dvec2 mouseCursorChange = IOSystem::get()->getMouseCursorChange();
                        const f32 C = 0.02f;
                        f32 phi = mouseCursorChange.x * C;
                        f32 theta = mouseCursorChange.y * C;
                        /** note - @min:
                        * copied from Camera.cpp CameraEntity::orbit();
                        */
                        glm::vec3 p = camera.position - camera.lookAt;
                        glm::quat quat(cos(.5f * -phi), sin(.5f * -phi) * camera.worldUp);
                        quat = glm::rotate(quat, -theta, camera.right());
                        glm::mat4 model(1.f);
                        model = glm::translate(model, camera.lookAt);
                        glm::mat4 rot = glm::toMat4(quat);
                        glm::vec4 pPrime = rot * glm::vec4(p, 1.f);
                        camera.position = glm::vec3(pPrime.x, pPrime.y, pPrime.z) + camera.lookAt;
                    }
                }
            } ImGui::End();
        });
    }

    void Renderer::renderDebugObjects() {

    }
}