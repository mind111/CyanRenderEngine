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

#define GPU_RAYTRACING 0

namespace Cyan
{
    void RenderTexture2D::createResource(RenderQueue& renderQueue)
    { 
        if (!texture)
        {
            texture = renderQueue.createTexture2DInternal(tag, spec);
        }
    }
    
    Renderer* Singleton<Renderer>::singleton = nullptr;
    static Mesh* fullscreenQuad = nullptr;

    Renderer::Renderer(GfxContext* ctx, u32 windowWidth, u32 windowHeight)
        : Singleton<Renderer>(), 
        m_ctx(ctx),
        m_windowWidth(windowWidth),
        m_windowHeight(windowHeight),
        m_SSAAWidth(2 * windowWidth),
        m_SSAAHeight(2 * windowHeight),
        m_frameAllocator(1024 * 1024 * 32) // 32MB frame allocator 
    {
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
                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R8G8B8A8;
                ITextureRenderable::Parameter parameter = { };
                parameter.minificationFilter = ITextureRenderable::Parameter::Filtering::LINEAR_MIPMAP_LINEAR;
                parameter.magnificationFilter = ITextureRenderable::Parameter::Filtering::NEAREST;
                // m_sceneVoxelGrid.normal = AssetManager::createTexture3D("VoxelGridNormal", spec, parameter);
                // m_sceneVoxelGrid.emission = AssetManager::createTexture3D("VoxelGridEmission", spec, parameter);

                spec.numMips = std::log2(m_sceneVoxelGrid.resolution) + 1;
                // m_sceneVoxelGrid.albedo = AssetManager::createTexture3D("VoxelGridAlbedo", spec, parameter);
                // m_sceneVoxelGrid.radiance = AssetManager::createTexture3D("VoxelGridRadiance", spec, parameter);

                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R32F;
                // m_sceneVoxelGrid.opacity = AssetManager::createTexture3D("VoxelGridOpacity", spec, parameter);
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
    }

