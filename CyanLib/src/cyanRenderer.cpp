// TODO: Deal with name conflicts with windows header
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

    // static singleton pointer will be set to 0 before main() get called
    Renderer* Renderer::m_renderer = 0;

    Renderer::Renderer(GLFWwindow* window, const glm::vec2& windowSize)
        : m_frameAllocator(1024u * 1024u),  // 1 megabytes
        m_ctx(nullptr),
        u_model(0),
        u_cameraView(0),
        u_cameraProjection(0),
        m_bSuperSampleAA(true),
        m_offscreenRenderWidth(1280u),
        m_offscreenRenderHeight(720u),
        m_windowWidth(1280u),
        m_windowHeight(720u),
        m_sceneColorTexture(0),
        m_sceneColorRenderTarget(0),
        m_sceneColorTextureSSAA(0),
        m_sceneColorRTSSAA(0),
        m_voxelData{ 0 },
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
            // FIXME: this does not prevent from creating new instance
            // ensure that we are not creating new instance of Renderer
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

    void Renderer::initRenderTargets(u32 windowWidth, u32 windowHeight)
    {
        auto textureManager = TextureManager::getSingletonPtr();
        m_windowWidth = windowWidth;
        m_windowHeight = windowHeight;
        m_SSAAWidth = 2u * m_windowWidth;
        m_SSAAHeight = 2u * m_windowHeight;

        // super-sampling setup
        TextureSpec spec = { };
        spec.type = Texture::Type::TEX_2D;
        spec.format = Texture::ColorFormat::R16G16B16A16; 
        spec.dataType = Texture::DataType::Float;
        spec.width = m_SSAAWidth;
        spec.height = m_SSAAHeight;
        spec.min = Texture::Filter::LINEAR;
        spec.mag = Texture::Filter::LINEAR;
        spec.s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.r = Texture::Wrap::CLAMP_TO_EDGE;
        m_sceneColorTextureSSAA = textureManager->createTextureHDR("SceneColorTexSSAA", spec);
        m_sceneColorRTSSAA = createRenderTarget(m_SSAAWidth, m_SSAAHeight);
        m_sceneColorRTSSAA->setColorBuffer(m_sceneColorTextureSSAA, 0u);
        {
            TextureSpec spec0 = { };
            spec0.type = Texture::Type::TEX_2D;
            spec0.format = Texture::ColorFormat::R32G32B32; 
            spec0.dataType = Texture::DataType::Float;
            spec0.width = m_SSAAWidth;
            spec0.height = m_SSAAHeight;
            spec0.min = Texture::Filter::LINEAR;
            spec0.mag = Texture::Filter::LINEAR;
            spec0.s = Texture::Wrap::CLAMP_TO_EDGE;
            spec0.t = Texture::Wrap::CLAMP_TO_EDGE;
            spec0.r = Texture::Wrap::CLAMP_TO_EDGE;
            // todo: it seems using rgb16f leads to precision issue when building ssao
            m_sceneDepthTextureSSAA = textureManager->createTextureHDR("SceneDepthTexSSAA", spec0);
            m_sceneNormalTextureSSAA = textureManager->createTextureHDR("SceneNormalTextureSSAA", spec0);
            m_sceneColorRTSSAA->setColorBuffer(m_sceneDepthTextureSSAA, 1u);
            m_sceneColorRTSSAA->setColorBuffer(m_sceneNormalTextureSSAA, 2u);
        }

        // scene color render targets 
        spec.width = m_windowWidth;
        spec.height = m_windowHeight;
        m_sceneColorTexture = textureManager->createTextureHDR("SceneColorTexture", spec);
        m_sceneColorRenderTarget = createRenderTarget(m_windowWidth, m_windowHeight);
        m_sceneColorRenderTarget->setColorBuffer(m_sceneColorTexture, 0u);
        {
            TextureSpec spec0 = { };
            spec0.type = Texture::Type::TEX_2D;
            spec0.format = Texture::ColorFormat::R32G32B32; 
            spec0.dataType = Texture::DataType::Float;
            spec0.width = m_windowWidth;
            spec0.height = m_windowHeight;
            spec0.min = Texture::Filter::LINEAR;
            spec0.mag = Texture::Filter::LINEAR;
            spec0.s = Texture::Wrap::CLAMP_TO_EDGE;
            spec0.t = Texture::Wrap::CLAMP_TO_EDGE;
            spec0.r = Texture::Wrap::CLAMP_TO_EDGE;
            m_sceneNormalTexture = textureManager->createTextureHDR("SceneNormalTexture", spec0);
            spec0.format = Texture::ColorFormat::R32F; 
            m_sceneDepthTexture = textureManager->createTextureHDR("SceneDepthTexture", spec0);
            m_sceneColorRenderTarget->setColorBuffer(m_sceneDepthTexture, 1u);
            m_sceneColorRenderTarget->setColorBuffer(m_sceneNormalTexture, 2u);
        }
        // for composite pass
        m_compositeColorTexture = textureManager->createTextureHDR("CompositeColorTexture", spec);
        m_compositeRenderTarget = createRenderTarget(spec.width, spec.height);
        m_compositeRenderTarget->setColorBuffer(m_compositeColorTexture, 0u);

        // voxel
        m_voxelGridResolution = 512u;
        // create render target
        m_voxelRenderTarget = Cyan::createRenderTarget(m_voxelGridResolution, m_voxelGridResolution);
        TextureSpec voxelSpec = { };
        // TODO: using low resolution voxel grid for now because we are regenerating
        // mipmap per frame
        voxelSpec.width = m_voxelGridResolution;
        voxelSpec.height = m_voxelGridResolution;
        voxelSpec.type = Texture::Type::TEX_2D;
        voxelSpec.dataType = Texture::DataType::Float;
        voxelSpec.format = Texture::ColorFormat::R16G16B16;
        voxelSpec.min = Texture::Filter::LINEAR; 
        voxelSpec.mag = Texture::Filter::LINEAR;
        voxelSpec.numMips = 1;
        m_voxelColorTexture = textureManager->createTexture("Voxelization", voxelSpec);
        {
            TextureSpec visSpec = { };
            visSpec.width = 320;
            visSpec.height = 180;
            visSpec.type = Texture::Type::TEX_2D;
            visSpec.dataType = Texture::DataType::Float;
            visSpec.format = Texture::ColorFormat::R16G16B16;
            visSpec.min = Texture::Filter::LINEAR; 
            visSpec.mag = Texture::Filter::LINEAR;
            visSpec.numMips = 1;
            m_voxelVisColorTexture = textureManager->createTexture("VoxelVis", visSpec);
            m_voxelVisRenderTarget = createRenderTarget(visSpec.width, visSpec.height);
            m_voxelVisRenderTarget->setColorBuffer(m_voxelVisColorTexture, 0u);
        }
        m_voxelRenderTarget->setColorBuffer(m_voxelColorTexture, 0u);
        m_voxelVolumeTexture = new Texture;

        TextureSpec voxelDataSpec = { };
        voxelDataSpec.width = m_voxelGridResolution;
        voxelDataSpec.height = m_voxelGridResolution;
        voxelDataSpec.depth = m_voxelGridResolution;
        voxelDataSpec.type = Texture::Type::TEX_3D;
        voxelDataSpec.dataType = Texture::DataType::UNSIGNED_INT;
        voxelDataSpec.format = Texture::ColorFormat::R8G8B8A8;
        voxelDataSpec.min = Texture::Filter::LINEAR; 
        voxelDataSpec.mag = Texture::Filter::LINEAR;
        voxelDataSpec.r = Texture::Wrap::CLAMP_TO_EDGE;
        voxelDataSpec.s = Texture::Wrap::CLAMP_TO_EDGE;
        voxelDataSpec.t = Texture::Wrap::CLAMP_TO_EDGE;
        voxelDataSpec.numMips = 1;
        m_voxelData.m_albedo = textureManager->createTexture3D("VoxelAlbedo", voxelDataSpec);
        m_voxelData.m_normal = textureManager->createTexture3D("VoxelNormal", voxelDataSpec);
        m_voxelData.m_emission = textureManager->createTexture3D("VoxelEmission", voxelDataSpec);
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

        m_sceneDepthNormalShader = createShader("SceneDepthNormalShader", SHADER_SOURCE_PATH "scene_depth_normal_v.glsl", SHADER_SOURCE_PATH "scene_depth_normal_p.glsl");
    }

    void Renderer::initialize(GLFWwindow* window, glm::vec2 windowSize)
    {
        // onRendererInitialized(windowSize);

        m_ctx = getCurrentGfxCtx();
        u_model = createUniform("s_model", Uniform::Type::u_mat4);
        u_cameraView = createUniform("s_view", Uniform::Type::u_mat4);
        u_cameraProjection = createUniform("s_projection", Uniform::Type::u_mat4);
        m_ssaoSamplePoints.setColor(glm::vec4(0.f, 1.f, 1.f, 1.f));
        m_ssaoSamplePoints.setViewProjection(u_cameraView, u_cameraProjection);
        // initialize per frame shader draw data
        glCreateBuffers(1, &gDrawDataBuffer);
        glNamedBufferData(gDrawDataBuffer, sizeof(GlobalDrawData), &m_globalDrawData, GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &gLighting.dirLightSBO);
        glNamedBufferData(gLighting.dirLightSBO, gLighting.kDynamicLightBufferSize, nullptr, GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &gLighting.pointLightsSBO);
        glNamedBufferData(gLighting.pointLightsSBO, gLighting.kDynamicLightBufferSize, nullptr, GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &gInstanceTransforms.SBO);
        glNamedBufferData(gInstanceTransforms.SBO, gInstanceTransforms.kBufferSize, nullptr, GL_DYNAMIC_DRAW);

        m_viewport = { static_cast<u32>(0u), 
                       static_cast<u32>(0u), 
                       static_cast<u32>(windowSize.x), 
                       static_cast<u32>(windowSize.y) };
        // shadow
        m_shadowmapManager.initShadowmap(m_csm, glm::uvec2(4096u, 4096u));

        // voxel
        m_voxelVisShader = createShader("VoxelVisShader", SHADER_SOURCE_PATH "shader_render_voxel.vs", SHADER_SOURCE_PATH "shader_render_voxel.fs");
        // create shader
        m_voxelizeShader = createVsGsPsShader("VoxelizeShader", 
                                                   SHADER_SOURCE_PATH "shader_voxel.vs", 
                                                   SHADER_SOURCE_PATH "shader_voxel.gs", 
                                                   SHADER_SOURCE_PATH "shader_voxel.fs");
        m_voxelVisMatl = createMaterial(m_voxelVisShader)->createInstance();
        m_voxelizeMatl = createMaterial(m_voxelizeShader)->createInstance(); 

        // set back-face culling
        m_ctx->setCullFace(FrontFace::CounterClockWise, FaceCull::Back);

        // create shaders
        initShaders();
        // render targets
        initRenderTargets(m_viewport.m_width, m_viewport.m_height);

        // ssao
        {
            m_freezeDebugLines = false;
            auto textureManager = TextureManager::getSingletonPtr();
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
            auto textureManger = TextureManager::getSingletonPtr();
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
            m_bloomSetupRenderTarget->setColorBuffer(textureManger->createTexture("BloomSetupTexture", spec), 0);
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
                m_bloomDsTargets[index].src = textureManger->createTextureHDR(buff, spec);
                sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
                m_bloomDsTargets[index].scratch = textureManger->createTextureHDR(buff, spec);

                m_bloomUsTargets[index].renderTarget = createRenderTarget(spec.width, spec.height);
                sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
                m_bloomUsTargets[index].src = textureManger->createTextureHDR(buff, spec);
                sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
                m_bloomUsTargets[index].scratch = textureManger->createTextureHDR(buff, spec);
                spec.width /= 2;
                spec.height /= 2;
            };

            for (u32 i = 0u; i < kNumBloomPasses; ++i) {
                initBloomBuffers(i, spec);
            }
        }

        // composite
        {
            m_compositeShader = createShader("CompositeShader", SHADER_SOURCE_PATH "composite_v.glsl", SHADER_SOURCE_PATH "composite_p.glsl");
            m_compositeMatl = createMaterial(m_compositeShader)->createInstance();
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

    glm::vec2 Renderer::getViewportSize()
    {
        return glm::vec2(m_viewport.m_width, m_viewport.m_height);
    }

    void Renderer::setViewportSize(glm::vec2 size)
    {
        m_viewport.m_width = size.x;
        m_viewport.m_height = size.y;
    }

    Viewport Renderer::getViewport()
    {
        return m_viewport;
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

    Cyan::Texture* Renderer::voxelizeMesh(MeshInstance* mesh, glm::mat4* modelMatrix)
    {
        glEnable(GL_NV_conservative_raster);
        // clear voxel grid textures
        glClearTexImage(m_voxelData.m_albedo->handle, 0, GL_RGBA, GL_UNSIGNED_INT, 0);
        glClearTexImage(m_voxelData.m_normal->handle, 0, GL_RGBA, GL_UNSIGNED_INT, 0);

        m_ctx->setDepthControl(DepthControl::kDisable);
        // set shader
        m_ctx->setShader(m_voxelizeShader);
        // set render target
        m_ctx->setRenderTarget(m_voxelRenderTarget, { 0 });
        m_ctx->setClearColor(glm::vec4(0.1f));
        m_ctx->clear();
        Viewport originViewport = m_ctx->m_viewport;
        m_ctx->setViewport({ 0, 0, m_voxelGridResolution, m_voxelGridResolution });
        BoundingBox3f aabb = mesh->getAABB();
        glm::vec3 aabbMin = *modelMatrix * aabb.m_pMin;
        glm::vec3 aabbMax = *modelMatrix * aabb.m_pMax;

        MaterialInstance* boundMatl = mesh->getMaterial(0);
        Texture* albedo = boundMatl->getTexture("diffuseMaps[0]");
        Texture* normal = boundMatl->getTexture("normalMap");
        m_voxelizeMatl->bindTexture("albedoMap", albedo);
        m_voxelizeMatl->bindTexture("normalMap", normal);
        // bind 3D volume texture to image units
        glBindImageTexture(0, m_voxelData.m_albedo->handle, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(1, m_voxelData.m_normal->handle, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

        m_voxelizeMatl->set("model", modelMatrix);
        m_voxelizeMatl->set("aabbMin", &aabbMin.x);
        m_voxelizeMatl->set("aabbMax", &aabbMax.x);
        m_voxelizeMatl->bind();
        m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
        // draw mesh
        auto meshDef = mesh->m_mesh;
        for (auto sm : meshDef->m_subMeshes)
        {
            m_ctx->setVertexArray(sm->m_vertexArray);
            if (m_ctx->m_vertexArray->m_ibo != static_cast<u32>(-1)) {
                m_ctx->drawIndex(m_ctx->m_vertexArray->m_numIndices);
            } else {
                m_ctx->drawIndexAuto(sm->m_numVerts);
            }
        }
        m_ctx->setDepthControl(DepthControl::kEnable);
        glDisable(GL_NV_conservative_raster);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // generate mipmap for updated voxel textures
        // glGenerateTextureMipmap(m_voxelData.m_albedo->m_id);
        // glGenerateTextureMipmap(m_voxelData.m_normal->m_id);
        return m_voxelColorTexture;
    }

    // @ returns a copy is slow; in world space
    BoundingBox3f Renderer::computeSceneAABB(Scene* scene)
    {
        BoundingBox3f sceneAABB;
        // compute scene AABB in model space
        for (auto entity : scene->entities)
        {
            std::queue<SceneNode*> nodes;
            nodes.push(entity->m_sceneRoot);
            while (!nodes.empty())
            {
                SceneNode* node = nodes.front();
                nodes.pop();
                for (auto child : node->m_child)
                {
                    nodes.push(child);
                }
                if (MeshInstance* mesh = node->getAttachedMesh())
                {
                    BoundingBox3f aabb = mesh->getAABB();
                    glm::mat4 model = node->getWorldTransform().toMatrix();
                    aabb.m_pMin = model * aabb.m_pMin;
                    aabb.m_pMax = model * aabb.m_pMax;
                    sceneAABB.bound(aabb);
                }
            }
        }
        sceneAABB.init();
        return sceneAABB;
    } 

    typedef void(*SceneNodeCallBack)(SceneNode*);

    template<typename Callback>
    void customSceneVisitor(Scene* scene, Callback callback)
    {
        for (auto entity : scene->entities)
        {
            std::queue<SceneNode*> nodes;
            nodes.push(entity->m_sceneRoot);
            while (!nodes.empty())
            {
                SceneNode* node = nodes.front();
                nodes.pop();
                callback(node);
                for (auto child : node->m_child)
                {
                    nodes.push(child);
                }
            }
        }
    };

    /*
        * voxelize the scene by rasterizing along x,y and z-axis
    */
    /*
        @Note: Let's just go crazy and tank the frame rate to start things simple. Revoxelize the scene
        every frame, and regenerate mipmap every frame.
    */
    Cyan::Texture* Renderer::voxelizeScene(Scene* scene)
    {
        BoundingBox3f sceneAABB = computeSceneAABB(scene);

        glEnable(GL_NV_conservative_raster);
        glDisable(GL_CULL_FACE);
        // clear voxel grid textures
        glClearTexImage(m_voxelData.m_albedo->handle, 0, GL_RGBA, GL_UNSIGNED_INT, 0);
        glClearTexImage(m_voxelData.m_normal->handle, 0, GL_RGBA, GL_UNSIGNED_INT, 0);

        m_ctx->setDepthControl(DepthControl::kDisable);
        // set shader
        m_ctx->setShader(m_voxelizeShader);
        // set render target
        m_ctx->setRenderTarget(m_voxelRenderTarget, { 0 });
        m_ctx->setClearColor(glm::vec4(0.1f));
        m_ctx->clear();
        Viewport originViewport = m_ctx->m_viewport;
        m_ctx->setViewport({ 0, 0, m_voxelGridResolution, m_voxelGridResolution });
        m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
        // bind 3D volume texture to image units
        glBindImageTexture(0, m_voxelData.m_albedo->handle, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(1, m_voxelData.m_normal->handle, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

        u32 axis = 0u;
        auto nodeCallback = [&](SceneNode* node)
        {
            if (MeshInstance* meshInstance = node->getAttachedMesh())
            {
                if (node->m_hasAABB)
                {
                    i32 smIdx = 0u;
                    for (auto sm : meshInstance->m_mesh->m_subMeshes)
                    {
                        glm::mat4 model = node->getWorldTransform().toMatrix();
                        MaterialInstance* boundMatl = meshInstance->getMaterial(smIdx++);
                        Texture* albedo = boundMatl->getTexture("diffuseMaps[0]");
                        Texture* normal = boundMatl->getTexture("normalMap");
                        m_voxelizeMatl->bindTexture("albedoMap", albedo);
                        m_voxelizeMatl->set("flag", 0);
                        m_voxelizeMatl->set("model", &model[0][0]);
                        m_voxelizeMatl->set("aabbMin", &sceneAABB.m_pMin.x);
                        m_voxelizeMatl->set("aabbMax", &sceneAABB.m_pMax.x);
                        m_voxelizeMatl->bind();

                        // x-axis
                        m_ctx->setVertexArray(sm->m_vertexArray);
                        if (sm->m_vertexArray->m_ibo != static_cast<u32>(-1))
                        {
                            m_ctx->drawIndex(sm->m_vertexArray->m_numIndices);
                        }
                        else
                        {
                            m_ctx->drawIndexAuto(sm->m_numVerts);
                        }

                        m_voxelizeMatl->set("flag", 1);
                        m_voxelizeMatl->bind();
                        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                        // y-axis
                        if (sm->m_vertexArray->m_ibo != static_cast<u32>(-1))
                        {
                            m_ctx->drawIndex(sm->m_vertexArray->m_numIndices);
                        }
                        else
                        {
                            m_ctx->drawIndexAuto(sm->m_numVerts);
                        }

                        m_voxelizeMatl->set("flag", 2);
                        m_voxelizeMatl->bind();
                        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                        // z-axis
                        if (sm->m_vertexArray->m_ibo != static_cast<u32>(-1))
                        {
                            m_ctx->drawIndex(sm->m_vertexArray->m_numIndices);
                        }
                        else
                        {
                            m_ctx->drawIndexAuto(sm->m_numVerts);
                        }
                    }
                }
            }
        };     

        customSceneVisitor(scene, nodeCallback);

        m_ctx->setDepthControl(DepthControl::kEnable);
        glEnable(GL_CULL_FACE);
        glDisable(GL_NV_conservative_raster);
        // generate mipmap for updated voxel textures
        // glGenerateTextureMipmap(m_voxelData.m_albedo->m_id);
        // glGenerateTextureMipmap(m_voxelData.m_normal->m_id);
        return m_voxelColorTexture;
    }

    Texture* Renderer::renderVoxel(Scene* scene)
    {
        BoundingBox3f sceneAABB = computeSceneAABB(scene);
        m_ctx->setRenderTarget(m_voxelVisRenderTarget, { 0 });
        m_ctx->clear();
        m_ctx->setViewport({0u, 0u, m_voxelVisRenderTarget->width, m_voxelVisRenderTarget->height});
        m_ctx->setShader(m_voxelVisShader);
        QuadMesh* quad = getQuadMesh();
        m_ctx->setDepthControl(DepthControl::kDisable);
        m_ctx->setVertexArray(quad->m_vertexArray);
        m_voxelVisMatl->bindTexture("albedoMap", m_voxelData.m_albedo);
        m_voxelVisMatl->bindTexture("normalMap", m_voxelData.m_normal);
        m_voxelVisMatl->set("cameraPos", &scene->getActiveCamera().position.x);
        m_voxelVisMatl->set("cameraLookAt", &scene->getActiveCamera().lookAt.x);
        m_voxelVisMatl->set("aabbMin", &sceneAABB.m_pMin.x);
        m_voxelVisMatl->set("aabbMax", &sceneAABB.m_pMax.x);
        m_voxelVisMatl->bind();
        m_ctx->drawIndexAuto(6u);
        m_ctx->setDepthControl(DepthControl::kEnable);
        return m_voxelVisColorTexture;
    }

/*
    // TODO: this should take a custom factory defined by client and use
    // that factory to create an according RenderPass instance 
    void Renderer::addCustomPass(RenderPass* pass)
    {
        m_renderState.addRenderPass(pass);
        m_renderState.addClearRenderTarget(pass->m_renderTarget);
    }

    void Renderer::addScenePass(Scene* scene)
    {
        RenderTarget* renderTarget = m_renderState.m_superSampleAA ? m_sceneColorRTSSAA : m_sceneColorRenderTarget;
        void* preallocatedAddr = m_frameAllocator.alloc(sizeof(ScenePass));
        // placement new for initialization
        Viewport viewport = { 0, 0, m_offscreenRenderWidth, m_offscreenRenderHeight };
        ScenePass* pass = new (preallocatedAddr) ScenePass(renderTarget, viewport, scene);
        m_renderState.addClearRenderTarget(renderTarget);
        m_renderState.addRenderPass(pass);
    }

    void Renderer::addPostProcessPasses()
    {
        // TODO: bloom right now takes about 7ms to run, need to improve performance!
        if (m_bloom)
        {
            addBloomPass();
        }

        RenderTarget* renderTarget = m_outputRenderTarget;
        Texture* sceneColorTexture = m_bSuperSampleAA ? m_sceneColorTextureSSAA : m_sceneColorTexture;
        void* preallocatedAddr = m_frameAllocator.alloc(sizeof(PostProcessResolvePass));
        Viewport viewport = { 0u, 0u, renderTarget->m_width, renderTarget->m_height };
        PostProcessResolveInput inputs = { m_exposure, 0.f, 1.0f, sceneColorTexture, BloomPass::getBloomOutput() };
        if (m_bloom)
        {
            inputs.bloom = 1.0f;
        }
        PostProcessResolvePass* pass = new (preallocatedAddr) PostProcessResolvePass(renderTarget, viewport, inputs);
        m_renderState.addRenderPass(pass);
    }

    void Renderer::addTexturedQuadPass(RenderTarget* renderTarget, Viewport viewport, Texture* srcTexture)
    {
        void* preallocatedAddr = m_frameAllocator.alloc(sizeof(TexturedQuadPass));
        // need to call constructor to initialize vtable, that's why need to use placement new
        TexturedQuadPass* pass = new (preallocatedAddr) TexturedQuadPass(renderTarget, viewport, srcTexture);
        m_renderState.addRenderPass(pass);
    }

    void Renderer::addBloomPass()
    {
        void* preallocatedAddr = m_frameAllocator.alloc(sizeof(BloomPass));
        Texture* sceneColorTexture = m_bSuperSampleAA ? m_sceneColorTextureSSAA : m_sceneColorTexture;
        Viewport viewport = {0};
        BloomPassInput inputs = { sceneColorTexture, m_bloomOutput };
        // placement new for initialization
        BloomPass* pass = new (preallocatedAddr) BloomPass(nullptr, viewport, inputs);
        m_renderState.addRenderPass(pass);
    }

    void Renderer::addGaussianBlurPass(RenderTarget* renderTarget, Viewport viewport, Texture* srcTexture, Texture* horiTexture, Texture* vertTexture, GaussianBlurInput input)
    {
        void* mem = m_frameAllocator.alloc(sizeof(GaussianBlurPass) * 2);
        GaussianBlurPass* depthBlurPass = 
            new (mem) GaussianBlurPass(renderTarget,
                viewport,
                srcTexture,
                horiTexture,
                vertTexture,
                input);
        m_renderState.addRenderPass(depthBlurPass);
    }
*/

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
        // clear per frame allocator
        m_frameAllocator.reset();

        if (m_opts.enableAA)
        {
            m_offscreenRenderWidth = m_SSAAWidth;
            m_offscreenRenderHeight = m_SSAAHeight;
        }
        else 
        {
            m_offscreenRenderWidth = m_windowWidth;
            m_offscreenRenderHeight = m_windowHeight;
        }
    }

    void Renderer::render(Scene* scene, const std::function<void()>& debugRender)
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
            if (m_opts.enableSSAO)
            {
                // scene depth & normal pass
                renderSceneDepthNormal(scene);
                // ssao pass
                ssao(scene->getActiveCamera());
            }
            // main scene pass
            renderScene(scene);
            // debug object pass
            debugRender();
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
#define gTexBinding(x) static_cast<u32>(GlobalTextureBindings##::##x)
    void Renderer::updateFrameGlobalData(Scene* scene, const Camera& camera)
    {
        // bind global draw data
        m_globalDrawData.view = camera.view;
        m_globalDrawData.projection = camera.projection;
        m_globalDrawData.numPointLights = (i32)scene->pLights.size();
        m_globalDrawData.numDirLights = (i32)scene->dLights.size();
        m_globalDrawData.ssao = m_opts.enableSSAO ? 1.f : 0.f;

        updateTransforms(scene);
        // bind lighting data
        updateLighting(scene);

        // bind global textures
        glBindTextureUnit(gTexBinding(SSAO), m_ssaoTexture->handle);

        // upload data to gpu
        glNamedBufferSubData(gDrawDataBuffer, 0, sizeof(GlobalDrawData), &m_globalDrawData);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<u32>(BufferBindings::DrawData), gDrawDataBuffer);
    }

    void Renderer::updateTransforms(Scene* scene)
    {
        if (sizeofVector(scene->g_globalTransforms) > gInstanceTransforms.kBufferSize)
            cyanError("Gpu global transform SBO overflow!");
        glNamedBufferSubData(gInstanceTransforms.SBO, 0, sizeofVector(scene->g_globalTransformMatrices), scene->g_globalTransformMatrices.data());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)BufferBindings::GlobalTransforms, gInstanceTransforms.SBO);
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
        for (u32 i = 0; i < scene->pLights.size(); ++i)
        {
            scene->pLights[i].update();
            gLighting.pointLights.emplace_back(scene->pLights[i].getData());
        }
        glNamedBufferSubData(gLighting.dirLightSBO, 0, sizeofVector(gLighting.dirLights), gLighting.dirLights.data());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gLighting.dirLightSBO);
        glNamedBufferSubData(gLighting.pointLightsSBO, 0, sizeofVector(gLighting.pointLights), gLighting.pointLights.data());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gLighting.pointLightsSBO);
        gLighting.irradianceProbe = scene->m_irradianceProbe;
        gLighting.reflectionProbe = scene->m_reflectionProbe;

        // shared BRDF lookup texture used in split sum approximation for image based lighting
        glBindTextureUnit(2, ReflectionProbe::getBRDFLookupTexture()->handle);

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
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<u32>(BufferBindings::DrawData), gDrawDataBuffer);
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
        // draw entities
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

        auto quad = getQuadMesh();
        m_ctx->setVertexArray(quad->m_vertexArray);
        m_ctx->drawIndexAuto(quad->m_vertexArray->numVerts());
        m_ctx->setDepthControl(DepthControl::kEnable);
    }

    void Renderer::bloom()
    {
        auto quad = getQuadMesh();

        // be aware that these functions modify 'renderTarget''s state
        auto downsample = [this, quad](Texture* dst, Texture* src, RenderTarget* renderTarget) {
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
            m_ctx->setVertexArray(quad->m_vertexArray);
            m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
            m_ctx->drawIndexAuto(quad->m_vertexArray->numVerts());

            m_ctx->setDepthControl(Cyan::DepthControl::kEnable);
            // glFinish();
        };

        auto upscale = [this, quad](Texture* dst, Texture* src, Texture* blend, RenderTarget* renderTarget, u32 stageIndex) {
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
            m_ctx->setVertexArray(quad->m_vertexArray);
            m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
            m_ctx->drawIndexAuto(quad->m_vertexArray->numVerts());

            m_ctx->setDepthControl(Cyan::DepthControl::kEnable);
            // glFinish();
        };

        // user have to make sure that renderTarget is compatible with 'dst', 'src', and 'scratch'
        auto gaussianBlur = [this, quad](Texture* dst, Texture* src, Texture* scratch, RenderTarget* renderTarget, i32 kernelIndex, i32 radius) {

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
                m_ctx->setVertexArray(quad->m_vertexArray);
                m_ctx->setPrimitiveType(PrimitiveType::TriangleList);
                m_ctx->drawIndexAuto(quad->m_vertexArray->numVerts());
                // glFinish();
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
                m_ctx->setVertexArray(quad->m_vertexArray);
                m_ctx->drawIndexAuto(quad->m_vertexArray->numVerts());
                // glFinish();
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
        m_ctx->setVertexArray(quad->m_vertexArray);
        m_ctx->drawIndexAuto(quad->m_vertexArray->numVerts());
        m_ctx->setDepthControl(Cyan::DepthControl::kEnable);
        // glFinish();

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
        auto quad = getQuadMesh();
        m_ctx->setVertexArray(quad->m_vertexArray);
        m_ctx->drawIndexAuto(quad->m_vertexArray->numVerts());
        m_ctx->setDepthControl(DepthControl::kEnable);

        m_finalColorTexture = m_compositeColorTexture;
    }
    
    void Renderer::renderUI(const std::function<void()>& callback)
    {
        // begin imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        // draw widgets
        callback();

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
}