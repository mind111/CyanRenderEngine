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
#include "CyanRenderer.h"
#include "Material.h"
#include "MathUtils.h"
#include "CyanUI.h"
#include "Ray.h"

#define DRAW_SSAO_DEBUG_VIS 0

bool fileWasModified(const char* fileName, FILETIME* writeTime)
{
    FILETIME lastWriteTime;
    HANDLE hFile = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
    // If the file is being written to then CreatFile will return a invalid handle.
    if (hFile != INVALID_HANDLE_VALUE)
    {
        GetFileTime(hFile, 0, 0, &lastWriteTime);
        CloseHandle(hFile);
        if (CompareFileTime(&lastWriteTime, writeTime) > 0)
        {
            *writeTime = lastWriteTime;
            return true;
        }
    }
    return false;
}

namespace Cyan
{
    float quadVerts[24] = {
        -1.f, -1.f, 0.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f,

        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
         1.f,  1.f, 1.f, 1.f
    };

    static QuadMesh s_quad = { };

    QuadMesh* Renderer::getQuadMesh()
    {
        if (!s_quad.m_vertexArray)
        {
            s_quad.init(glm::vec2(0.f), glm::vec2(1.f, 1.f));
        }
        return &s_quad;
    }

    // static singleton pointer will be set to 0 before main() get called
    Renderer* Renderer::m_renderer = 0;

    Renderer::Renderer(GLFWwindow* window, const glm::vec2& windowSize)
        : m_frameAllocator(1024u * 1024u),  // 1 megabytes
        m_ctx(nullptr),
        m_windowWidth(windowSize.x),
        m_windowHeight(windowSize.y),
        m_SSAAWidth(2u * windowSize.x),
        m_SSAAHeight(2u * windowSize.y),
        m_sceneColorTexture(nullptr),
        m_sceneColorRenderTarget(nullptr),
        m_sceneColorTextureSSAA(nullptr),
        m_sceneColorRTSSAA(nullptr),
        m_ssaoSamplePoints(32),
        m_globalDrawData{ },
        gLighting { }, 
        gDrawDataBuffer(-1),
        m_bloomOutTexture(nullptr)
    {
        if (!m_renderer)
        {
            m_renderer = this;
            m_renderer->initialize(window, windowSize);
        }
        else
        {
            CYAN_ASSERT(0, "There should be only one instance of Renderer")
        }
    }

    Renderer* Renderer::getSingletonPtr()
    {
        return m_renderer;
    }

    StackAllocator& Renderer::getAllocator()
    {
        return m_frameAllocator;
    }

    Texture* Renderer::getColorOutTexture()
    {
        return m_finalColorTexture;
    }

    void Renderer::initShaders()
    {
        m_lumHistogramShader = glCreateShader(GL_COMPUTE_SHADER);
        const char* src = ShaderUtil::readShaderFile( SHADER_SOURCE_PATH "shader_lumin_histogram_c.glsl");
        glShaderSource(m_lumHistogramShader, 1, &src, nullptr);
        glCompileShader(m_lumHistogramShader);
        ShaderUtil::checkShaderCompilation(m_lumHistogramShader);
        m_lumHistogramProgram = glCreateProgram();
        glAttachShader(m_lumHistogramProgram, m_lumHistogramShader);
        glLinkProgram(m_lumHistogramProgram);
        ShaderUtil::checkShaderLinkage(m_lumHistogramProgram);
    }

    void Renderer::initialize(GLFWwindow* window, glm::vec2 windowSize)
    {
        m_ctx = getCurrentGfxCtx();
        m_ssaoSamplePoints.setColor(glm::vec4(0.f, 1.f, 1.f, 1.f));

        // initialize per frame shader draw data
        glCreateBuffers(1, &gDrawDataBuffer);
        glNamedBufferData(gDrawDataBuffer, sizeof(GlobalDrawData), &m_globalDrawData, GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &gLighting.dirLightSBO);
        glNamedBufferData(gLighting.dirLightSBO, gLighting.kDynamicLightBufferSize, nullptr, GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &gLighting.pointLightsSBO);
        glNamedBufferData(gLighting.pointLightsSBO, gLighting.kDynamicLightBufferSize, nullptr, GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &gInstanceTransforms.SBO);
        glNamedBufferData(gInstanceTransforms.SBO, gInstanceTransforms.kBufferSize, nullptr, GL_DYNAMIC_DRAW);

        // quad mesh
        s_quad.init(glm::vec2(0.f), glm::vec2(1.f, 1.f));

        // shadow
        m_shadowmapManager.initShadowmap(m_csm, glm::uvec2(4096u, 4096u));

        // set back-face culling
        m_ctx->setCullFace(FrontFace::CounterClockWise, FaceCull::Back);

        // create shaders
        initShaders();

        auto textureManager = TextureManager::getSingletonPtr();
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
            m_sceneDepthNormalShader = createShader("SceneDepthNormalShader", SHADER_SOURCE_PATH "scene_depth_normal_v.glsl", SHADER_SOURCE_PATH "scene_depth_normal_p.glsl");
        }