    void Renderer::finalize()
    {
        // imgui clean up
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void Renderer::drawMeshInstance(const RenderableScene& sceneRenderable, RenderTarget* renderTarget, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers, bool clearRenderTarget, Viewport viewport, GfxPipelineState pipelineState, MeshInstance* meshInstance, i32 transformIndex)
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
                task.clearRenderTarget = clearRenderTarget;
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
            task.pipelineState = pipelineState;
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
        // set render target
        m_ctx->setRenderTarget(task.renderTarget, task.drawBuffers);
        if (task.clearRenderTarget)
        {
            task.renderTarget->clear(task.drawBuffers);
        }
        m_ctx->setViewport(task.viewport);

        // set shader
        m_ctx->setShader(task.shader);

        // set graphics pipeline state
        m_ctx->setDepthControl(task.pipelineState.depth);
        m_ctx->setPrimitiveType(task.pipelineState.primitiveMode);

        // pre-draw setup
        task.renderSetupLambda(task.renderTarget, task.shader);
        task.shader->commit(m_ctx);

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
        m_ctx->clearTransientTextureBindings();
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

    void Renderer::render(Scene* scene)
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
        beginRender();
        {
            glm::uvec2 renderResolution = m_settings.enableAA ? glm::uvec2(m_SSAAWidth, m_SSAAHeight) : glm::uvec2(m_windowWidth, m_windowHeight);
            SceneView sceneView(*scene, scene->camera->getCamera(), EntityFlag_kVisible);
            // convert Scene instance to RenderableScene instance for rendering
            RenderableScene renderableScene(scene, sceneView, m_frameAllocator);
            renderableScene.setView(scene->camera->view());
            renderableScene.setProjection(scene->camera->projection());

            // main scene pass
#if GPU_RAYTRACING
            RenderTexture2D* sceneColorTexture = nullptr;
            ITextureRenderable::Spec spec = { };
            spec.type = TEX_2D;
            spec.pixelFormat = PF_RGB16F;
            spec.width = 2560;
            spec.height = 1440;

            auto depthPrepassOutput = renderSceneDepthNormal(renderableScene, sceneView, renderResolution);

            sceneColorTexture = m_renderQueue.createTexture2D("RayTracingOutput", spec);
            RayTracingScene rtxScene(*scene);
            gpuRayTracing(rtxScene, sceneColorTexture, depthPrepassOutput.depthTexture, depthPrepassOutput.normalTexture);
#else
            renderShadow(*scene, renderableScene);

            // scene depth & normal pass
            // bool depthNormalPrepassRequired = (m_settings.enableSSAO || m_settings.enableVctx);
            bool depthNormalPrepassRequired = true;
            SSGITextures SSGIOutput = { };
            if (depthNormalPrepassRequired)
            {
                auto depthPrepassOutput = renderSceneDepthNormal(renderableScene, sceneView, renderResolution);
                SSGIOutput = screenSpaceRayTracing(depthPrepassOutput.depthTexture, depthPrepassOutput.normalTexture);
            }

            /*
            // voxel cone tracing pass
            if (m_settings.enableVctx)
            {
                // todo: revoxelizing the scene every frame is causing flickering in the volume texture
                // voxelizeScene(scene);
                // renderVctx();
            }

            // ssao pass
            if (m_settings.enableSSAO)
            {
                Texture2DRenderable* sceneDepthTexture = m_settings.enableAA ? m_sceneDepthTextureSSAA : m_sceneDepthTexture;
                Texture2DRenderable* sceneNormalTexture = m_settings.enableAA ? m_sceneNormalTextureSSAA : m_sceneNormalTexture;
                sceneView.renderTarget = m_ssaoRenderTarget;
                sceneView.drawBuffers = {
                    { 0u }
                };
                ssao(sceneView, sceneDepthTexture, sceneNormalTexture, glm::uvec2(1280, 720));
            }
            */

#if BINDLESS_TEXTURE
            RenderTexture2D* sceneColorTexture = renderSceneMultiDraw(renderableScene, sceneView, renderResolution, SSGIOutput);
#else
            RenderTexture2D* sceneColorTexture = renderScene(renderableScene, sceneView, renderResolution);
#endif
#endif
            if (m_settings.enableTAA)
            {
                taa();
            }

            // post processing
            RenderTexture2D* bloomOutput = nullptr;
            RenderTexture2D* finalColorOutput = nullptr;
            bloomOutput = bloom(sceneColorTexture);
            finalColorOutput = composite(sceneColorTexture, bloomOutput, glm::uvec2(m_windowWidth, m_windowHeight));
            renderToScreen(finalColorOutput);

            m_renderQueue.execute();
        } 
        endRender();
    }

    void Renderer::renderToScreen(RenderTexture2D* inTexture)
    {
        m_renderQueue.addPass(
            "RenderToScreenPass",
            [inTexture](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addInput(inTexture);
            },
            [inTexture, this]() {
                Shader* fullscreenBlitShader = ShaderManager::createShader({
                    ShaderSource::Type::kVsPs, 
                    "BlitQuadShader",
                    SHADER_SOURCE_PATH "blit_v.glsl",
                    SHADER_SOURCE_PATH "blit_p.glsl"
                    });

                // final blit to default framebuffer
                submitFullScreenPass(
                    RenderTarget::getDefaultRenderTarget(m_windowWidth, m_windowHeight),
                    { { 0u } },
                    fullscreenBlitShader,
                    [this, inTexture](RenderTarget* renderTarget, Shader* shader) {
                        shader->setTexture("srcTexture", inTexture->getTextureResource());
                    });
            });
    }

    void Renderer::endRender()
    {
        // render UI widgets
        renderUI();
        m_ctx->flip();
        m_numFrames++;
    }

    void Renderer::renderShadow(const Scene& scene, const RenderableScene& renderableScene)
    {
        // construct a scene view for all shadow casting lights 
        SceneView shadowView(scene, scene.camera->getCamera(), EntityFlag_kVisible | EntityFlag_kCastShadow);
        RenderableScene shadowScene(&scene, shadowView, m_frameAllocator);

        for (auto light : renderableScene.lights)
        {
            light->renderShadow(scene, shadowScene, *this);
        }

        // m_renderQueue.execute();
    }

