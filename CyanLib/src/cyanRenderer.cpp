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

#include "Common.h"
#include "CyanRenderer.h"
#include "Material.h"
#include "MathUtils.h"

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

    Renderer::Renderer()
        : m_frameAllocator(1024u * 1024u),  // 1 megabytes
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
        m_ssao(1.f),
        m_globalDrawData{ },
        gLighting { }, 
        gDrawDataBuffer(-1)
    {
        if (!m_renderer)
        {
            m_renderer = this;
        }
        else
        {
            // FIXME: this does not prevent from creating new instance
            // ensure that we are not creating new instance of Renderer
            CYAN_ASSERT(0, "There should be only one instance of Renderer")
        }

        u_model = createUniform("s_model", Uniform::Type::u_mat4);
        u_cameraView = createUniform("s_view", Uniform::Type::u_mat4);
        u_cameraProjection = createUniform("s_projection", Uniform::Type::u_mat4);
        m_ssaoSamplePoints.setColor(glm::vec4(0.f, 1.f, 1.f, 1.f));
        m_ssaoSamplePoints.setViewProjection(u_cameraView, u_cameraProjection);
        // create shaders
        m_sceneDepthNormalShader = createShader("SceneDepthNormalShader", "../../shader/scene_depth_normal_v.glsl", "../../shader/scene_depth_normal_p.glsl");
        // initialize per frame shader draw data
        glCreateBuffers(1, &gDrawDataBuffer);
        glNamedBufferData(gDrawDataBuffer, sizeof(GlobalDrawData), &m_globalDrawData, GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &gLighting.dirLightSBO);
        glNamedBufferData(gLighting.dirLightSBO, gLighting.kDynamicLightBufferSize, nullptr, GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &gLighting.pointLightsSBO);
        glNamedBufferData(gLighting.pointLightsSBO, gLighting.kDynamicLightBufferSize, nullptr, GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &gInstanceTransforms.SBO);
        glNamedBufferData(gInstanceTransforms.SBO, gInstanceTransforms.kBufferSize, nullptr, GL_DYNAMIC_DRAW);
    }

    Renderer* Renderer::getSingletonPtr()
    {
        return m_renderer;
    }

    StackAllocator& Renderer::getAllocator()
    {
        return m_frameAllocator;
    }

    RenderTarget* Renderer::getSceneColorRenderTarget()
    {
        return m_bSuperSampleAA ? m_sceneColorRTSSAA : m_sceneColorRenderTarget;
    }

    RenderTarget* Renderer::getRenderOutputRenderTarget()
    {
        return m_outputRenderTarget;
    }

    Texture* Renderer::getRenderOutputTexture()
    {
        // if post-processing is enbaled
        // else return m_sceneColorTexture
        return m_outputColorTexture;
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
        spec.m_type = Texture::Type::TEX_2D;
        spec.m_format = Texture::ColorFormat::R16G16B16A16; 
        spec.m_dataType = Texture::DataType::Float;
        spec.m_width = m_SSAAWidth;
        spec.m_height = m_SSAAHeight;
        spec.m_min = Texture::Filter::LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        m_sceneColorTextureSSAA = textureManager->createTextureHDR("SceneColorTexSSAA", spec);
        m_sceneColorRTSSAA = createRenderTarget(m_SSAAWidth, m_SSAAHeight);
        m_sceneColorRTSSAA->attachColorBuffer(m_sceneColorTextureSSAA);
        {
            TextureSpec spec0 = { };
            spec0.m_type = Texture::Type::TEX_2D;
            spec0.m_format = Texture::ColorFormat::R32G32B32; 
            spec0.m_dataType = Texture::DataType::Float;
            spec0.m_width = m_SSAAWidth;
            spec0.m_height = m_SSAAHeight;
            spec0.m_min = Texture::Filter::LINEAR;
            spec0.m_mag = Texture::Filter::LINEAR;
            spec0.m_s = Texture::Wrap::CLAMP_TO_EDGE;
            spec0.m_t = Texture::Wrap::CLAMP_TO_EDGE;
            spec0.m_r = Texture::Wrap::CLAMP_TO_EDGE;
            // todo: it seems using rgb16f leads to precision issue when building ssao
            m_sceneDepthTextureSSAA = textureManager->createTextureHDR("SceneDepthTexSSAA", spec0);
            m_sceneNormalTextureSSAA = textureManager->createTextureHDR("SceneNormalTextureSSAA", spec0);
            m_sceneColorRTSSAA->attachColorBuffer(m_sceneNormalTextureSSAA);
            m_sceneColorRTSSAA->attachColorBuffer(m_sceneDepthTextureSSAA);
        }

        // scene color render targets 
        spec.m_width = m_windowWidth;
        spec.m_height = m_windowHeight;
        m_sceneColorTexture = textureManager->createTextureHDR("SceneColorTexture", spec);
        m_sceneColorRenderTarget = createRenderTarget(m_windowWidth, m_windowHeight);
        m_sceneColorRenderTarget->attachColorBuffer(m_sceneColorTexture);
        {
            TextureSpec spec0 = { };
            spec0.m_type = Texture::Type::TEX_2D;
            spec0.m_format = Texture::ColorFormat::R32G32B32; 
            spec0.m_dataType = Texture::DataType::Float;
            spec0.m_width = m_windowWidth;
            spec0.m_height = m_windowHeight;
            spec0.m_min = Texture::Filter::LINEAR;
            spec0.m_mag = Texture::Filter::LINEAR;
            spec0.m_s = Texture::Wrap::CLAMP_TO_EDGE;
            spec0.m_t = Texture::Wrap::CLAMP_TO_EDGE;
            spec0.m_r = Texture::Wrap::CLAMP_TO_EDGE;
            m_sceneNormalTexture = textureManager->createTextureHDR("SceneNormalTexture", spec0);
            spec0.m_format = Texture::ColorFormat::R32F; 
            m_sceneDepthTexture = textureManager->createTextureHDR("SceneDepthTexture", spec0);
            m_sceneColorRenderTarget->attachColorBuffer(m_sceneNormalTexture);
            m_sceneColorRenderTarget->attachColorBuffer(m_sceneDepthTexture);
        }

        m_outputColorTexture = textureManager->createTextureHDR("FinalColorTexture", spec);
        m_outputRenderTarget = createRenderTarget(spec.m_width, spec.m_height);
        m_outputRenderTarget->attachColorBuffer(m_outputColorTexture);

        // voxel
        m_voxelGridResolution = 512u;
        // create render target
        m_voxelRenderTarget = Cyan::createRenderTarget(m_voxelGridResolution, m_voxelGridResolution);
        TextureSpec voxelSpec = { };
        // TODO: using low resolution voxel grid for now because we are regenerating
        // mipmap per frame
        voxelSpec.m_width = m_voxelGridResolution;
        voxelSpec.m_height = m_voxelGridResolution;
        voxelSpec.m_type = Texture::Type::TEX_2D;
        voxelSpec.m_dataType = Texture::DataType::Float;
        voxelSpec.m_format = Texture::ColorFormat::R16G16B16;
        voxelSpec.m_min = Texture::Filter::LINEAR; 
        voxelSpec.m_mag = Texture::Filter::LINEAR;
        voxelSpec.m_numMips = 1;
        m_voxelColorTexture = textureManager->createTexture("Voxelization", voxelSpec);
        {
            TextureSpec visSpec = { };
            visSpec.m_width = 320;
            visSpec.m_height = 180;
            visSpec.m_type = Texture::Type::TEX_2D;
            visSpec.m_dataType = Texture::DataType::Float;
            visSpec.m_format = Texture::ColorFormat::R16G16B16;
            visSpec.m_min = Texture::Filter::LINEAR; 
            visSpec.m_mag = Texture::Filter::LINEAR;
            visSpec.m_numMips = 1;
            m_voxelVisColorTexture = textureManager->createTexture("VoxelVis", visSpec);
            m_voxelVisRenderTarget = createRenderTarget(visSpec.m_width, visSpec.m_height);
            m_voxelVisRenderTarget->attachColorBuffer(m_voxelVisColorTexture);
        }
        m_voxelRenderTarget->attachColorBuffer(m_voxelColorTexture);
        m_voxelVolumeTexture = new Texture;

        TextureSpec voxelDataSpec = { };
        voxelDataSpec.m_width = m_voxelGridResolution;
        voxelDataSpec.m_height = m_voxelGridResolution;
        voxelDataSpec.m_depth = m_voxelGridResolution;
        voxelDataSpec.m_type = Texture::Type::TEX_3D;
        voxelDataSpec.m_dataType = Texture::DataType::UNSIGNED_INT;
        voxelDataSpec.m_format = Texture::ColorFormat::R8G8B8A8;
        voxelDataSpec.m_min = Texture::Filter::LINEAR; 
        voxelDataSpec.m_mag = Texture::Filter::LINEAR;
        voxelDataSpec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        voxelDataSpec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        voxelDataSpec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
        voxelDataSpec.m_numMips = 1;
        m_voxelData.m_albedo = textureManager->createTexture3D("VoxelAlbedo", voxelDataSpec);
        m_voxelData.m_normal = textureManager->createTexture3D("VoxelNormal", voxelDataSpec);
        m_voxelData.m_emission = textureManager->createTexture3D("VoxelEmission", voxelDataSpec);
    }

    void Renderer::initShaders()
    {
        m_lumHistogramShader = glCreateShader(GL_COMPUTE_SHADER);
        const char* src = ShaderUtil::readShaderFile("../../shader/shader_lumin_histogram_c.glsl");
        glShaderSource(m_lumHistogramShader, 1, &src, nullptr);
        glCompileShader(m_lumHistogramShader);
        ShaderUtil::checkShaderCompilation(m_lumHistogramShader);
        m_lumHistogramProgram = glCreateProgram();
        glAttachShader(m_lumHistogramProgram, m_lumHistogramShader);
        glLinkProgram(m_lumHistogramProgram);
        ShaderUtil::checkShaderLinkage(m_lumHistogramProgram);
    }

    void Renderer::initialize(glm::vec2 windowSize)
    {
        onRendererInitialized(windowSize);

        m_viewport = { static_cast<u32>(0u), 
                       static_cast<u32>(0u), 
                       static_cast<u32>(windowSize.x), 
                       static_cast<u32>(windowSize.y) };

        // misc
        m_bSuperSampleAA = true;
        m_exposure = 1.f;
        m_bloom = true;

        // voxel
        m_voxelVisShader = createShader("VoxelVisShader", "../../shader/shader_render_voxel.vs", "../../shader/shader_render_voxel.fs");
        // create shader
        m_voxelizeShader = createVsGsPsShader("VoxelizeShader", 
                                                   "../../shader/shader_voxel.vs", 
                                                   "../../shader/shader_voxel.gs", 
                                                   "../../shader/shader_voxel.fs");
        m_voxelVisMatl = createMaterial(m_voxelVisShader)->createInstance();
        m_voxelizeMatl = createMaterial(m_voxelizeShader)->createInstance(); 

        // set back-face culling
        Cyan::getCurrentGfxCtx()->setCullFace(FrontFace::CounterClockWise, FaceCull::Back);

        // render targets
        initShaders();
        initRenderTargets(m_viewport.m_width, m_viewport.m_height);

        // ssao
        {
            m_freezeDebugLines = false;
            auto textureManager = TextureManager::getSingletonPtr();
            TextureSpec spec = { };
            spec.m_width = m_windowWidth;
            spec.m_height = m_windowHeight;
            spec.m_type = Texture::Type::TEX_2D;
            spec.m_format = Texture::ColorFormat::R16G16B16; 
            spec.m_dataType = Texture::DataType::Float;
            spec.m_min = Texture::Filter::LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
            m_ssaoRenderTarget = createRenderTarget(spec.m_width, spec.m_height);
            m_ssaoTexture = textureManager->createTextureHDR("SSAOTexture", spec);
            m_ssaoRenderTarget->attachColorBuffer(m_ssaoTexture);
            m_ssaoShader = createShader("SSAOShader", "../../shader/shader_ao.vs", "../../shader/shader_ao.fs");
            m_ssaoMatl = createMaterial(m_ssaoShader)->createInstance();
            m_ssaoDebugVisBuffer = createRegularBuffer(sizeof(SSAODebugVisData));

            glm::mat4 model(1.f);
            m_ssaoDebugVisLines.normal.init();
            m_ssaoDebugVisLines.normal.setColor(glm::vec4(0.f, 0.f, 1.f, 1.f));
            m_ssaoDebugVisLines.normal.setModel(model);
            m_ssaoDebugVisLines.normal.setViewProjection(u_cameraView, u_cameraProjection);

            m_ssaoDebugVisLines.projectedNormal.init();
            m_ssaoDebugVisLines.projectedNormal.setColor(glm::vec4(1.f, 1.f, 0.f, 1.f));
            m_ssaoDebugVisLines.projectedNormal.setModel(model);
            m_ssaoDebugVisLines.projectedNormal.setViewProjection(u_cameraView, u_cameraProjection);

            m_ssaoDebugVisLines.wo.init();
            m_ssaoDebugVisLines.wo.setColor(glm::vec4(1.f, 0.f, 1.f, 1.f));
            m_ssaoDebugVisLines.wo.setModel(model);
            m_ssaoDebugVisLines.wo.setViewProjection(u_cameraView, u_cameraProjection);

            m_ssaoDebugVisLines.sliceDir.init();
            m_ssaoDebugVisLines.sliceDir.setColor(glm::vec4(1.f, 1.f, 1.f, 1.f));
            m_ssaoDebugVisLines.sliceDir.setModel(model);
            m_ssaoDebugVisLines.sliceDir.setViewProjection(u_cameraView, u_cameraProjection);

            for (int i = 0; i < ARRAY_COUNT(m_ssaoDebugVisLines.samples); ++i)
            {
                m_ssaoDebugVisLines.samples[i].init();
                m_ssaoDebugVisLines.samples[i].setColor(glm::vec4(1.f, 0.f, 0.f, 1.f));
                m_ssaoDebugVisLines.samples[i].setModel(model);
                m_ssaoDebugVisLines.samples[i].setViewProjection(u_cameraView, u_cameraProjection);
            }
        }

        {
            // misc
            m_debugCam.position = glm::vec3(0.f, 50.f, 0.f);
            m_debugCam.lookAt = glm::vec3(0.f);
            m_debugCam.fov = 50.f;
            m_debugCam.n = 0.1f;
            m_debugCam.f = 100.f;
            m_debugCam.projection = glm::perspective(m_debugCam.fov, 16.f/9.f, m_debugCam.n, m_debugCam.f);
            CameraManager::updateCamera(m_debugCam);
            GLint kMaxNumColorBuffers = 0;
            glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &kMaxNumColorBuffers);
        }

        // load global glsl definitions and save them in a map <path, content>
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

    void Renderer::drawMeshInstance(MeshInstance* meshInstance, i32 transformIndex)
    {
        Mesh* mesh = meshInstance->m_mesh;
        for (u32 i = 0; i < mesh->m_subMeshes.size(); ++i)
        {
            auto ctx = Cyan::getCurrentGfxCtx();
            MaterialInstance* matl = meshInstance->m_matls[i]; 
            Shader* shader = matl->getShader();
            ctx->setShader(shader);
            matl->set("transformIndex", transformIndex);
            UsedBindingPoints used = matl->bind();
            Mesh::SubMesh* sm = mesh->m_subMeshes[i];
            ctx->setVertexArray(sm->m_vertexArray);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            if (sm->m_vertexArray->hasIndexBuffer())
                ctx->drawIndex(sm->m_numIndices);
            else
                ctx->drawIndexAuto(sm->m_numVerts);
        }
    }

    Cyan::Texture* Renderer::voxelizeMesh(MeshInstance* mesh, glm::mat4* modelMatrix)
    {
        glEnable(GL_NV_conservative_raster);
        // clear voxel grid textures
        glClearTexImage(m_voxelData.m_albedo->m_id, 0, GL_RGBA, GL_UNSIGNED_INT, 0);
        glClearTexImage(m_voxelData.m_normal->m_id, 0, GL_RGBA, GL_UNSIGNED_INT, 0);

        auto ctx = getCurrentGfxCtx();
        ctx->setDepthControl(DepthControl::kDisable);
        // set shader
        ctx->setShader(m_voxelizeShader);
        // set render target
        ctx->setRenderTarget(m_voxelRenderTarget, 0u);
        ctx->setClearColor(glm::vec4(0.1f));
        ctx->clear();
        Viewport originViewport = ctx->m_viewport;
        ctx->setViewport({ 0, 0, m_voxelGridResolution, m_voxelGridResolution });
        BoundingBox3f aabb = mesh->getAABB();
        glm::vec3 aabbMin = *modelMatrix * aabb.m_pMin;
        glm::vec3 aabbMax = *modelMatrix * aabb.m_pMax;

        MaterialInstance* boundMatl = mesh->getMaterial(0);
        Texture* albedo = boundMatl->getTexture("diffuseMaps[0]");
        Texture* normal = boundMatl->getTexture("normalMap");
        m_voxelizeMatl->bindTexture("albedoMap", albedo);
        m_voxelizeMatl->bindTexture("normalMap", normal);
        // bind 3D volume texture to image units
        glBindImageTexture(0, m_voxelData.m_albedo->m_id, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(1, m_voxelData.m_normal->m_id, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

        m_voxelizeMatl->set("model", modelMatrix);
        m_voxelizeMatl->set("aabbMin", &aabbMin.x);
        m_voxelizeMatl->set("aabbMax", &aabbMax.x);
        m_voxelizeMatl->bind();
        ctx->setPrimitiveType(PrimitiveType::TriangleList);
        // draw mesh
        auto meshDef = mesh->m_mesh;
        for (auto sm : meshDef->m_subMeshes)
        {
            ctx->setVertexArray(sm->m_vertexArray);
            if (ctx->m_vertexArray->m_ibo != static_cast<u32>(-1)) {
                ctx->drawIndex(ctx->m_vertexArray->m_numIndices);
            } else {
                ctx->drawIndexAuto(sm->m_numVerts);
            }
        }
        ctx->setDepthControl(DepthControl::kEnable);
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
        glClearTexImage(m_voxelData.m_albedo->m_id, 0, GL_RGBA, GL_UNSIGNED_INT, 0);
        glClearTexImage(m_voxelData.m_normal->m_id, 0, GL_RGBA, GL_UNSIGNED_INT, 0);

        auto ctx = getCurrentGfxCtx();
        ctx->setDepthControl(DepthControl::kDisable);
        // set shader
        ctx->setShader(m_voxelizeShader);
        // set render target
        ctx->setRenderTarget(m_voxelRenderTarget, 0u);
        ctx->setClearColor(glm::vec4(0.1f));
        ctx->clear();
        Viewport originViewport = ctx->m_viewport;
        ctx->setViewport({ 0, 0, m_voxelGridResolution, m_voxelGridResolution });
        ctx->setPrimitiveType(PrimitiveType::TriangleList);
        // bind 3D volume texture to image units
        glBindImageTexture(0, m_voxelData.m_albedo->m_id, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(1, m_voxelData.m_normal->m_id, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

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
                        ctx->setVertexArray(sm->m_vertexArray);
                        if (sm->m_vertexArray->m_ibo != static_cast<u32>(-1))
                        {
                            ctx->drawIndex(sm->m_vertexArray->m_numIndices);
                        }
                        else
                        {
                            ctx->drawIndexAuto(sm->m_numVerts);
                        }

                        m_voxelizeMatl->set("flag", 1);
                        m_voxelizeMatl->bind();
                        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                        // y-axis
                        if (sm->m_vertexArray->m_ibo != static_cast<u32>(-1))
                        {
                            ctx->drawIndex(sm->m_vertexArray->m_numIndices);
                        }
                        else
                        {
                            ctx->drawIndexAuto(sm->m_numVerts);
                        }

                        m_voxelizeMatl->set("flag", 2);
                        m_voxelizeMatl->bind();
                        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                        // z-axis
                        if (sm->m_vertexArray->m_ibo != static_cast<u32>(-1))
                        {
                            ctx->drawIndex(sm->m_vertexArray->m_numIndices);
                        }
                        else
                        {
                            ctx->drawIndexAuto(sm->m_numVerts);
                        }
                    }
                }
            }
        };     

        customSceneVisitor(scene, nodeCallback);

        ctx->setDepthControl(DepthControl::kEnable);
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
        auto ctx = getCurrentGfxCtx();
        ctx->setRenderTarget(m_voxelVisRenderTarget, 0u);
        ctx->clear();
        ctx->setViewport({0u, 0u, m_voxelVisRenderTarget->m_width, m_voxelVisRenderTarget->m_height});
        ctx->setShader(m_voxelVisShader);
        QuadMesh* quad = getQuadMesh();
        ctx->setDepthControl(DepthControl::kDisable);
        ctx->setVertexArray(quad->m_vertexArray);
        m_voxelVisMatl->bindTexture("albedoMap", m_voxelData.m_albedo);
        m_voxelVisMatl->bindTexture("normalMap", m_voxelData.m_normal);
        m_voxelVisMatl->set("cameraPos", &scene->getActiveCamera().position.x);
        m_voxelVisMatl->set("cameraLookAt", &scene->getActiveCamera().lookAt.x);
        m_voxelVisMatl->set("aabbMin", &sceneAABB.m_pMin.x);
        m_voxelVisMatl->set("aabbMax", &sceneAABB.m_pMax.x);
        m_voxelVisMatl->bind();
        ctx->drawIndexAuto(6u);
        ctx->setDepthControl(DepthControl::kEnable);
        return m_voxelVisColorTexture;
    }

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

    void Renderer::addDirectionalShadowPass(Scene* scene, Camera& camera, u32 lightIdx)
    {
        void* preallocatedAddr = m_frameAllocator.alloc(sizeof(DirectionalShadowPass));
        Viewport viewport = { 0, 0, DirectionalShadowPass::s_depthRenderTarget->m_width, DirectionalShadowPass::s_depthRenderTarget->m_height };
        DirectionalShadowPass* pass = new (preallocatedAddr) DirectionalShadowPass(0, viewport, scene, camera, lightIdx);
        m_renderState.addRenderPass(pass);

        switch (DirectionalShadowPass::m_cascadedShadowMap.m_technique)
        {
            case ShadowTechnique::kVariance_Shadow:
            {
                GaussianBlurInput inputs = { };
                inputs.kernelIndex = 0;
                inputs.radius = 3;

                for (u32 i = 0; i < (DirectionalShadowPass::kNumShadowCascades / 2); ++i)
                {
                    auto& cascade = DirectionalShadowPass::m_cascadedShadowMap.cascades[i];
                    void* mem = m_frameAllocator.alloc(sizeof(GaussianBlurPass) * 2);
                    GaussianBlurPass* depthBlurPass = 
                        new (mem) GaussianBlurPass(DirectionalShadowPass::s_depthRenderTarget,
                            viewport,
                            cascade.varianceShadowMap.shadowMap,
                            DirectionalShadowPass::m_horizontalBlurTex,
                            cascade.varianceShadowMap.shadowMap,
                            inputs);
                    m_renderState.addRenderPass(depthBlurPass);
                }

            } break;
            case ShadowTechnique::kPCF_Shadow:
                break;
        }
    }

    void Renderer::addPostProcessPasses()
    {
        //TODO: bloom right now takes about 7ms to run, need improve performance!
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

    void Renderer::addLinePass()
    {

    }

    // TODO:
    void Renderer::submitMesh(Mesh* mesh, glm::mat4 modelTransform)
    {

    }

    void Renderer::renderFrame()
    {
    }

    void Renderer::drawEntity(Entity* entity) 
    {
        // drawSceneNode(entity->m_sceneRoot);
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

    void Renderer::drawSceneNode(SceneNode* node)
    {
        if (node->m_meshInstance)
            drawMeshInstance(node->m_meshInstance, node->globalTransform);
        for (auto* child : node->m_child)
        {
            drawSceneNode(child);
        }
    }

    void Renderer::endFrame()
    {
        Cyan::getCurrentGfxCtx()->flip();
    }
    /*
        * render passes should be pushed after call to beginRender() 
    */
    void Renderer::beginRender()
    {
        // set render target to m_defaultRenderTarget
        auto ctx = Cyan::getCurrentGfxCtx();

        // clear per frame allocator
        m_frameAllocator.reset();
        m_renderState.clearRenderTargets();
        m_renderState.m_superSampleAA = m_bSuperSampleAA;
        if (m_bSuperSampleAA)
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

    void Renderer::render(Scene* scene)
    {
        // update all global data
        updateFrameGlobalData(scene, scene->cameras[scene->activeCamera]);
        for (auto renderPass : m_renderState.m_renderPasses)
        {
            renderPass->render();
        }
        m_renderState.clearRenderPasses();
    }

    void Renderer::endRender()
    {
        // reset render target
        Cyan::getCurrentGfxCtx()->setRenderTarget(nullptr, 0u);
    }

    void Renderer::renderDebugObjects()
    {

    }

    void Renderer::drawMesh(Mesh* mesh)
    {
        auto ctx = getCurrentGfxCtx();
        for (u32 sm = 0; sm < mesh->numSubMeshes(); ++sm)
        {
            auto subMesh = mesh->m_subMeshes[sm];
            ctx->setVertexArray(subMesh->m_vertexArray);
            if (subMesh->m_vertexArray->hasIndexBuffer())
                ctx->drawIndex(subMesh->m_vertexArray->m_numIndices);
            else ctx->drawIndexAuto(subMesh->m_vertexArray->numVerts());
        }
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
        m_globalDrawData.m_ssao = m_ssao;

        // bind lighting data
        updateTransforms(scene);
        updateLighting(scene);

        // bind global textures
        glBindTextureUnit(gTexBinding(SSAO), m_ssaoTexture->m_id);

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
        gLighting.distantProbe    = scene->m_distantProbe;
        gLighting.irradianceProbe = scene->m_irradianceProbe;
        gLighting.reflectionProbe = scene->m_reflectionProbe;
        // global distant probe
        if (gLighting.distantProbe)
        {
            glBindTextureUnit(0, gLighting.distantProbe->m_diffuse->m_id);
            glBindTextureUnit(1, gLighting.distantProbe->m_specular->m_id);
            glBindTextureUnit(2, gLighting.distantProbe->m_brdfIntegral->m_id);
        }
        // local GI probes
        if (gLighting.irradianceProbe)
            glBindTextureUnit(3, gLighting.irradianceProbe->m_convolvedIrradianceTexture->m_id);

        if (gLighting.reflectionProbe)
            glBindTextureUnit(4, gLighting.reflectionProbe->m_convolvedReflectionTexture->m_id);
    }

    void Renderer::updateSunShadow()
    {
        CascadedShadowMap& csm          = DirectionalShadowPass::m_cascadedShadowMap;
        m_globalDrawData.sunLightView   = csm.lightView;
        // bind sun light shadow for this frame
        u32 textureUnitOffset = gTexBinding(SunShadow);
        for (u32 i = 0; i < DirectionalShadowPass::kNumShadowCascades; ++i)
        {
            m_globalDrawData.sunShadowProjections[i] = csm.cascades[i].lightProjection;
            if (csm.m_technique == kVariance_Shadow)
                glBindTextureUnit(textureUnitOffset + i, csm.cascades[i].varianceShadowMap.shadowMap->m_id);
            else if (csm.m_technique == kPCF_Shadow)
                glBindTextureUnit(textureUnitOffset + i, csm.cascades[i].basicShadowMap.shadowMap->m_id);
        }

        // upload data to gpu
        glNamedBufferSubData(gDrawDataBuffer, 0, sizeof(GlobalDrawData), &m_globalDrawData);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<u32>(BufferBindings::DrawData), gDrawDataBuffer);
    }

    void Renderer::renderSceneDepthNormal(Scene* scene, Camera& camera)
    {
        auto ctx = getCurrentGfxCtx();
        ctx->setShader(m_sceneDepthNormalShader);
        // entities 
        for (u32 e = 0; e < (u32)scene->entities.size(); ++e)
        {
            auto entity = scene->entities[e];
            if (entity->m_includeInGBufferPass && entity->m_visible)
            {
                executeOnEntity(entity, [this, ctx](SceneNode* node) { 
                    if (node->m_meshInstance)
                    {
                        m_sceneDepthNormalShader->setUniform1i("transformIndex", node->globalTransform);
                        drawMesh(node->m_meshInstance->m_mesh);
                    }
                });
            }
        }
    }

    void Renderer::renderScene(Scene* scene, Camera& camera)
    {
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

    void Renderer::renderSSAO(Camera& camera)
    {
        auto ctx = getCurrentGfxCtx();
        ctx->setShader(m_ssaoShader);
        ctx->setRenderTarget(m_ssaoRenderTarget, 0u);
        ctx->setViewport({0, 0, m_ssaoRenderTarget->m_width, m_ssaoRenderTarget->m_height});
        ctx->clear();
        ctx->setDepthControl(kDisable);
        m_ssaoMatl->bindTexture("normalTexture", m_sceneNormalTextureSSAA);
        m_ssaoMatl->bindTexture("depthTexture", m_sceneDepthTextureSSAA);
        m_ssaoMatl->set("cameraPos", &camera.position.x);
        m_ssaoMatl->set("view", &camera.view[0]);
        m_ssaoMatl->set("projection", &camera.projection[0]);
        m_ssaoMatl->bindBuffer("DebugVisData", m_ssaoDebugVisBuffer);
        m_ssaoMatl->bind();
        auto quad = getQuadMesh();
        ctx->setVertexArray(quad->m_vertexArray);
        ctx->drawIndexAuto(quad->m_vertexArray->numVerts());
        ctx->setDepthControl(DepthControl::kEnable);
    }

    void Renderer::renderSceneToLightProbe(Scene* scene, Camera& camera)
    {
        // turn off ssao
        m_ssao = 0.f;
        updateFrameGlobalData(scene, camera);
        // entities 
        for (auto entity : scene->entities)
        {
            if (entity->m_static)
                drawEntity(entity);
        }
        m_ssao = 1.f;
    }
}