        // ssao
        {
            m_freezeDebugLines = false;
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
            m_ssaoShader = createShader("SSAOShader", SHADER_SOURCE_PATH "shader_ao.vs", SHADER_SOURCE_PATH "shader_ao.fs");
            m_ssaoMatl = createMaterial(m_ssaoShader)->createInstance();
            m_ssaoDebugVisBuffer = createRegularBuffer(sizeof(SSAODebugVisData));

            m_ssaoDebugVisLines.normal.init();
            m_ssaoDebugVisLines.normal.setColor(glm::vec4(0.f, 0.f, 1.f, 1.f));

            m_ssaoDebugVisLines.projectedNormal.init();
            m_ssaoDebugVisLines.projectedNormal.setColor(glm::vec4(1.f, 1.f, 0.f, 1.f));

            m_ssaoDebugVisLines.wo.init();
            m_ssaoDebugVisLines.wo.setColor(glm::vec4(1.f, 0.f, 1.f, 1.f));

            m_ssaoDebugVisLines.sliceDir.init();
            m_ssaoDebugVisLines.sliceDir.setColor(glm::vec4(1.f, 1.f, 1.f, 1.f));

            for (int i = 0; i < ARRAY_COUNT(m_ssaoDebugVisLines.samples); ++i)
            {
                m_ssaoDebugVisLines.samples[i].init();
                m_ssaoDebugVisLines.samples[i].setColor(glm::vec4(1.f, 0.f, 0.f, 1.f));
            }
        }

        // bloom
        {
            m_bloomSetupRenderTarget = createRenderTarget(windowSize.x, windowSize.y);
            TextureSpec spec = { };
            spec.width = windowSize.x;
            spec.height = windowSize.y;
            spec.type = Texture::Type::TEX_2D;
            spec.format = Texture::ColorFormat::R16G16B16A16; 
            spec.dataType = Texture::DataType::Float;
            spec.min = Texture::Filter::LINEAR;
            spec.mag = Texture::Filter::LINEAR;
            spec.s = Texture::Wrap::CLAMP_TO_EDGE;
            spec.t = Texture::Wrap::CLAMP_TO_EDGE;
            spec.r = Texture::Wrap::CLAMP_TO_EDGE;
            m_bloomSetupRenderTarget->setColorBuffer(textureManager->createTexture("BloomSetupTexture", spec), 0);
            m_bloomSetupShader = createShader("BloomSetupShader", SHADER_SOURCE_PATH "shader_bloom_preprocess.vs", SHADER_SOURCE_PATH "shader_bloom_preprocess.fs");
            m_bloomSetupMatl = createMaterial(m_bloomSetupShader)->createInstance();
            m_bloomDsShader = createShader("BloomDownSampleShader", SHADER_SOURCE_PATH "shader_downsample.vs", SHADER_SOURCE_PATH "shader_downsample.fs");
            m_bloomDsMatl = createMaterial(m_bloomDsShader)->createInstance();
            m_bloomUsShader = createShader("UpSampleShader", SHADER_SOURCE_PATH "shader_upsample.vs", SHADER_SOURCE_PATH "shader_upsample.fs");
            m_bloomUsMatl = createMaterial(m_bloomUsShader)->createInstance();
            m_gaussianBlurShader = createShader("GaussianBlurShader", SHADER_SOURCE_PATH "shader_gaussian_blur.vs", SHADER_SOURCE_PATH "shader_gaussian_blur.fs");
            m_gaussianBlurMatl = createMaterial(m_gaussianBlurShader)->createInstance();

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
            m_compositeShader = createShader("CompositeShader", SHADER_SOURCE_PATH "composite_v.glsl", SHADER_SOURCE_PATH "composite_p.glsl");
            m_compositeMatl = createMaterial(m_compositeShader)->createInstance();
        }

        // voxel cone tracing
        {
            TextureSpec voxelizeSpec = { };
            voxelizeSpec.width = m_sceneVoxelGrid.resolution;
            voxelizeSpec.height = m_sceneVoxelGrid.resolution;
            voxelizeSpec.type = Texture::Type::TEX_2D;
            voxelizeSpec.dataType = Texture::DataType::Float;
            voxelizeSpec.format = Texture::ColorFormat::R16G16B16;
            voxelizeSpec.min = Texture::Filter::LINEAR; 
            voxelizeSpec.mag = Texture::Filter::LINEAR;
            m_voxelizeColorTexture = textureManager->createTexture("VoxelizeDebug", voxelizeSpec);
            m_voxelizeRenderTarget = createRenderTarget(m_sceneVoxelGrid.resolution, m_sceneVoxelGrid.resolution);
            m_voxelizeRenderTarget->setColorBuffer(m_voxelizeColorTexture, 0);

            TextureSpec visSpec = { };
            visSpec.width = 1280;
            visSpec.height = 720;
            visSpec.type = Texture::Type::TEX_2D;
            visSpec.dataType = Texture::DataType::Float;
            visSpec.format = Texture::ColorFormat::R16G16B16;
            visSpec.min = Texture::Filter::LINEAR; 
            visSpec.mag = Texture::Filter::LINEAR;
            visSpec.numMips = 1;
            m_voxelVisColorTexture = textureManager->createTexture("VoxelVis", visSpec);
            m_voxelVisRenderTarget = createRenderTarget(visSpec.width, visSpec.height);
            m_voxelVisRenderTarget->setColorBuffer(m_voxelVisColorTexture, 0u);

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

                m_voxelizeShader = createVsGsPsShader("VoxelizeShader", SHADER_SOURCE_PATH "voxelize_v.glsl", SHADER_SOURCE_PATH "voxelize_g.glsl", SHADER_SOURCE_PATH "voxelize_p.glsl");
                m_voxelizeMatl = createMaterial(m_voxelizeShader)->createInstance(); 
                m_voxelVisShader = createVsGsPsShader("VoxelVisShader", SHADER_SOURCE_PATH "voxel_vis_v.glsl", SHADER_SOURCE_PATH "voxel_vis_g.glsl", SHADER_SOURCE_PATH "voxel_vis_p.glsl");
                m_voxelVisMatl = createMaterial(m_voxelVisShader)->createInstance();
                m_filterVoxelGridShader = createCsShader("VctxFilterShader", SHADER_SOURCE_PATH "vctx_filter_c.glsl");
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
                m_vctx.renderTarget->setColorBuffer(m_vctx.irradiance,(u32)ColorBuffers::kIrradiance);
                m_vctx.renderTarget->setColorBuffer(m_vctx.reflection,(u32)ColorBuffers::kReflection);

                m_vctx.shader = createShader("VctxShader", SHADER_SOURCE_PATH "vctx_v.glsl", SHADER_SOURCE_PATH "vctx_p.glsl");
            }