    void Renderer::renderSceneMeshOnly(RenderableScene& sceneRenderable, RenderTexture2D* outTexture, Shader* shader)
    {
        m_renderQueue.addPass(
            "MeshOnlyPass",
            [outTexture](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addOutput(outTexture);
            },
            [this, &sceneRenderable, outTexture, shader]() {
                sceneRenderable.submitSceneData(m_ctx);

                auto renderTarget = createRenderTarget(outTexture->spec.width, outTexture->spec.height);
                u32 transformIndex = 0u;
                for (auto& meshInst : sceneRenderable.meshInstances)
                {
                    submitMesh(
                        renderTarget,
                        { { 0 } },
                        false,
                        { 0u, 0u, renderTarget->width, renderTarget->height },
                        GfxPipelineState(), 
                        meshInst->parent, 
                        shader,
                        [transformIndex](RenderTarget* renderTarget, Shader* shader) {
                            shader->setUniform("transformIndex", (i32)transformIndex);
                        });
                    transformIndex++;
                }
            }
        );
    }

    void Renderer::renderSceneDepthOnly(RenderableScene& renderableScene, RenderTexture2D* outDepthTexture)
    {
        m_renderQueue.addPass(
            "DepthOnlyPass",
            [outDepthTexture](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addOutput(outDepthTexture);
            },
            [this, renderableScene, outDepthTexture]() mutable {

                Shader* depthOnlyShader = ShaderManager::createShader({ 
                    ShaderSource::Type::kVsPs, 
                    "DepthOnlyShader", 
                    SHADER_SOURCE_PATH "depth_only_v.glsl", 
                    SHADER_SOURCE_PATH "depth_only_p.glsl",
                    });

                // create render target
                std::unique_ptr<RenderTarget> depthRenderTargetPtr(createDepthOnlyRenderTarget(outDepthTexture->spec.width, outDepthTexture->spec.height));
                depthRenderTargetPtr->setDepthBuffer(reinterpret_cast<DepthTexture*>(outDepthTexture->getTextureResource()));
                depthRenderTargetPtr->clear({ { 0u } });

                renderableScene.submitSceneData(m_ctx);

                u32 transformIndex = 0u;
                for (auto& meshInst : renderableScene.meshInstances)
                {
                    submitMesh(
                        depthRenderTargetPtr.get(),
                        { { 0 } },
                        false,
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
        );
    }

    Renderer::ScenePrepassOutput Renderer::renderSceneDepthNormal(RenderableScene& renderableScene, const SceneView& sceneView, const glm::uvec2& outputResolution)
    {
        ITextureRenderable::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = outputResolution.x;
        spec.height = outputResolution.y;
        spec.pixelFormat = PF_RGB32F;
        auto depthTexture = m_renderQueue.createTexture2D("SceneDepthTexture", spec);
        auto normalTexture = m_renderQueue.createTexture2D("SceneNormalTexture", spec);

        m_renderQueue.addPass(
            "SceneDepthNormalPass",
            [this, depthTexture, normalTexture](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addInput(depthTexture);
                pass->addInput(normalTexture);
                pass->addOutput(depthTexture);
                pass->addOutput(normalTexture);
            },
            [this, depthTexture, normalTexture, &renderableScene]() {
                renderableScene.submitSceneData(m_ctx);

                std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(depthTexture->spec.width, depthTexture->spec.height));
                renderTarget->setColorBuffer(depthTexture->getTextureResource(), 0);
                renderTarget->setColorBuffer(normalTexture->getTextureResource(), 1);
                GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
                glNamedFramebufferDrawBuffers(renderTarget->fbo, 2, drawBuffers);
                renderTarget->clear({ 
                    { 0, glm::vec4(1.f) },
                    { 1, glm::vec4(0.f, 0.f, 0.f, 1.f) } 
                    });

                Shader* sceneDepthNormalShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "SceneDepthNormalShader", SHADER_SOURCE_PATH "scene_depth_normal_v.glsl", SHADER_SOURCE_PATH "scene_depth_normal_p.glsl" });
                u32 transformIndex = 0u;
                for (auto& meshInst : renderableScene.meshInstances)
                {
                    submitMesh(
                        /*renderTarget=*/renderTarget.get(),
                        /*drawBuffers=*/{ {0, glm::vec4(1.f) }, {1} },
                        /*clearRenderTarget=*/false,
                        /*viewport=*/{0u, 0u, renderTarget->width, renderTarget->height},
                        GfxPipelineState(), // pipeline state
                        meshInst->parent, // mesh
                        sceneDepthNormalShader, // shader
                        [&transformIndex, this](RenderTarget* renderTarget, Shader* shader) { // renderSetupLambda
                            shader->setUniform("transformIndex", (i32)transformIndex++);
                        });
                }
            });

        addUIRenderCommand([depthTexture, normalTexture](){
            ImGui::Begin("Texture Viewer");
            auto depth = depthTexture->getTextureResource();
            auto normal = normalTexture->getTextureResource();
            ImGui::Image((ImTextureID)(depth->getGpuResource()), ImVec2(480, 270), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::Image((ImTextureID)(normal->getGpuResource()), ImVec2(480, 270), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::End();
        });
        return { depthTexture, normalTexture};
    }

    Renderer::SSGITextures Renderer::screenSpaceRayTracing(RenderTexture2D* sceneDepthTexture, RenderTexture2D* sceneNormalTexture)
    {
        RenderTexture2D* ssao = m_renderQueue.createTexture2D("SSAO", sceneNormalTexture->spec);
        RenderTexture2D* ssbn = m_renderQueue.createTexture2D("SSBN", sceneNormalTexture->spec);

        // screen space ao
        m_renderQueue.addPass(
            "ScreenSpaceRayTracingPass",
            [this, sceneDepthTexture, sceneNormalTexture, ssao, ssbn](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addInput(sceneDepthTexture);
                pass->addInput(sceneNormalTexture);
                pass->addInput(ssao);
                pass->addInput(ssbn);
                pass->addOutput(ssao);
                pass->addOutput(ssbn);
            },
            [this, ssao, ssbn, sceneDepthTexture, sceneNormalTexture]() {
                std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(ssao->spec.width, ssao->spec.height));
                renderTarget->setColorBuffer(ssao->getTextureResource(), 0);
                renderTarget->setColorBuffer(ssbn->getTextureResource(), 1);

                GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
                glNamedFramebufferDrawBuffers(renderTarget->fbo, 2, drawBuffers);
                renderTarget->clear({ 
                    { 0, glm::vec4(0.f, 0.f, 0.f, 1.f) },
                    { 1, glm::vec4(0.f, 0.f, 0.f, 1.f) },
                });

                Shader* ssrtShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "ScreenSpaceRayTracingShader", SHADER_SOURCE_PATH "screenspace_raytracing.glsl", SHADER_SOURCE_PATH "screenspace_raytracing.glsl" });
                submitFullScreenPass(
                    renderTarget.get(),
                    { { 0 }, { 1 } },
                    ssrtShader,
                    [sceneDepthTexture, sceneNormalTexture](RenderTarget* renderTarget, Shader* shader) {
                        shader->setTexture("depthTexture", sceneDepthTexture->getTextureResource());
                        shader->setTexture("normalTexture", sceneNormalTexture->getTextureResource());
                        auto blueNoiseTexture = AssetManager::getAsset<Texture2DRenderable>("BlueNoise_1024x1024");
                        shader->setTexture("blueNoiseTexture", blueNoiseTexture);
                    });
            });

