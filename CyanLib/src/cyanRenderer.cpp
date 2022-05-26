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
#include "Asset.h"
#include "CyanRenderer.h"
#include "Material.h"
#include "MathUtils.h"
#include "CyanUI.h"
#include "Ray.h"

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
        m_globalDrawData{ },
        gLighting { }, 
        gDrawDataBuffer(-1),
        m_bloomOutTexture(nullptr)
    {
        m_rasterDirectShadowManager = std::make_unique<RasterDirectShadowManager>();
    }

    Texture* Renderer::getColorOutTexture()
    {
        return m_finalColorTexture;
    }

    void Renderer::Vctx::Voxelizer::init(u32 resolution)
    {
        auto textureManager = TextureManager::get();

        TextureSpec voxelizeSpec = { };
        voxelizeSpec.width = resolution;
        voxelizeSpec.height = resolution;
        voxelizeSpec.type = Texture::Type::TEX_2D;
        voxelizeSpec.dataType = Texture::DataType::Float;
        voxelizeSpec.format = Texture::ColorFormat::R16G16B16;
        voxelizeSpec.min = Texture::Filter::LINEAR; 
        voxelizeSpec.mag = Texture::Filter::LINEAR;

        TextureSpec voxelizeSSSpec = { };
        voxelizeSSSpec.width = resolution * ssaaRes;
        voxelizeSSSpec.height = resolution * ssaaRes;
        voxelizeSSSpec.type = Texture::Type::TEX_2D;
        voxelizeSSSpec.dataType = Texture::DataType::Float;
        voxelizeSSSpec.format = Texture::ColorFormat::R16G16B16;
        voxelizeSSSpec.min = Texture::Filter::LINEAR; 
        voxelizeSSSpec.mag = Texture::Filter::LINEAR;
        Texture* voxelizeSSColorbuffer = textureManager->createTexture("VoxelizeSS", voxelizeSSSpec);
        ssRenderTarget = createRenderTarget(resolution * ssaaRes, resolution * ssaaRes);
        ssRenderTarget->setColorBuffer(voxelizeSSColorbuffer, 0);

        colorBuffer = textureManager->createTexture("VoxelizeDebug", voxelizeSpec);
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
        auto textureManager = TextureManager::get();

        TextureSpec visSpec = { };
        visSpec.width = 1280;
        visSpec.height = 720;
        visSpec.type = Texture::Type::TEX_2D;
        visSpec.dataType = Texture::DataType::Float;
        visSpec.format = Texture::ColorFormat::R16G16B16;
        visSpec.min = Texture::Filter::LINEAR; 
        visSpec.mag = Texture::Filter::LINEAR;
        visSpec.numMips = 1;
        colorBuffer = textureManager->createTexture("VoxelVis", visSpec);
        renderTarget = createRenderTarget(visSpec.width, visSpec.height);
        renderTarget->setColorBuffer(colorBuffer, 0u);
        voxelGridVisShader = ShaderManager::createShader({ ShaderType::kVsGsPs, "VoxelVisShader", SHADER_SOURCE_PATH "voxel_vis_v.glsl", SHADER_SOURCE_PATH "voxel_vis_p.glsl", SHADER_SOURCE_PATH "voxel_vis_g.glsl" });
        // voxelGridVisMatl = createMaterial(voxelGridVisShader)->createInstance();
        coneVisComputeShader = ShaderManager::createShader({ ShaderType::kCs, "ConeVisComputeShader", nullptr, nullptr, nullptr, SHADER_SOURCE_PATH "cone_vis_c.glsl" });
        coneVisDrawShader = ShaderManager::createShader({ ShaderType::kVsGsPs, "ConeVisGraphicsShader", SHADER_SOURCE_PATH "cone_vis_v.glsl", SHADER_SOURCE_PATH "cone_vis_p.glsl", SHADER_SOURCE_PATH "cone_vis_g.glsl" });

        TextureSpec depthNormSpec = { };
        depthNormSpec.type = Texture::Type::TEX_2D;
        depthNormSpec.format = Texture::ColorFormat::R32G32B32; 
        depthNormSpec.dataType = Texture::DataType::Float;
        depthNormSpec.width = 1280 * 2u;
        depthNormSpec.height = 720 * 2u;
        depthNormSpec.min = Texture::Filter::LINEAR;
        depthNormSpec.mag = Texture::Filter::LINEAR;
        depthNormSpec.s = Texture::Wrap::CLAMP_TO_EDGE;
        depthNormSpec.t = Texture::Wrap::CLAMP_TO_EDGE;
        depthNormSpec.r = Texture::Wrap::CLAMP_TO_EDGE;
        cachedSceneDepth = textureManager->createTexture("CachedSceneDepth", depthNormSpec);
        cachedSceneNormal = textureManager->createTexture("CachedSceneNormal", depthNormSpec);

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
        // m_ssaoSamplePoints.setColor(glm::vec4(0.f, 1.f, 1.f, 1.f));

        // initialize per frame shader draw data
        glCreateBuffers(1, &gDrawDataBuffer);
        glNamedBufferData(gDrawDataBuffer, sizeof(GlobalDrawData), &m_globalDrawData, GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &gLighting.dirLightSBO);
        glNamedBufferData(gLighting.dirLightSBO, gLighting.kDynamicLightBufferSize, nullptr, GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &gLighting.pointLightsSBO);
        glNamedBufferData(gLighting.pointLightsSBO, gLighting.kDynamicLightBufferSize, nullptr, GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &gInstanceTransforms.sbo);
        glNamedBufferData(gInstanceTransforms.sbo, gInstanceTransforms.kBufferSize, nullptr, GL_DYNAMIC_DRAW);

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

        auto textureManager = TextureManager::get();
        // scene render targets
        {
            enum class ColorBuffers
            {
                kColor = 0,
                kDepth,
                kNormal
            };

            // 4x super sampled buffers 
            TextureSpec colorSpec = { };
            colorSpec.type = Texture::Type::TEX_2D;
            colorSpec.format = Texture::ColorFormat::R16G16B16A16;
            colorSpec.dataType = Texture::DataType::Float;
            colorSpec.width = m_SSAAWidth;
            colorSpec.height = m_SSAAHeight;
            colorSpec.min = Texture::Filter::LINEAR;
            colorSpec.mag = Texture::Filter::LINEAR;
            colorSpec.s = Texture::Wrap::CLAMP_TO_EDGE;
            colorSpec.t = Texture::Wrap::CLAMP_TO_EDGE;
            colorSpec.r = Texture::Wrap::CLAMP_TO_EDGE;
            m_sceneColorTextureSSAA = textureManager->createTextureHDR("SceneColorTexSSAA", colorSpec);
            m_sceneColorRTSSAA = createRenderTarget(m_SSAAWidth, m_SSAAHeight);
            m_sceneColorRTSSAA->setColorBuffer(m_sceneColorTextureSSAA, static_cast<u32>(ColorBuffers::kColor));

            // non-aa buffers
            colorSpec.width = m_windowWidth;
            colorSpec.height = m_windowHeight;
            m_sceneColorTexture = textureManager->createTextureHDR("SceneColorTexture", colorSpec);
            m_sceneColorRenderTarget = createRenderTarget(m_windowWidth, m_windowHeight);
            m_sceneColorRenderTarget->setColorBuffer(m_sceneColorTexture, static_cast<u32>(ColorBuffers::kColor));

            // todo: it seems using rgb16f leads to precision issue when building ssao
            TextureSpec depthNormSpec = { };
            depthNormSpec.type = Texture::Type::TEX_2D;
            depthNormSpec.format = Texture::ColorFormat::R32G32B32; 
            depthNormSpec.dataType = Texture::DataType::Float;
            depthNormSpec.width = m_SSAAWidth;
            depthNormSpec.height = m_SSAAHeight;
            depthNormSpec.min = Texture::Filter::LINEAR;
            depthNormSpec.mag = Texture::Filter::LINEAR;
            depthNormSpec.s = Texture::Wrap::CLAMP_TO_EDGE;
            depthNormSpec.t = Texture::Wrap::CLAMP_TO_EDGE;
            depthNormSpec.r = Texture::Wrap::CLAMP_TO_EDGE;
            m_sceneDepthTextureSSAA = textureManager->createTextureHDR("SceneDepthTexSSAA", depthNormSpec);
            m_sceneNormalTextureSSAA = textureManager->createTextureHDR("SceneNormalTextureSSAA", depthNormSpec);
            m_sceneColorRTSSAA->setColorBuffer(m_sceneDepthTextureSSAA, static_cast<u32>(ColorBuffers::kDepth));
            m_sceneColorRTSSAA->setColorBuffer(m_sceneNormalTextureSSAA, static_cast<u32>(ColorBuffers::kNormal));

            depthNormSpec.width = m_windowWidth;
            depthNormSpec.height = m_windowHeight;
            m_sceneNormalTexture = textureManager->createTextureHDR("SceneNormalTexture", depthNormSpec);
            m_sceneDepthTexture = textureManager->createTextureHDR("SceneDepthTexture", depthNormSpec);
            m_sceneColorRenderTarget->setColorBuffer(m_sceneDepthTexture, static_cast<u32>(ColorBuffers::kDepth));
            m_sceneColorRenderTarget->setColorBuffer(m_sceneNormalTexture, static_cast<u32>(ColorBuffers::kNormal));
            m_sceneDepthNormalShader = ShaderManager::createShader({ ShaderType::kVsPs, "SceneDepthNormalShader", SHADER_SOURCE_PATH "scene_depth_normal_v.glsl", SHADER_SOURCE_PATH "scene_depth_normal_p.glsl" });
        }

        // ssao
        {
            TextureSpec spec = { };
            spec.width = m_windowWidth;
            spec.height = m_windowHeight;
            spec.type = Texture::Type::TEX_2D;
            spec.format = Texture::ColorFormat::R16G16B16; 
            spec.dataType = Texture::DataType::Float;
            spec.min = Texture::Filter::LINEAR;
            spec.mag = Texture::Filter::LINEAR;
            spec.s = Texture::Wrap::CLAMP_TO_EDGE;
            spec.t = Texture::Wrap::CLAMP_TO_EDGE;
            spec.r = Texture::Wrap::CLAMP_TO_EDGE;
            m_ssaoRenderTarget = createRenderTarget(spec.width, spec.height);
            m_ssaoTexture = textureManager->createTextureHDR("SSAOTexture", spec);
            m_ssaoRenderTarget->setColorBuffer(m_ssaoTexture, 0u);
            m_ssaoShader = ShaderManager::createShader({ ShaderType::kVsPs, "SSAOShader", SHADER_SOURCE_PATH "shader_ao.vs", SHADER_SOURCE_PATH "shader_ao.fs" });
        }

        // bloom
        {
            m_bloomSetupRenderTarget = createRenderTarget(m_windowWidth, m_windowHeight);
            TextureSpec spec = { };
            spec.width = m_windowWidth;
            spec.height = m_windowHeight;
            spec.type = Texture::Type::TEX_2D;
            spec.format = Texture::ColorFormat::R16G16B16A16; 
            spec.dataType = Texture::DataType::Float;
            spec.min = Texture::Filter::LINEAR;
            spec.mag = Texture::Filter::LINEAR;
            spec.s = Texture::Wrap::CLAMP_TO_EDGE;
            spec.t = Texture::Wrap::CLAMP_TO_EDGE;
            spec.r = Texture::Wrap::CLAMP_TO_EDGE;
            m_bloomSetupRenderTarget->setColorBuffer(textureManager->createTexture("BloomSetupTexture", spec), 0);
            m_bloomSetupShader = ShaderManager::createShader({ ShaderType::kVsPs, "BloomSetupShader", SHADER_SOURCE_PATH "shader_bloom_preprocess.vs", SHADER_SOURCE_PATH "shader_bloom_preprocess.fs" });
            m_bloomDsShader = ShaderManager::createShader({ ShaderType::kVsPs, "BloomDownSampleShader", SHADER_SOURCE_PATH "shader_downsample.vs", SHADER_SOURCE_PATH "shader_downsample.fs" });
            m_bloomUsShader = ShaderManager::createShader({ ShaderType::kVsPs, "UpSampleShader", SHADER_SOURCE_PATH "shader_upsample.vs", SHADER_SOURCE_PATH "shader_upsample.fs" });
            m_gaussianBlurShader = ShaderManager::createShader({ ShaderType::kVsPs, "GaussianBlurShader", SHADER_SOURCE_PATH "shader_gaussian_blur.vs", SHADER_SOURCE_PATH "shader_gaussian_blur.fs" });

            u32 numBloomTextures = 0u;
            auto initBloomBuffers = [&](u32 index, TextureSpec& spec) {
                m_bloomDsTargets[index].renderTarget = createRenderTarget(spec.width, spec.height);
                char buff[64];
                sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
                m_bloomDsTargets[index].src = textureManager->createTextureHDR(buff, spec);
                sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
                m_bloomDsTargets[index].scratch = textureManager->createTextureHDR(buff, spec);

                m_bloomUsTargets[index].renderTarget = createRenderTarget(spec.width, spec.height);
                sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
                m_bloomUsTargets[index].src = textureManager->createTextureHDR(buff, spec);
                sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
                m_bloomUsTargets[index].scratch = textureManager->createTextureHDR(buff, spec);
                spec.width /= 2;
                spec.height /= 2;
            };

            for (u32 i = 0u; i < kNumBloomPasses; ++i) {
                initBloomBuffers(i, spec);
            }
        }

        // composite
        {
            TextureSpec colorSpec = { };
            colorSpec.type = Texture::Type::TEX_2D;
            colorSpec.format = Texture::ColorFormat::R16G16B16A16;
            colorSpec.dataType = Texture::DataType::Float;
            colorSpec.width = m_windowWidth;
            colorSpec.height = m_windowHeight;
            colorSpec.min = Texture::Filter::LINEAR;
            colorSpec.mag = Texture::Filter::LINEAR;
            colorSpec.s = Texture::Wrap::CLAMP_TO_EDGE;
            colorSpec.t = Texture::Wrap::CLAMP_TO_EDGE;
            colorSpec.r = Texture::Wrap::CLAMP_TO_EDGE;
            m_compositeColorTexture = textureManager->createTextureHDR("CompositeColorTexture", colorSpec);
            m_compositeRenderTarget = createRenderTarget(colorSpec.width, colorSpec.height);
            m_compositeRenderTarget->setColorBuffer(m_compositeColorTexture, 0u);
            m_compositeShader = ShaderManager::createShader({ ShaderType::kVsPs, "CompositeShader", SHADER_SOURCE_PATH "composite_v.glsl", SHADER_SOURCE_PATH "composite_p.glsl" });
            // m_compositeMatl = createMaterial(m_compositeShader)->createInstance();
        }

        // voxel cone tracing
        {
            m_vctx.voxelizer.init(m_sceneVoxelGrid.resolution);
            m_vctx.visualizer.init();

            {
                TextureSpec voxelDataSpec = { };
                voxelDataSpec.width = m_sceneVoxelGrid.resolution;
                voxelDataSpec.height = m_sceneVoxelGrid.resolution;
                voxelDataSpec.depth = m_sceneVoxelGrid.resolution;
                voxelDataSpec.type = Texture::Type::TEX_3D;
                voxelDataSpec.dataType = Texture::DataType::UNSIGNED_INT;
                voxelDataSpec.format = Texture::ColorFormat::R8G8B8A8;
                voxelDataSpec.min = Texture::Filter::MIPMAP_LINEAR; 
                voxelDataSpec.mag = Texture::Filter::NEAREST;
                voxelDataSpec.r = Texture::Wrap::CLAMP_TO_EDGE;
                voxelDataSpec.s = Texture::Wrap::CLAMP_TO_EDGE;
                voxelDataSpec.t = Texture::Wrap::CLAMP_TO_EDGE;
                m_sceneVoxelGrid.normal = textureManager->createTexture3D("VoxelGridNormal", voxelDataSpec);
                m_sceneVoxelGrid.emission = textureManager->createTexture3D("VoxelGridEmission", voxelDataSpec);

                voxelDataSpec.numMips = std::log2(m_sceneVoxelGrid.resolution) + 1;
                m_sceneVoxelGrid.albedo = textureManager->createTexture3D("VoxelGridAlbedo", voxelDataSpec);
                m_sceneVoxelGrid.radiance = textureManager->createTexture3D("VoxelGridRadiance", voxelDataSpec);

                voxelDataSpec.dataType = Texture::DataType::Float;
                voxelDataSpec.format = Texture::ColorFormat::R32F;
                m_sceneVoxelGrid.opacity = textureManager->createTexture3D("VoxelGridOpacity", voxelDataSpec);
            }

            {
                using ColorBuffers = Vctx::ColorBuffers;

                TextureSpec spec = { };
                spec.width = 1280;
                spec.height = 720;
                spec.type = Texture::Type::TEX_2D;
                spec.dataType = Texture::DataType::Float;
                spec.format = Texture::ColorFormat::R16G16B16;
                spec.min = Texture::Filter::LINEAR;
                spec.mag = Texture::Filter::LINEAR;

                m_vctx.occlusion = textureManager->createTexture("VctxOcclusion", spec);
                m_vctx.irradiance = textureManager->createTexture("VctxIrradiance", spec);
                m_vctx.reflection = textureManager->createTexture("VctxReflection", spec);

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

    void Renderer::drawMeshInstance(RenderTarget* renderTarget, std::initializer_list<i32>&& drawBuffers, Viewport viewport, GfxPipelineState pipelineState, MeshInstance* meshInstance, i32 transformIndex)
    {
        Mesh* parent = meshInstance->parent;
        for (u32 i = 0; i < parent->numSubmeshes(); ++i)
        {
            IMaterial* material = meshInstance->getMaterial(i);
            if (material)
            {
                RenderTask task = { };
                task.renderTarget = renderTarget;
                task.drawBuffers = std::move(drawBuffers);
                task.viewport = viewport;
                task.shader = material->getMaterialShader();
                task.renderSetupLambda = [this, transformIndex, material](RenderTarget* renderTarget, Shader* shader) {
                    material->setShaderParameters();
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
        glClearTexImage(m_sceneVoxelGrid.albedo->handle, 0, GL_RGBA, GL_UNSIGNED_INT, 0);
        glClearTexImage(m_sceneVoxelGrid.normal->handle, 0, GL_RGBA, GL_UNSIGNED_INT, 0);
        glClearTexImage(m_sceneVoxelGrid.radiance->handle, 0, GL_RGBA, GL_UNSIGNED_INT, 0);
        glClearTexImage(m_sceneVoxelGrid.opacity->handle, 0, GL_R, GL_FLOAT, 0);

        enum class ImageBindings
        {
            kAlbedo = 0,
            kNormal,
            kEmission,
            kRadiance,
            kOpacity
        };
        // bind 3D volume texture to image units
        glBindImageTexture(static_cast<u32>(ImageBindings::kAlbedo), m_sceneVoxelGrid.albedo->handle, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(static_cast<u32>(ImageBindings::kNormal), m_sceneVoxelGrid.normal->handle, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(static_cast<u32>(ImageBindings::kRadiance), m_sceneVoxelGrid.radiance->handle, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(static_cast<u32>(ImageBindings::kOpacity), m_sceneVoxelGrid.opacity->handle, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32F);
        if (m_vctx.opts.useSuperSampledOpacity > 0.f)
        {
            enum class SsboBindings
            {
                kOpacityMask = (i32)GlobalBufferBindings::kCount,
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
        renderTarget->clear({ 0 });
        glEnable(GL_NV_conservative_raster);
        glDisable(GL_CULL_FACE);

        for (i32 axis = (i32)Steps::kXAxis; axis < (i32)Steps::kCount; ++axis)
        {
            for (u32 i = 0; i < scene->entities.size(); ++i)
            {
                executeOnEntity(scene->entities[i], [this, voxelizer, pmin, pmax, axis, renderTarget](SceneComponent* node) {
                    if (MeshInstance* meshInstance = node->getAttachedMesh())
                    {
                        for (u32 i = 0; i < meshInstance->parent->numSubmeshes(); ++i)
                        {
                            auto sm = meshInstance->parent->getSubmesh(i);

                            RenderTask task = { };
                            task.renderTarget = renderTarget;
                            task.drawBuffers = { 0 };
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
                                    Texture* albedo = matl->parameter.albedo;
                                    Texture* normal = matl->parameter.normal;
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
                kOpacityMask = (i32)GlobalBufferBindings::kCount,
            };
            auto index = glGetProgramResourceIndex(m_vctx.resolveShader->getGpuResource(), GL_SHADER_STORAGE_BLOCK, "OpacityData");
            glShaderStorageBlockBinding(m_vctx.resolveShader->getGpuResource(), index, (u32)SsboBindings::kOpacityMask);
            m_ctx->setShader(m_vctx.resolveShader);
            glDispatchCompute(m_sceneVoxelGrid.resolution, m_sceneVoxelGrid.resolution, m_sceneVoxelGrid.resolution);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }

        if (m_opts.autoFilterVoxelGrid)
        {
            glGenerateTextureMipmap(m_sceneVoxelGrid.albedo->handle);
            glGenerateTextureMipmap(m_sceneVoxelGrid.radiance->handle);
            glGenerateTextureMipmap(m_sceneVoxelGrid.opacity->handle);
        }
        // manually filter & down-sample to generate mipmap with anisotropic voxels
        else
        {
            m_ctx->setShader(voxelizer.filterVoxelGridShader);

            const i32 kNumDownsample = 1u;
            for (i32 i = 0; i < kNumDownsample; ++i)
            {
                i32 textureUnit = (i32)GlobalTextureBindings::kCount;
                glm::ivec3 currGridDim = glm::ivec3(m_sceneVoxelGrid.resolution) / (i32)pow(2, i+1);
                voxelizer.filterVoxelGridShader->setUniform("srcMip", i);
                voxelizer.filterVoxelGridShader->setUniform("srcAlbedo", textureUnit);
                glBindTextureUnit(textureUnit++, m_sceneVoxelGrid.albedo->handle);

                voxelizer.filterVoxelGridShader->setUniform("dstAlbedo", textureUnit);
                glBindImageTexture(textureUnit++, m_sceneVoxelGrid.albedo->handle, i+1, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
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
        glBindTextureUnit(gTexBinding(VoxelGridAlbedo), m_sceneVoxelGrid.albedo->handle);
        glBindTextureUnit(gTexBinding(VoxelGridNormal), m_sceneVoxelGrid.normal->handle);
        glBindTextureUnit(gTexBinding(VoxelGridRadiance), m_sceneVoxelGrid.radiance->handle);
        glBindTextureUnit(gTexBinding(VoxelGridOpacity), m_sceneVoxelGrid.opacity->handle);

        // update buffer data
        glm::vec3 pmin, pmax;
        calcSceneVoxelGridAABB(scene->aabb, pmin, pmax);
        m_vctxGpuData.localOrigin = glm::vec3(pmin.x, pmin.y, pmax.z);
        m_vctxGpuData.voxelSize = (pmax.x - pmin.x) / (f32)m_sceneVoxelGrid.resolution;
        m_vctxGpuData.visMode = 1 << static_cast<i32>(m_vctx.visualizer.mode);
        glNamedBufferSubData(m_vctxSsbo, 0, sizeof(VctxGpuData), &m_vctxGpuData);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, gBufferBinding(VctxGlobalData), m_vctxSsbo);
    }

    void Renderer::visualizeVoxelGrid()
    {
        glDisable(GL_CULL_FACE);
        {
            m_ctx->setRenderTarget(m_vctx.visualizer.renderTarget, { 0 });
            m_vctx.visualizer.renderTarget->clear({ 0 });
            m_ctx->setViewport({0u, 0u, m_vctx.visualizer.renderTarget->width, m_vctx.visualizer.renderTarget->height});

            m_ctx->setShader(m_vctx.visualizer.voxelGridVisShader);
            m_vctx.visualizer.voxelGridVisShader->setUniform("activeMip", m_vctx.visualizer.activeMip);
            // m_vctx.visualizer.voxelGridVisMatl->set("activeMip", m_vctx.visualizer.activeMip);
            // m_vctx.visualizer.voxelGridVisMatl->setShaderParameters();

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
        auto sceneDepthTexture = m_opts.enableAA ? m_sceneDepthTextureSSAA : m_sceneDepthTexture;
        auto sceneNormalTexture = m_opts.enableAA ? m_sceneNormalTextureSSAA : m_sceneNormalTexture;
        if (!visualizer.cachedTexInitialized || visualizer.debugScreenPosMoved)
        {
            if (visualizer.cachedSceneDepth->width == sceneDepthTexture->width && visualizer.cachedSceneDepth->height == sceneDepthTexture->height &&
                visualizer.cachedSceneNormal->width == sceneNormalTexture->width && visualizer.cachedSceneNormal->height == sceneNormalTexture->height)
            {
                // update cached snapshot of scene depth/normal by copying texture data
                glCopyImageSubData(sceneDepthTexture->handle, GL_TEXTURE_2D, 0, 0, 0, 0, 
                    visualizer.cachedSceneDepth->handle, GL_TEXTURE_2D, 0, 0, 0, 0, sceneDepthTexture->width, sceneDepthTexture->height, 1);
                glCopyImageSubData(sceneNormalTexture->handle, GL_TEXTURE_2D, 0, 0, 0, 0, 
                    visualizer.cachedSceneNormal->handle, GL_TEXTURE_2D, 0, 0, 0, 0, sceneNormalTexture->width, sceneNormalTexture->height, 1);


                visualizer.cachedView = m_globalDrawData.view;
                visualizer.cachedProjection = m_globalDrawData.projection;

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
        glBindTextureUnit((u32)TexBindings::kSceneDepth, m_vctx.visualizer.cachedSceneDepth->handle);
        glBindTextureUnit((u32)TexBindings::kSceneNormal, m_vctx.visualizer.cachedSceneNormal->handle);

        glDispatchCompute(1, 1, 1);

        // sync
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // visualize traced cones
        m_ctx->setRenderTarget(m_vctx.visualizer.renderTarget, { 0 });
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

    void Renderer::drawEntity(RenderTarget* renderTarget, std::initializer_list<i32>&& drawBuffers, Viewport viewport, GfxPipelineState pipelineState, Entity* entity)
    {
        std::queue<SceneComponent*> nodes;
        nodes.push(entity->m_sceneRoot);
        while (!nodes.empty())
        {
            auto node = nodes.front();
            nodes.pop();
            if (auto meshInst = node->getAttachedMesh())
            {
                drawMeshInstance(renderTarget, std::move(drawBuffers), viewport, pipelineState, meshInst, node->globalTransform);
            }
            for (u32 i = 0; i < node->m_child.size(); ++i)
            {
                nodes.push(node->m_child[i]);
            }
        }
    }

    void Renderer::submitMesh(RenderTarget* renderTarget, std::initializer_list<i32>&& drawBuffers, Viewport viewport, GfxPipelineState pipelineState, Mesh* mesh, Shader* shader, const RenderSetupLambda& perMeshSetupLambda)
    {
        perMeshSetupLambda(renderTarget, shader);

        for (u32 i = 0; i < mesh->numSubmeshes(); ++i)
        {
            RenderTask task = { };
            task.renderTarget = renderTarget;
            task.drawBuffers = std::move(drawBuffers);
            task.viewport = viewport;
            task.shader = shader;
            task.submesh = mesh->getSubmesh(i);

            submitRenderTask(std::move(task));
        }
    }

    void Renderer::submitMaterialMesh(RenderTarget* renderTarget, std::initializer_list<i32>&& drawBuffers, Viewport viewport, GfxPipelineState pipelineState, Mesh* mesh, IMaterial* material, const RenderSetupLambda& perMeshSetupLambda)
    {
        if (material)
        {
            material->setShaderParameters();
            Shader* shader = material->getMaterialShader();
            perMeshSetupLambda(renderTarget, shader);

            for (u32 i = 0; i < mesh->numSubmeshes(); ++i)
            {
                RenderTask task = { };
                task.renderTarget = renderTarget;
                task.drawBuffers = std::move(drawBuffers);
                task.viewport = viewport;
                task.shader = shader;
                task.submesh = mesh->getSubmesh(i);

                submitRenderTask(std::move(task));
            }
        }
    }

    void Renderer::submitFullScreenPass(RenderTarget* renderTarget, std::initializer_list<i32>&& drawBuffers, Shader* shader, const RenderSetupLambda& renderSetupLambda)
    {
        GfxPipelineState pipelineState;
        pipelineState.depth = DepthControl::kDisable;

        submitMesh(
            renderTarget,
            std::move(drawBuffers),
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

    template <typename T>
    u32 sizeofVector(const std::vector<T>& vec)
    {
        if (vec.size() == 0)
        {
            return 0;
        }
        return sizeof(vec[0]) * (u32)vec.size();
    }

    /*
        * render passes should be pushed after call to beginRender() 
    */
    void Renderer::beginRender()
    {
        // todo: broadcast beginRender event 

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

        /**
        * todo: convert Scene to RenderableScene if cached RenderableScene is outdated
        */
        m_renderableScene.buildFromScene(scene);

        beginRender();
        {
            // update all global data
            updateFrameGlobalData(scene, scene->camera);

            // sun shadow pass
            if (m_opts.enableSunShadow)
            {
                renderSunShadow(scene, scene->entities);
            }

            // scene depth & normal pass
            bool depthNormalPrepassRequired = (m_opts.enableSSAO || m_opts.enableVctx);
            if (depthNormalPrepassRequired)
            {
                renderSceneDepthNormal(scene);
            }

            // voxel cone tracing pass
            if (m_opts.enableVctx)
            {
                // todo: revoxelizing the scene every frame is causing flickering in the volume texture
                voxelizeScene(scene);
                renderVctx();
            }

            // ssao pass
            if (m_opts.enableSSAO)
            {
                ssao(scene->camera);
            }

            // main scene pass
            renderScene(scene);

            // debug object pass
            renderDebugObjects(scene, externDebugRender);

            // post processing
            {
                // reset state
                m_bloomOutTexture = nullptr;
                m_finalColorTexture = m_opts.enableAA ? m_sceneColorTextureSSAA : m_sceneColorTexture;

                if (m_opts.enableBloom)
                {
                    bloom();
                }
                composite();
            }
        }
    }

    void Renderer::endRender()
    {
        // reset render target
        m_ctx->setRenderTarget(nullptr, {});
        m_ctx->flip();
    }

    void Renderer::executeOnEntity(Entity* e, const std::function<void(SceneComponent*)>& func)
    {
        std::queue<SceneComponent*> nodes;
        nodes.push(e->m_sceneRoot);
        while (!nodes.empty())
        {
            auto node = nodes.front();
            nodes.pop();
            func(node);
            for (u32 i = 0; i < (u32)node->m_child.size(); ++i)
                nodes.push(node->m_child[i]);
        }
    }

    // Data that needs to be updated on frame start, such as transforms, and lighting
    void Renderer::updateFrameGlobalData(Scene* scene, const Camera& camera)
    {
        // bind global draw data
        m_globalDrawData.view = camera.view;
        m_globalDrawData.projection = camera.projection;
        m_globalDrawData.numPointLights = (i32)scene->pointLights.size();
        m_globalDrawData.numDirLights = (i32)scene->dLights.size();
        m_globalDrawData.ssao = m_opts.enableSSAO ? 1.f : 0.f;

        updateTransforms(scene);
        // bind lighting data
        updateLighting(scene);

        // bind global textures
        m_ctx->setPersistentTexture(m_ssaoTexture, gTexBinding(SSAO));

        // upload data to gpu
        glNamedBufferSubData(gDrawDataBuffer, 0, sizeof(GlobalDrawData), &m_globalDrawData);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<u32>(GlobalBufferBindings::DrawData), gDrawDataBuffer);
    }

    void Renderer::updateTransforms(Scene* scene)
    {
        const std::vector<glm::mat4>& matrices = scene->globalTransformMatrixPool.getObjects();
        if (sizeofVector(matrices) > gInstanceTransforms.kBufferSize)
        {
            cyanError("Gpu global transform SBO overflow!");
        }
        glNamedBufferSubData(gInstanceTransforms.sbo, 0, sizeofVector(matrices), matrices.data());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)GlobalBufferBindings::GlobalTransforms, gInstanceTransforms.sbo);
    }

    void Renderer::updateLighting(Scene* scene)
    {
        gLighting.dirLights.clear();
        gLighting.pointLights.clear();
        for (u32 i = 0; i < scene->dLights.size(); ++i)
        {
            scene->dLights[i].update();
            gLighting.dirLights.emplace_back(scene->dLights[i].getData());
        }
        for (u32 i = 0; i < scene->pointLights.size(); ++i)
        {
            scene->pointLights[i].update();
            gLighting.pointLights.emplace_back(scene->pointLights[i].getData());
        }
        glNamedBufferSubData(gLighting.dirLightSBO, 0, sizeofVector(gLighting.dirLights), gLighting.dirLights.data());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gLighting.dirLightSBO);
        glNamedBufferSubData(gLighting.pointLightsSBO, 0, sizeofVector(gLighting.pointLights), gLighting.pointLights.data());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gLighting.pointLightsSBO);
        gLighting.irradianceProbe = scene->m_irradianceProbe;
        gLighting.reflectionProbe = scene->m_reflectionProbe;

        // shared BRDF lookup texture used in split sum approximation for image based lighting
        if (Texture* BRDFLookupTexture = ReflectionProbe::getBRDFLookupTexture())
        {
            m_ctx->setPersistentTexture(BRDFLookupTexture, gTexBinding(BRDFLookupTexture));
        }

        // skybox
        if (gLighting.skybox)
        {
            m_ctx->setPersistentTexture(gLighting.skybox->getDiffueTexture(), 0);
            m_ctx->setPersistentTexture(gLighting.skybox->getSpecularTexture(), 1);
        }

        // additional light probes
        if (gLighting.irradianceProbe)
        {
            m_ctx->setPersistentTexture(gLighting.irradianceProbe->m_convolvedIrradianceTexture, 3);
        }

        if (gLighting.reflectionProbe)
        {
            m_ctx->setPersistentTexture(gLighting.reflectionProbe->m_convolvedReflectionTexture, 4);
        }
    }

    void Renderer::updateSunShadow(const CascadedShadowmap& csm)
    {
        m_globalDrawData.sunLightView   = csm.lightView;
        // bind sun light shadow map for this frame
        u32 textureUnitOffset = gTexBinding(SunShadow);
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
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<u32>(GlobalBufferBindings::DrawData), gDrawDataBuffer);
    }

    void Renderer::renderSunShadow(Scene* scene, const std::vector<Entity*>& shadowCasters)
    {
        auto& sunLight = scene->dLights[0];
        m_rasterDirectShadowManager->render(m_csm, scene, sunLight, shadowCasters);
        updateSunShadow(m_csm);
    }

    void Renderer::renderSceneDepthNormal(Scene* scene)
    {
        enum class ColorBuffers
        {
            kColor = 0,
            kDepth,
            kNormal
        };
        enum class DrawBuffers
        {
            kDepth = 0,
            kNormal
        };

        auto renderTarget = m_opts.enableAA ? m_sceneColorRTSSAA : m_sceneColorRenderTarget;
        renderTarget->clear({ static_cast<i32>(DrawBuffers::kDepth) }, glm::vec4(1.f));
        renderTarget->clear({ static_cast<i32>(DrawBuffers::kNormal) });

        for (u32 e = 0; e < (u32)scene->entities.size(); ++e)
        {
            auto entity = scene->entities[e];
            if (entity->m_includeInGBufferPass && entity->m_visible)
            {
                executeOnEntity(entity, [this, renderTarget](SceneComponent* sceneComponent) { 
                    if (auto meshInst = sceneComponent->getAttachedMesh())
                    {
                        submitMesh(
                            renderTarget, // renderTarget
                            { static_cast<i32>(ColorBuffers::kDepth), static_cast<i32>(ColorBuffers::kNormal) }, // drawBuffers
                            { 0u, 0u, renderTarget->width, renderTarget->height }, // viewport
                            GfxPipelineState(), // pipeline state
                            meshInst->parent, // mesh
                            m_sceneDepthNormalShader, // shader
                            [sceneComponent](RenderTarget* renderTarget, Shader* shader) { // renderSetupLambda
                                shader->setUniform("transformIndex", sceneComponent->globalTransform);
                            });
                    }
                });
            }
        }
    }

    void Renderer::renderScene(Scene* scene)
    {
        // draw skybox
        if (scene->m_skybox)
        {
            scene->m_skybox->render();
        }

        auto renderTarget = m_opts.enableAA ? m_sceneColorRTSSAA : m_sceneColorRenderTarget;
        renderTarget->clear({ 0 });

        // draw entities
        for (u32 i = 0; i < scene->entities.size(); ++i)
        {
            if (scene->entities[i]->m_visible)
            {
                executeOnEntity(scene->entities[i], 
                    [this, renderTarget](SceneComponent* node) {
                        if (auto meshInst = node->getAttachedMesh())
                        {
                            drawMeshInstance(renderTarget, { 0 }, { 0u, 0u, renderTarget->width, renderTarget->height }, GfxPipelineState(), meshInst, node->globalTransform);
                        }
                    });
            }
        }
    }

    void Renderer::ssao(Camera& camera)
    {
        submitFullScreenPass(
            m_ssaoRenderTarget, 
            { 0 }, 
            m_ssaoShader, 
            [this, camera](RenderTarget* renderTarget, Shader* shader) {
                renderTarget->clear({ 0 }, glm::vec4(1.f));
                shader->setTexture("normalTexture", m_opts.enableAA ? m_sceneNormalTextureSSAA : m_sceneNormalTexture)
                    .setTexture("depthTexture", m_opts.enableAA ? m_sceneDepthTextureSSAA : m_sceneDepthTexture)
                    .setUniform("cameraPos", camera.position)
                    .setUniform("view", camera.view)
                    .setUniform("projection", camera.projection);
            });
    }

    void Renderer::bloom()
    {
        // be aware that these functions modify 'renderTarget''s state
        auto downsample = [this](Texture* dst, Texture* src, RenderTarget* renderTarget) {
            enum class ColorBuffer
            {
                kDst = 0
            };

            renderTarget->setColorBuffer(dst, static_cast<u32>(ColorBuffer::kDst));
            submitFullScreenPass(
                renderTarget, 
                { static_cast<u32>(ColorBuffer::kDst) },
                m_bloomDsShader, 
                [this, src](RenderTarget* renderTarget, Shader* shader) {
                    shader->setTexture("srcImage", src);
                });
        };

        auto upscale = [this](Texture* dst, Texture* src, Texture* blend, RenderTarget* renderTarget, u32 stageIndex) {
            enum class ColorBuffer
            {
                kDst = 0
            };

            renderTarget->setColorBuffer(dst, static_cast<u32>(ColorBuffer::kDst));
            submitFullScreenPass(
                renderTarget, 
                { static_cast<u32>(ColorBuffer::kDst) },
                m_bloomUsShader, 
                [this, src, blend, stageIndex](RenderTarget* renderTarget, Shader* shader) {
                    shader->setTexture("srcImage", src)
                        .setTexture("blendImage", blend)
                        .setUniform("stageIndex", stageIndex);
                });
        };

        // user have to make sure that renderTarget is compatible with 'dst', 'src', and 'scratch'
        auto gaussianBlur = [this](Texture* dst, Texture* src, Texture* scratch, RenderTarget* renderTarget, i32 kernelIndex, i32 radius) {

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
                    { static_cast<u32>(ColorBuffer::kScratch) },
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
                std::initializer_list<i32> drawBuffers;
                if (dst)
                {
                    drawBuffers = { static_cast<i32>(ColorBuffer::kDst) };
                }
                else
                {
                    drawBuffers = { static_cast<i32>(ColorBuffer::kSrc) };
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

        auto* src = m_opts.enableAA ? m_sceneColorTextureSSAA : m_sceneColorTexture;

        // bloom setup pass
        submitFullScreenPass(
            m_bloomSetupRenderTarget, 
            { 0u },
            m_bloomSetupShader, 
            [this, src](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcTexture", src);
            });

        // downsample
        i32 kernelRadii[6] = { 3, 4, 6, 7, 8, 9};
        downsample(m_bloomDsTargets[0].src, m_bloomSetupRenderTarget->getColorBuffer(0u), m_bloomDsTargets[0].renderTarget);
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
            { 0u },
            m_compositeShader,
            [this](RenderTarget* renderTarget, Shader* shader) {
                shader->setUniform("exposure", m_opts.exposure)
                    .setUniform("bloom", m_opts.enableBloom ? 1.f : 0.f)
                    .setUniform("bloomIntensity", m_opts.bloomIntensity)
                    .setTexture("bloomOutTexture", m_bloomOutTexture)
                    .setTexture("sceneColorTexture", m_opts.enableAA ? m_sceneColorTextureSSAA : m_sceneColorTexture);
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
            ImGui::Image((ImTextureID)m_vctx.visualizer.colorBuffer->handle, visSize, ImVec2(0, 1), ImVec2(1, 0));

            ImGui::Separator();
            ImGui::Text("Vctx Occlusion");
            ImGui::Image((ImTextureID)m_vctx.occlusion->handle, visSize, ImVec2(0, 1), ImVec2(1, 0));

            ImGui::Separator();
            ImGui::Text("Vctx Irradiance");
            ImGui::Image((ImTextureID)m_vctx.irradiance->handle, visSize, ImVec2(0, 1), ImVec2(1, 0));
        }
        ImGui::End();

        // end imgui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // todo: refactor this, this should really just be renderScene()
    void Renderer::renderSceneToLightProbe(Scene* scene, LightProbe* probe, RenderTarget* renderTarget)
    {
        updateTransforms(scene);

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
        m_opts.enableSSAO = false;

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

            updateFrameGlobalData(scene, camera);
            // draw skybox
            if (scene->m_skybox)
            {
                scene->m_skybox->render();
            }
            renderTarget->clear({ (i32)f });
            // draw entities 
            for (u32 i = 0; i < staticObjects.size(); ++i)
            {
                drawEntity(
                    renderTarget, 
                    { (i32)f, -1, -1, -1 },
                    { 0u, 0u, probe->sceneCapture->width, probe->sceneCapture->height }, 
                    GfxPipelineState(), 
                    staticObjects[i]);
            }
        }

        m_opts.enableSSAO = true;
    }

    void Renderer::renderVctx()
    {
        using ColorBuffers = Vctx::ColorBuffers;

        m_ctx->setRenderTarget(m_vctx.renderTarget, { (i32)ColorBuffers::kOcclusion, (i32)ColorBuffers::kIrradiance, (i32)ColorBuffers::kReflection });
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
        auto depthTexture = m_opts.enableAA ? m_sceneDepthTextureSSAA : m_sceneDepthTexture;
        auto normalTexture = m_opts.enableAA ? m_sceneNormalTextureSSAA : m_sceneNormalTexture;
        glBindTextureUnit((u32)TexBindings::kDepth, depthTexture->handle);
        glBindTextureUnit((u32)TexBindings::kNormal, normalTexture->handle);
        m_ctx->setPrimitiveType(PrimitiveMode::TriangleList);
        m_ctx->setDepthControl(DepthControl::kDisable);
        m_ctx->drawIndexAuto(6u);
        m_ctx->setDepthControl(DepthControl::kEnable);

        // bind textures for rendering
        glBindTextureUnit(gTexBinding(VctxOcclusion), m_vctx.occlusion->handle);
        glBindTextureUnit(gTexBinding(VctxIrradiance), m_vctx.irradiance->handle);
        glBindTextureUnit(gTexBinding(VctxReflection), m_vctx.reflection->handle);
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