            m_vctxVis.coneVisComputeShader = createCsShader("ConeVisComputeShader", SHADER_SOURCE_PATH "cone_vis_c.glsl");
            m_vctxVis.coneVisDrawShader = createVsGsPsShader("ConeVisGraphicsShader", SHADER_SOURCE_PATH "cone_vis_v.glsl", SHADER_SOURCE_PATH "cone_vis_g.glsl", SHADER_SOURCE_PATH "cone_vis_p.glsl");

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
            m_vctxVis.cachedSceneDepth = textureManager->createTexture("CachedSceneDepth", depthNormSpec);
            m_vctxVis.cachedSceneNormal = textureManager->createTexture("CachedSceneNormal", depthNormSpec);

            // global ssbo holding vctx data
            glCreateBuffers(1, &m_vctxSsbo);
            glNamedBufferData(m_vctxSsbo, sizeof(VctxGpuData), &m_vctxGpuData, GL_DYNAMIC_DRAW);

            struct IndirectDrawArgs
            {
                GLuint first;
                GLuint count;
            };

            // debug ssbo
            glCreateBuffers(1, &m_vctxVis.ssbo);
            glNamedBufferData(m_vctxVis.ssbo, sizeof(VctxVis::DebugBuffer), &m_vctxVis.debugConeBuffer, GL_DYNAMIC_COPY);