        // screen space bent normal

        // screen space diffuse inter-reflection

        // screen space reflection

        addUIRenderCommand([this, ssao, ssbn](){
            ImGui::Begin("SSGI Viewer");
            ImGui::Checkbox("Bent Normal", &m_settings.useBentNormal);
            enum class Visualization
            {
                kAmbientOcclusion,
                kBentNormal,
                kCount
            };
            static i32 visualization = (i32)Visualization::kAmbientOcclusion;
            const char* visualizationNames[(i32)Visualization::kCount] = { "Ambient Occlusion", "Bent Normal", };
            ImGui::Combo("Visualization", &visualization, visualizationNames, (i32)Visualization::kCount);
            if (visualization == (i32)Visualization::kAmbientOcclusion)
                ImGui::Image((ImTextureID)(ssao->getTextureResource()->getGpuResource()), ImVec2(480, 270), ImVec2(0, 1), ImVec2(1, 0));
            if (visualization == (i32)Visualization::kBentNormal)
                ImGui::Image((ImTextureID)(ssbn->getTextureResource()->getGpuResource()), ImVec2(480, 270), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::End();
        });
        return { ssao, ssbn };
    }

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

    RenderTexture2D* Renderer::renderScene(RenderableScene& renderableScene, const SceneView& sceneView, const glm::uvec2& outputResolution)
    {
        ITextureRenderable::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = outputResolution.x;
        spec.height = outputResolution.y;
        spec.pixelFormat = PF_RGB16F;
        RenderTexture2D* outSceneColor = m_renderQueue.createTexture2D("SceneColor", spec);

        m_renderQueue.addPass(
            "MainScenePass",
            [outSceneColor, &renderableScene](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addOutput(outSceneColor);
            },
            [this, outSceneColor, &renderableScene, &sceneView]() {
                std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(outSceneColor->spec.width, outSceneColor->spec.height));
                renderTarget->setColorBuffer(outSceneColor->getTextureResource(), 0);
                renderTarget->clear({ { 0 } });

                renderableScene.submitSceneData(m_ctx);
                // render skybox
                if (renderableScene.skybox)
                {
                    renderableScene.skybox->render();
                }
                // render mesh instances
                u32 transformIndex = 0u;
                for (auto meshInst : renderableScene.meshInstances)
                {
                    drawMeshInstance(
                        renderableScene,
                        renderTarget.get(),
                        { { 0 } },
                        false,
                        { 0u, 0u, outSceneColor->spec.width, outSceneColor->spec.height },
                        GfxPipelineState(), 
                        meshInst,
                        transformIndex++
                    );
                }
            });
        return outSceneColor;
    }

    RenderTexture2D* Renderer::renderSceneMultiDraw(RenderableScene& renderableScene, const SceneView& sceneView, const glm::uvec2& outputResolution, const SSGITextures& SSGIOutput)
    {
        ITextureRenderable::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = outputResolution.x;
        spec.height = outputResolution.y;
        spec.pixelFormat = PF_RGB16F;
        RenderTexture2D* outSceneColor = m_renderQueue.createTexture2D("SceneColor", spec);
        m_renderQueue.addPass(
            "MainScenePass",
            [outSceneColor, &renderableScene](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addOutput(outSceneColor);
            },
            [this, outSceneColor, &renderableScene, &sceneView, &SSGIOutput]() {

                Shader* scenePassShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "ScenePassShader", SHADER_SOURCE_PATH "scene_pass_v.glsl", SHADER_SOURCE_PATH "scene_pass_p.glsl" });
                std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(outSceneColor->spec.width, outSceneColor->spec.height));
                renderTarget->setColorBuffer(outSceneColor->getTextureResource(), 0);
                renderTarget->clear({ { 0 } });

                renderableScene.submitSceneData(m_ctx);

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
                    command.count = RenderableScene::packedGeometry->submeshes[submesh].numIndices;
                    command.instanceCount = (*renderableScene.drawCalls)[draw + 1] - (*renderableScene.drawCalls)[draw];
                    command.baseInstance = 0;
                }

                glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectDrawBuffer.buffer);

                m_ctx->setShader(scenePassShader);
                m_ctx->setRenderTarget(renderTarget.get(), { { 0 } });
                m_ctx->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
                m_ctx->setDepthControl(DepthControl::kEnable);

                if (m_settings.useBentNormal)
                    scenePassShader->setUniform("useBentNormal", 1.f);
                else
                    scenePassShader->setUniform("useBentNormal", 0.f);
                auto ssao = SSGIOutput.ao->getTextureResource()->glHandle;
                auto ssbn = SSGIOutput.bentNormal->getTextureResource()->glHandle;
                if (glIsTextureHandleResidentARB(ssao) == GL_FALSE)
                {
                    glMakeTextureHandleResidentARB(ssao);
                }
                scenePassShader->setUniform("SSAO", ssao);
                if (glIsTextureHandleResidentARB(ssbn) == GL_FALSE)
                {
                    glMakeTextureHandleResidentARB(ssbn);
                }
                scenePassShader->setUniform("SSBN", ssbn);

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
                scenePassShader->setTexture("sceneLights.skyLight.irradiance", renderableScene.irradianceProbe->m_convolvedIrradianceTexture);
                scenePassShader->setTexture("sceneLights.skyLight.reflection", renderableScene.reflectionProbe->m_convolvedReflectionTexture);
                scenePassShader->setUniform("sceneLights.BRDFLookupTexture", BRDFLookupTexture);

                for (auto light : renderableScene.lights)
                {
                    light->setShaderParameters(scenePassShader);
                }
                scenePassShader->commit(m_ctx);

                // dispatch multi draw indirect
                // one sub-drawcall per instance
                u32 drawCount = (u32)renderableScene.drawCalls->getNumElements() - 1;
                glMultiDrawArraysIndirect(GL_TRIANGLES, 0, drawCount, 0);
            });

        addUIRenderCommand([](){
            ImGui::Begin("Texture Viewer");
            ImGui::Image((ImTextureID)(ReflectionProbe::getBRDFLookupTexture()->getGpuResource()), ImVec2(270, 270), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::End();
        });
        return outSceneColor;
    }

    void Renderer::ssao(const SceneView& sceneView, RenderTexture2D* sceneDepthTexture, RenderTexture2D* sceneNormalTexture, const glm::uvec2& outputResolution)
    {
        ITextureRenderable::Spec spec = sceneDepthTexture->spec;
        spec.width = outputResolution.x;
        spec.height = outputResolution.y;
        RenderTexture2D* ssaoTexture = m_renderQueue.createTexture2D("SSAOTexture", sceneDepthTexture->spec);
        m_renderQueue.addPass(
            "SSAOPass",
            [sceneDepthTexture, sceneNormalTexture, ssaoTexture](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addInput(sceneDepthTexture);
                pass->addInput(sceneNormalTexture);
                pass->addOutput(ssaoTexture);
            },
            [this, sceneDepthTexture, sceneNormalTexture, &sceneView, outputResolution, ssaoTexture]() {
                Shader* ssaoShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "SSAOShader", SHADER_SOURCE_PATH "shader_ao.vs", SHADER_SOURCE_PATH "shader_ao.fs" });
                std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(outputResolution.x, outputResolution.y));
                renderTarget->setColorBuffer(ssaoTexture->getTextureResource(), 0);

                submitFullScreenPass(
                    renderTarget.get(),
                    { { 0 } },
                    ssaoShader,
                    [this, sceneDepthTexture, sceneNormalTexture, &sceneView](RenderTarget* renderTarget, Shader* shader) {
                        shader->setTexture("normalTexture", sceneNormalTexture->getTextureResource())
                            .setTexture("depthTexture", sceneDepthTexture->getTextureResource())
                            .setUniform("cameraPos", sceneView.camera->position)
                            .setUniform("view", sceneView.camera->view())
                            .setUniform("projection", sceneView.camera->projection());
                    });
            });
    }

    Renderer::DownsampleChain Renderer::downsample(RenderTexture2D* inTexture, u32 inNumStages)
    {
        u32 numStages = min(glm::log2(inTexture->spec.width), inNumStages);
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
                pass->addOutput(bloomSetupPassData.output);
            },
            [this, bloomSetupPassData, renderTarget]() {
                Shader* bloomSetupShader = ShaderManager::createShader({ ShaderType::kVsPs, "BloomSetupShader", SHADER_SOURCE_PATH "bloom_setup_v.glsl", SHADER_SOURCE_PATH "bloom_setup_p.glsl" });
                // std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(bloomSetupPassData.output->spec.width, bloomSetupPassData.output->spec.height));
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
            ITextureRenderable::Spec spec = m_sceneColorTextureSSAA->getTextureSpec();
            m_TAAPingPongTextures[0] = AssetManager::createTexture2D("TAAPingTexture", spec);
            m_TAAPingPongTextures[1] = AssetManager::createTexture2D("TAAPongTexture", spec);
            m_TAAPingPongRenderTarget[0]->setColorBuffer(m_TAAPingPongTextures[0], 0);
            m_TAAPingPongRenderTarget[1]->setColorBuffer(m_TAAPingPongTextures[1], 0);
        }

        Shader* TAAShader = ShaderManager::createShader({ ShaderType::kVsPs, "TAAShader", SHADER_SOURCE_PATH "taa_v.glsl", SHADER_SOURCE_PATH "taa_p.glsl" });
        u32 src = max((m_numFrames - 1), 0u) % 2;
        u32 dst = m_numFrames % 2;
        submitFullScreenPass(
            m_TAAPingPongRenderTarget[dst],
            { { 0 } },
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

    RenderTexture2D* Renderer::composite(RenderTexture2D* inSceneColor, RenderTexture2D* inBloomColor, const glm::uvec2& outputResolution)
    {
        ITextureRenderable::Spec spec = inSceneColor->spec;
        spec.width = outputResolution.x;
        spec.height = outputResolution.y;
        RenderTexture2D* compositeOutput = m_renderQueue.createTexture2D("CompositedColorOutput", spec);

        // add a reconstruction pass using Gaussian filter
        auto filteredSceneColor = gaussianBlur(inSceneColor, 2u, 1.0f);

        m_renderQueue.addPass(
            "CompositePass",
            [filteredSceneColor, inBloomColor, compositeOutput](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addInput(filteredSceneColor);
                pass->addInput(inBloomColor);
                pass->addOutput(compositeOutput);
            },
            [this, filteredSceneColor, inBloomColor, compositeOutput]() {
                Shader* compositeShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "CompositeShader", SHADER_SOURCE_PATH "composite_v.glsl", SHADER_SOURCE_PATH "composite_p.glsl" });
                auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(compositeOutput->spec.width, compositeOutput->spec.height));
                renderTarget->setColorBuffer(compositeOutput->getTextureResource(), 0);

                submitFullScreenPass(
                    renderTarget.get(),
                    { { 0 } },
                    compositeShader,
                    [this, inBloomColor, filteredSceneColor](RenderTarget* renderTarget, Shader* shader) {
                        shader->setUniform("enableTonemapping", m_settings.enableTonemapping ? 1.f : 0.f);
                        shader->setUniform("tonemapOperator", m_settings.tonemapOperator);
                        shader->setUniform("exposure", m_settings.exposure)
                            .setUniform("colorTempreture", m_settings.colorTempreture)
                            .setUniform("enableBloom", m_settings.enableBloom ? 1.f : 0.f)
                            .setUniform("bloomIntensity", m_settings.bloomIntensity)
                            .setTexture("bloomColorTexture", inBloomColor->getTextureResource())
                            .setTexture("sceneColorTexture", filteredSceneColor->getTextureResource());
                    });
            }
        );
        return compositeOutput;
    }

    RenderTexture2D* Renderer::gaussianBlur(RenderTexture2D* inTexture, u32 inRadius, f32 inSigma)
    {
        static const u32 kMaxkernelRadius = 10;
        static const u32 kMinKernelRadius = 2;

        struct GaussianKernel
        {
            GaussianKernel(u32 inRadius, f32 inSigma, LinearAllocator& allocator)
                : radius(max(kMinKernelRadius, min(inRadius, kMaxkernelRadius))), sigma(inSigma)
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

#if 0
        ImGuiWindowFlags flags = ImGuiWindowFlags_None | ImGuiWindowFlags_NoResize;
        ImGui::SetNextWindowSize(ImVec2(200.f, 400.f));
        ImGui::SetNextWindowPos(ImVec2(30.f, 30.f));
        ImGui::SetNextWindowBgAlpha(0.5f);

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
#endif

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

    void Renderer::debugDrawLine(const glm::vec3& v0, const glm::vec3& v1)
    {

    }

    void Renderer::renderDebugObjects(RenderTexture2D* outTexture)
    {
    }
}