            glCreateBuffers(1, &m_vctxVis.idbo);
            glNamedBufferData(m_vctxVis.idbo, sizeof(IndirectDrawArgs), nullptr, GL_DYNAMIC_COPY);
        }

        {
            // misc
            m_debugCam.position = glm::vec3(0.f, 50.f, 0.f);
            m_debugCam.lookAt = glm::vec3(0.f);
            m_debugCam.fov = 50.f;
            m_debugCam.n = 0.1f;
            m_debugCam.f = 100.f;
            m_debugCam.projection = glm::perspective(m_debugCam.fov, 16.f/9.f, m_debugCam.n, m_debugCam.f);
            m_debugCam.update();
        }

        // todo: load global glsl definitions and save them in a map <path, content>

        // ui
        UI::initialize(window);
    }

    void Renderer::finalize()
    {
        // imgui clean up
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void Renderer::drawMesh(Mesh* mesh)
    {
        for (u32 sm = 0; sm < mesh->numSubMeshes(); ++sm)
        {
            auto subMesh = mesh->m_subMeshes[sm];
            m_ctx->setVertexArray(subMesh->m_vertexArray);
            if (subMesh->m_vertexArray->hasIndexBuffer())
                m_ctx->drawIndex(subMesh->m_vertexArray->m_numIndices);
            else m_ctx->drawIndexAuto(subMesh->m_vertexArray->numVerts());
        }
    }

    void Renderer::drawMesh(Mesh* mesh, MaterialInstance* matl, RenderTarget* dstRenderTarget, const std::initializer_list<i32>& drawBuffers, const Viewport& viewport)
    {
        m_ctx->setShader(matl->getShader());
        matl->bind();
        m_ctx->setRenderTarget(dstRenderTarget, drawBuffers);
        m_ctx->setViewport(viewport);
        for (u32 sm = 0; sm < mesh->numSubMeshes(); ++sm)
        {
            auto subMesh = mesh->m_subMeshes[sm];
            m_ctx->setVertexArray(subMesh->m_vertexArray);
            if (subMesh->m_vertexArray->hasIndexBuffer())
                m_ctx->drawIndex(subMesh->m_vertexArray->m_numIndices);
            else m_ctx->drawIndexAuto(subMesh->m_vertexArray->numVerts());
        }
    }

    void Renderer::drawMeshInstance(MeshInstance* meshInstance, i32 transformIndex)
    {
        Mesh* mesh = meshInstance->m_mesh;
        for (u32 i = 0; i < mesh->m_subMeshes.size(); ++i)
        {
            MaterialInstance* matl = meshInstance->m_matls[i]; 
            Shader* shader = matl->getShader();
            m_ctx->setShader(shader);
            matl->set("transformIndex", transformIndex);
            UsedBindingPoints used = matl->bind();
            Mesh::SubMesh* sm = mesh->m_subMeshes[i];
            m_ctx->setVertexArray(sm->m_vertexArray);
            m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
            if (sm->m_vertexArray->hasIndexBuffer())
                m_ctx->drawIndex(sm->m_numIndices);
            else
                m_ctx->drawIndexAuto(sm->m_numVerts);
        }
    }

    void sceneVoxelGridAABB(const BoundingBox3f& aabb, glm::vec3& pmin, glm::vec3& pmax)
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
        // clear voxel grid textures
        glClearTexImage(m_sceneVoxelGrid.albedo->handle, 0, GL_RGBA, GL_UNSIGNED_INT, 0);
        glClearTexImage(m_sceneVoxelGrid.normal->handle, 0, GL_RGBA, GL_UNSIGNED_INT, 0);
        glClearTexImage(m_sceneVoxelGrid.radiance->handle, 0, GL_RGBA, GL_UNSIGNED_INT, 0);

        m_ctx->setRenderTarget(m_voxelizeRenderTarget, { 0 });
        m_voxelizeRenderTarget->clear({ 0 });
        m_ctx->setViewport({ 0, 0, m_sceneVoxelGrid.resolution, m_sceneVoxelGrid.resolution });
        m_ctx->setDepthControl(DepthControl::kDisable);
        glEnable(GL_NV_conservative_raster);
        glDisable(GL_CULL_FACE);

        m_ctx->setShader(m_voxelizeShader);
        m_ctx->setPrimitiveType(PrimitiveType::TriangleList);

        enum class ImageBindings
        {
            kAlbedo = 0,
            kNormal,
            kEmission,
            kRadiance
        };
        // bind 3D volume texture to image units
        glBindImageTexture(static_cast<u32>(ImageBindings::kAlbedo), m_sceneVoxelGrid.albedo->handle, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(static_cast<u32>(ImageBindings::kNormal), m_sceneVoxelGrid.normal->handle, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(static_cast<u32>(ImageBindings::kRadiance), m_sceneVoxelGrid.radiance->handle, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

        enum class Steps
        {
            kXAxis = 0,
            kYAxis,
            kZAxis,
            kCount
        };

        // get a cube shape aabb enclosing the scene
        glm::vec3 pmin, pmax;
        sceneVoxelGridAABB(scene->aabb, pmin, pmax);

        for (i32 axis = (i32)Steps::kXAxis; axis < (i32)Steps::kCount; ++axis)
        {
            for (u32 i = 0; i < scene->entities.size(); ++i)
            {
                executeOnEntity(scene->entities[i], [this, pmin, pmax, axis](SceneNode* node) {
                    if (MeshInstance* meshInstance = node->getAttachedMesh())
                    {
                        glm::mat4 model = node->getWorldTransform().toMatrix();
                        m_voxelizeMatl->set("model", &model[0]);
                        m_voxelizeMatl->set("aabbMin", &pmin.x);
                        m_voxelizeMatl->set("aabbMax", &pmax.x);
                        m_voxelizeMatl->set("axis", (u32)axis);

                        for (u32 i = 0; i < meshInstance->m_mesh->numSubMeshes(); ++i)
                        {
                            u32 matlFlag = 0u;
                            auto sm = meshInstance->m_mesh->m_subMeshes[i];
                            MaterialInstance* smMatl = meshInstance->getMaterial(i);
                            Texture* albedo = smMatl->getTexture("diffuseMaps[0]");
                            Texture* normal = smMatl->getTexture("normalMap");
                            m_voxelizeMatl->bindTexture("albedoTexture", albedo);
                            m_voxelizeMatl->bindTexture("normalTexture", normal);
                            if (albedo)
                            {
                                matlFlag |= StandardPbrMaterial::Flags::kHasDiffuseMap;
                            }
                            else
                            {
                                glm::vec4 flatColor = smMatl->getVec4("uMatlData.flatColor");
                                m_voxelizeMatl->set("flatColor", &flatColor.x);
                            }
                            if (normal)
                            {
                                matlFlag |= StandardPbrMaterial::Flags::kHasNormalMap;
                            }
                            else
                            {

                            }
                            m_voxelizeMatl->set("matlFlag", matlFlag);
                            m_voxelizeMatl->bind();

                            m_ctx->setVertexArray(sm->m_vertexArray);
                            if (sm->m_vertexArray->hasIndexBuffer())
                            {
                                m_ctx->drawIndex(sm->m_vertexArray->m_numIndices);
                            }
                            else
                            {
                                m_ctx->drawIndexAuto(sm->m_numVerts);
                            }
                        }
                    }
                });
            }
        }

        m_ctx->setDepthControl(DepthControl::kEnable);
        glEnable(GL_CULL_FACE);
        glDisable(GL_NV_conservative_raster);

        if (m_opts.regenVoxelGridMipmap)
        {
            glGenerateTextureMipmap(m_sceneVoxelGrid.albedo->handle);
            glGenerateTextureMipmap(m_sceneVoxelGrid.radiance->handle);
        }
        // manually filter & down-sample to generate mipmap with anisotropic voxels
        else
        {
            m_ctx->setShader(m_filterVoxelGridShader);

            const i32 kNumDownsample = 1u;
            for (i32 i = 0; i < kNumDownsample; ++i)
            {
                i32 textureUnit = (i32)GlobalTextureBindings::kCount;
                glm::ivec3 currGridDim = glm::ivec3(m_sceneVoxelGrid.resolution) / (i32)pow(2, i+1);
                m_filterVoxelGridShader->setUniform1i("srcMip", i);
                m_filterVoxelGridShader->setUniform1i("srcAlbedo", textureUnit);
                glBindTextureUnit(textureUnit++, m_sceneVoxelGrid.albedo->handle);

                m_filterVoxelGridShader->setUniform1i("dstAlbedo", textureUnit);
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

        // update buffer data
        glm::vec3 pmin, pmax;
        sceneVoxelGridAABB(scene->aabb, pmin, pmax);
        m_vctxGpuData.localOrigin = glm::vec3(pmin.x, pmin.y, pmax.z);
        m_vctxGpuData.voxelSize = (pmax.x - pmin.x) / (f32)m_sceneVoxelGrid.resolution;
        m_vctxGpuData.visMode = 1 << static_cast<i32>(m_vctxVis.mode);
        glNamedBufferSubData(m_vctxSsbo, 0, sizeof(VctxGpuData), &m_vctxGpuData);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, gBufferBinding(VctxGlobalData), m_vctxSsbo);
    }

    void Renderer::visualizeVoxelGrid()
    {
        glDisable(GL_CULL_FACE);
        {
            m_ctx->setRenderTarget(m_voxelVisRenderTarget, { 0 });
            m_voxelVisRenderTarget->clear({ 0 });
            m_ctx->setViewport({0u, 0u, m_voxelVisRenderTarget->width, m_voxelVisRenderTarget->height});

            m_ctx->setShader(m_voxelVisShader);
            m_voxelVisMatl->set("activeMip", m_vctxVis.activeMip);
            m_voxelVisMatl->bind();

            m_ctx->setPrimitiveType(PrimitiveType::Points);
            u32 currRes = m_sceneVoxelGrid.resolution / pow(2, m_vctxVis.activeMip);
            m_ctx->drawIndexAuto(currRes * currRes * currRes);
        }
        m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
        glEnable(GL_CULL_FACE);
    }

    void Renderer::visualizeConeTrace(Scene* scene)
    {
        // debug vis for voxel cone tracing
        auto sceneDepthTexture = m_opts.enableAA ? m_sceneDepthTextureSSAA : m_sceneDepthTexture;
        auto sceneNormalTexture = m_opts.enableAA ? m_sceneNormalTextureSSAA : m_sceneNormalTexture;
#if 1
        if (!m_vctxVis.cachedTexInitialized || m_vctxVis.debugScreenPosMoved)
        {
            if (m_vctxVis.cachedSceneDepth->width == sceneDepthTexture->width && m_vctxVis.cachedSceneDepth->height == sceneDepthTexture->height &&
                m_vctxVis.cachedSceneNormal->width == sceneNormalTexture->width && m_vctxVis.cachedSceneNormal->height == sceneNormalTexture->height)
            {
                // update cached snapshot of scene depth/normal by copying texture data
                glCopyImageSubData(sceneDepthTexture->handle, GL_TEXTURE_2D, 0, 0, 0, 0, 
                    m_vctxVis.cachedSceneDepth->handle, GL_TEXTURE_2D, 0, 0, 0, 0, sceneDepthTexture->width, sceneDepthTexture->height, 1);
                glCopyImageSubData(sceneNormalTexture->handle, GL_TEXTURE_2D, 0, 0, 0, 0, 
                    m_vctxVis.cachedSceneNormal->handle, GL_TEXTURE_2D, 0, 0, 0, 0, sceneNormalTexture->width, sceneNormalTexture->height, 1);


                m_vctxVis.cachedView = m_globalDrawData.view;
                m_vctxVis.cachedProjection = m_globalDrawData.projection;

                if (!m_vctxVis.cachedTexInitialized)
                {
                    m_vctxVis.cachedTexInitialized = true;
                }
            }
            else
            {
                cyanError("Invalid texture copying operation due to mismatched texture dimensions!");
            }
        }
#endif
        // pass 0: run a compute pass to fill debug cone data and visualize cone directions
        m_ctx->setShader(m_vctxVis.coneVisComputeShader);

        enum class SsboBindings
        {
            kIndirectDraw = 5,
            kConeData
        };

        auto index = glGetProgramResourceIndex(m_vctxVis.coneVisComputeShader->handle, GL_SHADER_STORAGE_BLOCK, "IndirectDrawArgs");
        glShaderStorageBlockBinding(m_vctxVis.coneVisComputeShader->handle, index, (u32)SsboBindings::kIndirectDraw);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)SsboBindings::kIndirectDraw, m_vctxVis.idbo);

        index = glGetProgramResourceIndex(m_vctxVis.coneVisComputeShader->handle, GL_SHADER_STORAGE_BLOCK, "ConeTraceDebugData");
        glShaderStorageBlockBinding(m_vctxVis.coneVisComputeShader->handle, index, (u32)SsboBindings::kConeData);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)SsboBindings::kConeData, m_vctxVis.ssbo);

        enum class TexBindings
        {
            kSceneDepth = 0,
            kSceneNormal
        };

        m_vctxVis.coneVisComputeShader->setUniformVec2("debugScreenPos", &m_vctxVis.debugScreenPos.x);
        m_vctxVis.coneVisComputeShader->setUniformMat4f("cachedView", &m_vctxVis.cachedView[0][0]);
        m_vctxVis.coneVisComputeShader->setUniformMat4f("cachedProjection", &m_vctxVis.cachedProjection[0][0]);
        glm::vec2 renderSize = glm::vec2(m_voxelVisRenderTarget->width, m_voxelVisRenderTarget->height);
        m_vctxVis.coneVisComputeShader->setUniformVec2("renderSize", &renderSize.x);
        m_vctxVis.coneVisComputeShader->setUniform1i("sceneDepthTexture", (i32)TexBindings::kSceneDepth);
        m_vctxVis.coneVisComputeShader->setUniform1i("sceneNormalTexture", (i32)TexBindings::kSceneNormal);
        m_vctxVis.coneVisComputeShader->setUniform1f("vctxOffset", m_vctx.opts.offset);
        glBindTextureUnit((u32)TexBindings::kSceneDepth, m_vctxVis.cachedSceneDepth->handle);
        glBindTextureUnit((u32)TexBindings::kSceneNormal, m_vctxVis.cachedSceneNormal->handle);

        glDispatchCompute(1, 1, 1);

        // sync
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // visualize traced cones
        m_ctx->setRenderTarget(m_voxelVisRenderTarget, { 0 });
        m_ctx->setViewport({ 0, 0, m_voxelVisRenderTarget->width, m_voxelVisRenderTarget->height });
        m_ctx->setShader(m_vctxVis.coneVisDrawShader);

#if 0
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
        // indirect draw to launch vs/gs
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_vctxVis.idbo);
        glDrawArraysIndirect(GL_POINTS, 0);
#else
        index = glGetProgramResourceIndex(m_vctxVis.coneVisDrawShader->handle, GL_SHADER_STORAGE_BLOCK, "ConeTraceDebugData");
        glShaderStorageBlockBinding(m_vctxVis.coneVisDrawShader->handle, index, (u32)SsboBindings::kConeData);

        struct IndirectDrawArgs
        {
            GLuint first;
            GLuint count;
        } command;
        glGetNamedBufferSubData(m_vctxVis.idbo, 0, sizeof(IndirectDrawArgs), &command);
        m_ctx->setPrimitiveType(PrimitiveType::Points);
        glDisable(GL_CULL_FACE);
        m_ctx->drawIndexAuto(command.count);
#endif
        m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
        glEnable(GL_CULL_FACE);
    }

    void Renderer::drawEntity(Entity* entity)
    {
        std::queue<SceneNode*> nodes;
        nodes.push(entity->m_sceneRoot);
        while (!nodes.empty())
        {
            auto node = nodes.front();
            nodes.pop();
            if (node->m_meshInstance)
                drawMeshInstance(node->m_meshInstance, node->globalTransform);
            for (u32 i = 0; i < node->m_child.size(); ++i)
                nodes.push(node->m_child[i]);
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
        // clear default render target
        m_ctx->setRenderTarget(nullptr, { });
        m_ctx->clear();

        // clear per frame allocator
        m_frameAllocator.reset();
    }

    void Renderer::render(Scene* scene, const std::function<void()>& externDebugRender)
    {
        beginRender();
        {
            // update all global data
            updateFrameGlobalData(scene, scene->cameras[scene->activeCamera]);
            // sun shadow pass
            if (m_opts.enableSunShadow)
            {
                renderSunShadow(scene, scene->entities);
            }
            // generate vctx data
            {
                // todo: revoxelizing the scene every frame is causing flickering in the volume texture
                voxelizeScene(scene);
            }
            if (m_opts.enableSSAO)
            {
                // scene depth & normal pass
                renderSceneDepthNormal(scene);
                // ssao pass
                ssao(scene->getActiveCamera());
            }
            if (m_opts.enableVctx)
            {
                // voxel cone tracing pass
                renderVctx();
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
        endRender();
    }

    void Renderer::endRender()
    {
        // reset render target
        m_ctx->setRenderTarget(nullptr, {});
    }

    void Renderer::flip()
    {
        m_ctx->flip();
    }

    void Renderer::executeOnEntity(Entity* e, const std::function<void(SceneNode*)>& func)
    {
        std::queue<SceneNode*> nodes;
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
        glBindTextureUnit(gTexBinding(SSAO), m_ssaoTexture->handle);

        // upload data to gpu
        glNamedBufferSubData(gDrawDataBuffer, 0, sizeof(GlobalDrawData), &m_globalDrawData);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<u32>(GlobalBufferBindings::DrawData), gDrawDataBuffer);
    }

    void Renderer::updateTransforms(Scene* scene)
    {
        if (sizeofVector(scene->g_globalTransforms) > gInstanceTransforms.kBufferSize)
            cyanError("Gpu global transform SBO overflow!");
        glNamedBufferSubData(gInstanceTransforms.SBO, 0, sizeofVector(scene->g_globalTransformMatrices), scene->g_globalTransformMatrices.data());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)GlobalBufferBindings::GlobalTransforms, gInstanceTransforms.SBO);
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
        glBindTextureUnit(gTexBinding(BRDFLookupTexture), ReflectionProbe::getBRDFLookupTexture()->handle);

        // skybox
        if (gLighting.skybox)
        {
            glBindTextureUnit(0, gLighting.skybox->getDiffueTexture()->handle);
            glBindTextureUnit(1, gLighting.skybox->getSpecularTexture()->handle);
        }

        // additional light probes
        if (gLighting.irradianceProbe)
            glBindTextureUnit(3, gLighting.irradianceProbe->m_convolvedIrradianceTexture->handle);

        if (gLighting.reflectionProbe)
            glBindTextureUnit(4, gLighting.reflectionProbe->m_convolvedReflectionTexture->handle);
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
        m_shadowmapManager.render(m_csm, scene, sunLight, shadowCasters);
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
        m_ctx->setRenderTarget(renderTarget, { static_cast<i32>(ColorBuffers::kDepth), static_cast<i32>(ColorBuffers::kNormal) });
        m_ctx->setViewport({ 0u, 0u, renderTarget->width, renderTarget->height });
        renderTarget->clear({ static_cast<i32>(DrawBuffers::kDepth) }, glm::vec4(1.f));
        renderTarget->clear({ static_cast<i32>(DrawBuffers::kNormal) });

        m_ctx->setShader(m_sceneDepthNormalShader);
        for (u32 e = 0; e < (u32)scene->entities.size(); ++e)
        {
            auto entity = scene->entities[e];
            if (entity->m_includeInGBufferPass && entity->m_visible)
            {
                executeOnEntity(entity, [this](SceneNode* node) { 
                    if (node->m_meshInstance)
                    {
                        m_sceneDepthNormalShader->setUniform1i("transformIndex", node->globalTransform);
                        drawMesh(node->m_meshInstance->m_mesh);
                    }
                });
            }
        }
    }

    void Renderer::renderScene(Scene* scene)
    {
        auto renderTarget = m_opts.enableAA ? m_sceneColorRTSSAA : m_sceneColorRenderTarget;
        m_ctx->setRenderTarget(renderTarget, { 0 });
        m_ctx->setViewport({ 0u, 0u, renderTarget->width, renderTarget->height });
        renderTarget->clear({ 0 });

        // draw skybox
        if (scene->m_skybox)
        {
            scene->m_skybox->render();
        }
        // draw entitiess
        for (u32 i = 0; i < scene->entities.size(); ++i)
        {
            if (scene->entities[i]->m_visible)
            {
                executeOnEntity(scene->entities[i], 
                    [this](SceneNode* node) {
                        if (node->m_meshInstance)
                            drawMeshInstance(node->m_meshInstance, node->globalTransform);
                    });
            }
        }
    }

    void Renderer::ssao(Camera& camera)
    {
        m_ctx->setRenderTarget(m_ssaoRenderTarget, { 0 });
        m_ctx->setViewport({0, 0, m_ssaoRenderTarget->width, m_ssaoRenderTarget->height});
        m_ssaoRenderTarget->clear({ 0 }, glm::vec4(1.f));

        m_ctx->setShader(m_ssaoShader);
        m_ctx->setDepthControl(kDisable);
        m_ssaoMatl->bindTexture("normalTexture", m_opts.enableAA ? m_sceneNormalTextureSSAA : m_sceneNormalTexture);
        m_ssaoMatl->bindTexture("depthTexture", m_opts.enableAA ? m_sceneDepthTextureSSAA : m_sceneDepthTexture);
        m_ssaoMatl->set("cameraPos", &camera.position.x);
        m_ssaoMatl->set("view", &camera.view[0]);
        m_ssaoMatl->set("projection", &camera.projection[0]);
        m_ssaoMatl->bindBuffer("DebugVisData", m_ssaoDebugVisBuffer);
        m_ssaoMatl->bind();

        m_ctx->setVertexArray(s_quad.m_vertexArray);
        m_ctx->drawIndexAuto(s_quad.m_vertexArray->numVerts());
        m_ctx->setDepthControl(DepthControl::kEnable);
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
            m_ctx->setRenderTarget(renderTarget, { static_cast<u32>(ColorBuffer::kDst) });
            m_ctx->setViewport({ 0u, 0u, dst->width, dst->height });
            m_ctx->setDepthControl(Cyan::DepthControl::kDisable);

            m_ctx->setShader(m_bloomDsShader);
            m_bloomDsMatl->bindTexture("srcImage", src);
            m_bloomDsMatl->bind();
            m_ctx->setVertexArray(s_quad.m_vertexArray);
            m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
            m_ctx->drawIndexAuto(s_quad.m_vertexArray->numVerts());

            m_ctx->setDepthControl(Cyan::DepthControl::kEnable);
        };

        auto upscale = [this](Texture* dst, Texture* src, Texture* blend, RenderTarget* renderTarget, u32 stageIndex) {
            enum class ColorBuffer
            {
                kDst = 0
            };
            renderTarget->setColorBuffer(dst, static_cast<i32>(ColorBuffer::kDst));
            m_ctx->setRenderTarget(renderTarget, { static_cast<i32>(ColorBuffer::kDst)});
            m_ctx->setViewport({ 0u, 0u, dst->width, dst->height });
            m_ctx->setDepthControl(Cyan::DepthControl::kDisable);

            m_ctx->setShader(m_bloomUsShader);
            m_bloomUsMatl->bindTexture("srcImage", src);
            m_bloomUsMatl->bindTexture("blendImage", blend);
            m_bloomUsMatl->set("stageIndex", stageIndex);
            m_bloomUsMatl->bind();
            m_ctx->setVertexArray(s_quad.m_vertexArray);
            m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
            m_ctx->drawIndexAuto(s_quad.m_vertexArray->numVerts());

            m_ctx->setDepthControl(Cyan::DepthControl::kEnable);
        };

        // user have to make sure that renderTarget is compatible with 'dst', 'src', and 'scratch'
        auto gaussianBlur = [this](Texture* dst, Texture* src, Texture* scratch, RenderTarget* renderTarget, i32 kernelIndex, i32 radius) {

            enum class ColorBuffer
            {
                kSrc = 0,
                kScratch,
                kDst
            };

            m_ctx->setDepthControl(Cyan::DepthControl::kDisable);
            m_ctx->setShader(m_gaussianBlurShader);
            renderTarget->setColorBuffer(src, static_cast<u32>(ColorBuffer::kSrc));
            renderTarget->setColorBuffer(scratch, static_cast<u32>(ColorBuffer::kScratch));
            if (dst)
            {
                renderTarget->setColorBuffer(dst, static_cast<u32>(ColorBuffer::kDst));
            }
            m_ctx->setViewport({ 0u, 0u, renderTarget->width, renderTarget->height });

            // horizontal pass (blur 'src' into 'scratch')
            {
                m_ctx->setRenderTarget(renderTarget, { static_cast<u32>(ColorBuffer::kScratch) });
                m_gaussianBlurMatl->bindTexture("srcTexture", src);
                m_gaussianBlurMatl->set("horizontal", 1.0f);
                m_gaussianBlurMatl->set("kernelIndex", kernelIndex);
                m_gaussianBlurMatl->set("radius", radius);
                m_gaussianBlurMatl->bind();
                m_ctx->setVertexArray(s_quad.m_vertexArray);
                m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
                m_ctx->drawIndexAuto(s_quad.m_vertexArray->numVerts());
            }

            // vertical pass
            {
                if (dst)
                {
                    m_ctx->setRenderTarget(renderTarget, { static_cast<i32>(ColorBuffer::kDst) });
                }
                else
                {
                    m_ctx->setRenderTarget(renderTarget, { static_cast<i32>(ColorBuffer::kSrc) });
                }
                m_gaussianBlurMatl->bindTexture("srcTexture", scratch);
                m_gaussianBlurMatl->set("horizontal", 0.f);
                m_gaussianBlurMatl->set("kernelIndex", kernelIndex);
                m_gaussianBlurMatl->set("radius", radius);
                m_gaussianBlurMatl->bind();
                m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
                m_ctx->setVertexArray(s_quad.m_vertexArray);
                m_ctx->drawIndexAuto(s_quad.m_vertexArray->numVerts());
            }
            m_ctx->setDepthControl(Cyan::DepthControl::kEnable);
        };

        auto* src = m_opts.enableAA ? m_sceneColorTextureSSAA : m_sceneColorTexture;

        // bloom setup
        m_ctx->setRenderTarget(m_bloomSetupRenderTarget, { 0u });
        m_ctx->setShader(m_bloomSetupShader);
        m_ctx->setViewport({ 0u, 0u, m_bloomSetupRenderTarget->width, m_bloomSetupRenderTarget->height });
        m_bloomSetupRenderTarget->clear({ 0u });

        m_bloomSetupMatl->bindTexture("srcTexture", src);
        m_bloomSetupMatl->bind();

        m_ctx->setDepthControl(Cyan::DepthControl::kDisable);
        m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
        m_ctx->setVertexArray(s_quad.m_vertexArray);
        m_ctx->drawIndexAuto(s_quad.m_vertexArray->numVerts());
        m_ctx->setDepthControl(Cyan::DepthControl::kEnable);

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
        m_ctx->setRenderTarget(m_compositeRenderTarget, { 0u });
        m_ctx->setViewport({0u, 0u, m_compositeRenderTarget->width, m_compositeRenderTarget->height});

        m_ctx->setShader(m_compositeShader);
        m_compositeMatl->set("exposure", m_opts.exposure);
        m_compositeMatl->set("bloom", m_opts.enableBloom ? 1.f : 0.f);
        m_compositeMatl->set("bloomInstensity", m_opts.bloomIntensity);
        m_compositeMatl->bindTexture("bloomOutTexture", m_bloomOutTexture);
        m_compositeMatl->bindTexture("sceneColorTexture", m_opts.enableAA ? m_sceneColorTextureSSAA : m_sceneColorTexture);
        m_compositeMatl->bind();

        m_ctx->setDepthControl(DepthControl::kDisable);
        m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
        m_ctx->setVertexArray(s_quad.m_vertexArray);
        m_ctx->drawIndexAuto(s_quad.m_vertexArray->numVerts());
        m_ctx->setDepthControl(DepthControl::kEnable);

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
            // controls for VCTX
            ImGui::AlignTextToFramePadding();

            ImGui::Text("VisMode "); ImGui::SameLine();
            ImGui::Combo("##VisMode", reinterpret_cast<i32*>(&m_vctxVis.mode), vctxVisModeNames, (u32)VctxVis::Mode::kCount);

            ImGui::Text("Mip "); ImGui::SameLine();
            ImGui::SliderInt("##Mip", &m_vctxVis.activeMip, 0, log2(m_sceneVoxelGrid.resolution));

            ImGui::Text("Offset "); ImGui::SameLine();
            ImGui::SliderFloat("##Offset", &m_vctx.opts.offset, 0.f, 5.f, "%.2f");

            ImGui::Text("Ao Scale "); ImGui::SameLine();
            ImGui::SliderFloat("##Ao Scale", &m_vctx.opts.occlusionScale, 1.f, 5.f, "%.2f");

            ImGui::Text("Debug ScreenPos"); ImGui::SameLine();
            f32 v[2] = { m_vctxVis.debugScreenPos.x, m_vctxVis.debugScreenPos.y };
            m_vctxVis.debugScreenPosMoved = ImGui::SliderFloat2("##Debug ScreenPos", v, -1.f, 1.f, "%.2f");
            m_vctxVis.debugScreenPos = glm::vec2(v[0], v[1]);

            ImGui::Separator();
            ImVec2 visSize(480, 270);
            ImGui::Text("VoxelGrid Vis "); 
            ImGui::Image((ImTextureID)m_voxelVisColorTexture->handle, visSize, ImVec2(0, 1), ImVec2(1, 0));

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

        m_ctx->setViewport({ 0u, 0u, probe->sceneCapture->width, probe->sceneCapture->height });
        for (u32 f = 0; f < (sizeof(LightProbeCameras::cameraFacingDirections)/sizeof(LightProbeCameras::cameraFacingDirections[0])); ++f)
        {
            m_ctx->setRenderTarget(renderTarget, { (i32)f, -1, -1, -1 });
            renderTarget->clear({ (i32)f });

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
            // draw entities 
            for (u32 i = 0; i < staticObjects.size(); ++i)
            {
                drawEntity(staticObjects[i]);
            }
        }

        m_opts.enableSSAO = true;
    }

    void Renderer::renderVctx()
    {
        using ColorBuffers = Vctx::ColorBuffers;

        m_ctx->setRenderTarget(m_vctx.renderTarget, { (i32)ColorBuffers::kOcclusion, (i32)ColorBuffers::kIrradiance, (i32)ColorBuffers::kReflection });
        m_ctx->setViewport({ 0u, 0u, m_vctx.renderTarget->width, m_vctx.renderTarget->height });
        m_ctx->setShader(m_vctx.shader);

        enum class TexBindings
        {
            kDepth = 0,
            kNormal
        };
        glm::vec2 renderSize = glm::vec2(m_vctx.renderTarget->width, m_vctx.renderTarget->height);
        m_vctx.shader->setUniformVec2("renderSize", &renderSize.x);
        m_vctx.shader->setUniform1i("sceneDepthTexture", (i32)TexBindings::kDepth);
        m_vctx.shader->setUniform1i("sceneNormalTexture", (i32)TexBindings::kNormal);
        m_vctx.shader->setUniform1f("vctxOffset", m_vctx.opts.offset);
        m_vctx.shader->setUniform1f("occlusionScale", m_vctx.opts.occlusionScale);
        auto depthTexture = m_opts.enableAA ? m_sceneDepthTextureSSAA : m_sceneDepthTexture;
        auto normalTexture = m_opts.enableAA ? m_sceneNormalTextureSSAA : m_sceneNormalTexture;
        glBindTextureUnit((u32)TexBindings::kDepth, depthTexture->handle);
        glBindTextureUnit((u32)TexBindings::kNormal, normalTexture->handle);
        m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
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