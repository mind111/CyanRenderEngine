// TODO: Deal with name conflicts with windows header
#include <iostream>
#include <fstream>

#include "glfw3.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
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
        m_gpuLightingData{nullptr, nullptr, {}, {}, nullptr, nullptr, true},
        u_model(0),
        u_cameraView(0),
        u_cameraProjection(0),
        m_pointLightsBuffer(0),
        m_dirLightsBuffer(0),   
        m_bSuperSampleAA(true),
        m_offscreenRenderWidth(1280u),
        m_offscreenRenderHeight(720u),
        m_windowWidth(1280u),
        m_windowHeight(720u),
        m_sceneColorTexture(0),
        m_sceneColorRenderTarget(0),
        m_sceneColorTextureSSAA(0),
        m_sceneColorRTSSAA(0),
        m_voxelData{0},
        m_ssaoSamplePoints(32)
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

        m_gpuLightingData.pointLightsBuffer = createRegularBuffer(sizeof(PointLightGpuData) * 20);
        m_gpuLightingData.dirLightsBuffer = createRegularBuffer(sizeof(DirLightGpuData) * 1);
        m_ssaoSamplePoints.setColor(glm::vec4(0.f, 1.f, 1.f, 1.f));
        m_ssaoSamplePoints.setViewProjection(u_cameraView, u_cameraProjection);
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
        const char* src = ShaderUtil::readShaderFile("../../shader/shader_lumin_histogram.cs");
        glShaderSource(m_lumHistogramShader, 1, &src, nullptr);
        glCompileShader(m_lumHistogramShader);
        ShaderUtil::checkShaderCompilation(m_lumHistogramShader);
        m_lumHistogramProgram = glCreateProgram();
        glAttachShader(m_lumHistogramProgram, m_lumHistogramShader);
        glLinkProgram(m_lumHistogramProgram);
        ShaderUtil::checkShaderLinkage(m_lumHistogramProgram);
    }

    void Renderer::init(glm::vec2 windowSize)
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

        // scene depth and normal
        {
            m_sceneDepthNormalShader = createShader("SceneDepthNormalShader", "../../shader/shader_depth_normal.vs", "../../shader/shader_depth_normal.fs");
        }
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

    void Renderer::drawMeshInstance(MeshInstance* meshInstance, glm::mat4* modelMatrix)
    {
        Mesh* mesh = meshInstance->m_mesh;

        for (u32 i = 0; i < mesh->m_subMeshes.size(); ++i)
        {
            auto ctx = Cyan::getCurrentGfxCtx();
            MaterialInstance* matl = meshInstance->m_matls[i]; 
            Shader* shader = matl->getShader();
            ctx->setShader(shader);
            if (modelMatrix)
            {
                // TODO: this is obviously redumatl->m_shaderndant
                Cyan::setUniform(u_model, &(*modelMatrix)[0]);
                ctx->setUniform(u_model);
            }
            ctx->setUniform(u_cameraView);
            ctx->setUniform(u_cameraProjection);

            // TODO: clean up the logic regarding when and where to bind shadow map
            CascadedShadowMap& cascadedShadowMap = DirectionalShadowPass::m_cascadedShadowMap;
            matl->set("lightView", &cascadedShadowMap.lightView[0]);
            switch (cascadedShadowMap.m_technique)
            {
                case kVariance_Shadow:
                    for (u32 s = 0; s < DirectionalShadowPass::kNumShadowCascades; ++s)
                    {
                        char samplerName0[64];
                        sprintf_s(samplerName0, "shadowCascades[%d].depthMap", s);
                        matl->bindTexture(samplerName0, cascadedShadowMap.cascades[s].varianceShadowMap.shadowMap);

                        char projectionName[64];
                        sprintf_s(projectionName, "shadowCascades[%d].lightProjection", s);
                        matl->set(projectionName, &cascadedShadowMap.cascades[s].lightProjection[0]);
                    }
                    break;
                case kPCF_Shadow:
                    for (u32 s = 0; s < DirectionalShadowPass::kNumShadowCascades; ++s)
                    {
                        char samplerName0[64];
                        sprintf_s(samplerName0, "shadowCascades[%d].depthMap", s);
                        matl->bindTexture(samplerName0, cascadedShadowMap.cascades[s].basicShadowMap.shadowMap);

                        char projectionName[64];
                        sprintf_s(projectionName, "shadowCascades[%d].lightProjection", s);
                        matl->set(projectionName, &cascadedShadowMap.cascades[s].lightProjection[0]);
                    }
                    break;
                default:
                    CYAN_ASSERT(0, "Unknown shadow technique.")
            }

            UsedBindingPoints used = matl->bind();
            Mesh::SubMesh* sm = mesh->m_subMeshes[i];
            ctx->setVertexArray(sm->m_vertexArray);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            if (sm->m_vertexArray->m_ibo != static_cast<u32>(-1)) {
                ctx->drawIndex(sm->m_numIndices);
            } else {
                ctx->drawIndexAuto(sm->m_numVerts);
            }
            // NOTES: reset texture units because texture unit bindings are managed by gl context 
            // it won't change when binding different shaders
            for (u32 t = 0; t < used.m_usedTexBindings; ++t)
            {
                ctx->setTexture(nullptr, t);
            }
            // NOTES: reset shader storage binding points
            for (u32 p = 0; p < used.m_usedBufferBindings; ++p)
            {
                ctx->setBuffer(nullptr, p);
            }
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
                    // TODO: add gaussian blur pass really soften the shadow, but the shadow becomes almost too light
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

    void Renderer::addEntityPass(RenderTarget* renderTarget, Viewport viewport, std::vector<Entity*>& entities, LightingEnvironment& lighting, Camera& camera)
    {
        void* frameMem = m_frameAllocator.alloc(sizeof(EntityPass));
        EntityPass* pass = new (frameMem) EntityPass(renderTarget, viewport, entities, lighting, camera);
        m_renderState.addRenderPass(pass);
    }

    void Renderer::addGaussianBlurPass()
    {

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
        drawSceneNode(entity->m_sceneRoot);
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
        {
            MeshInstance* meshInstance = node->m_meshInstance; 
            Material* materialType = meshInstance->m_matls[0]->m_template;
            u32 numSubMeshs = (u32)meshInstance->m_mesh->m_subMeshes.size();

            // update lighting data if material can be lit
            if (materialType->m_dataFieldsFlag && (1 << Material::DataFields::Lit))
            {
                for (u32 sm = 0; sm < numSubMeshs; ++sm)
                {
                    meshInstance->m_matls[sm]->set("numPointLights", static_cast<u32>(m_gpuLightingData.pLights.size()));
                    meshInstance->m_matls[sm]->set("numDirLights", static_cast<u32>(m_gpuLightingData.dLights.size()));
                    meshInstance->m_matls[sm]->bindBuffer("dirLightsData", m_gpuLightingData.dirLightsBuffer);
                    meshInstance->m_matls[sm]->bindBuffer("pointLightsData", m_gpuLightingData.pointLightsBuffer);

                    // update probe texture bindings
                    meshInstance->m_matls[sm]->bindTexture("irradianceDiffuse", m_gpuLightingData.probe->m_diffuse);
                    meshInstance->m_matls[sm]->bindTexture("irradianceSpecular", m_gpuLightingData.probe->m_specular);
                    meshInstance->m_matls[sm]->bindTexture("brdfIntegral", m_gpuLightingData.probe->m_brdfIntegral);

                    // GI probes
                    if (m_gpuLightingData.irradianceProbe)
                        meshInstance->m_matls[sm]->bindTexture("gLighting.irradianceProbe", m_gpuLightingData.irradianceProbe->m_irradianceMap);
                }
            }

            glm::mat4 modelMatrix;
            modelMatrix = node->m_worldTransform.toMatrix();
            modelMatrix = modelMatrix;
            drawMeshInstance(node->m_meshInstance, &modelMatrix);
        } 
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

    void Renderer::render()
    {
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

    // TODO: implement this
    void Renderer::renderSceneDepthNormal(Scene* scene, Camera& camera)
    {
        auto ctx = getCurrentGfxCtx();
        auto renderTarget = m_renderState.m_superSampleAA ? m_sceneColorRTSSAA : m_sceneColorRenderTarget;
        // camera
        setUniform(u_cameraView, (void*)&camera.view[0]);
        setUniform(u_cameraProjection, (void*)&camera.projection[0]);
        // entities 
        for (auto entity : scene->entities)
        {
            for (auto node : entity->m_sceneRoot->m_child)
            {
                if (node->m_meshInstance)
                {
                    continue;
                    i32 drawBuffers[2] = { 1u, 2u };
                    ctx->setRenderTarget(renderTarget, drawBuffers, 2u);
                    ctx->setShader(m_sceneDepthNormalShader);
                }
            }
        }
    }

    void Renderer::renderEntities(std::vector<Entity*>& entities, LightingEnvironment& lighting, Camera& camera)
    {
        // camera
        setUniform(u_cameraView, (void*)&camera.view[0]);
        setUniform(u_cameraProjection, (void*)&camera.projection[0]);
        // lights
        uploadGpuLightingData(lighting);
        for (auto entity : entities)
        {
            drawEntity(entity);
        }
    }

    void Renderer::uploadGpuLightingData(LightingEnvironment& lighting)
    {
        m_gpuLightingData.dLights.clear();
        m_gpuLightingData.pLights.clear();

        for (auto& light : lighting.m_dirLights)
        {
            light.update();
            m_gpuLightingData.dLights.push_back(light.getData());
        }
        for (auto& light : lighting.m_pLights)
        {
            light.update();
            m_gpuLightingData.pLights.push_back(light.getData());
        }

        setBuffer(m_gpuLightingData.pointLightsBuffer, m_gpuLightingData.pLights.data(), sizeofVector(m_gpuLightingData.pLights));
        setBuffer(m_gpuLightingData.dirLightsBuffer, m_gpuLightingData.dLights.data(), sizeofVector(m_gpuLightingData.dLights));

        m_gpuLightingData.probe = lighting.m_probe;
        m_gpuLightingData.irradianceProbe = lighting.m_irradianceProbe;
    }

    void Renderer::probeRenderScene(Scene* scene, Camera& camera)
    {
        // camera
        setUniform(u_cameraView, (void*)&camera.view[0]);
        setUniform(u_cameraProjection, (void*)&camera.projection[0]);

        // lights
        LightingEnvironment lighting = { 
            scene->pLights,
            scene->dLights,
            scene->m_currentProbe,
            false
        };
        uploadGpuLightingData(lighting);

        // entities 
        for (auto entity : scene->entities)
        {
            if (entity->m_bakeInProbes)
                drawEntity(entity);
        }
    }

    // TODO: render scene depth & normal to compute ao results first
    void Renderer::renderScene(Scene* scene, Camera& camera)
    {
        // camera
        setUniform(u_cameraView, (void*)&camera.view[0]);
        setUniform(u_cameraProjection, (void*)&camera.projection[0]);

#if DRAW_DEBUG
        DirectionalShadowPass::drawDebugLines(u_cameraView, u_cameraProjection);
#endif

        // lights
        LightingEnvironment lighting = { 
            scene->pLights,
            scene->dLights,
            scene->m_currentProbe,
            scene->m_irradianceProbe,
            false
        };
        uploadGpuLightingData(lighting);

        // entities 
        for (auto entity : scene->entities)
        {
            drawEntity(entity);
        }

        // test ssao pass
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

#if DRAW_SSAO_DEBUG_VIS
            // render ssao debug vis
            {
                // read out data from buffer
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                SSAODebugVisData* debugVisDataPtr = reinterpret_cast<SSAODebugVisData*>(glMapNamedBuffer(m_ssaoDebugVisBuffer->m_ssbo, GL_READ_WRITE));
                memcpy(&m_ssaoDebugVisData, debugVisDataPtr, sizeof(SSAODebugVisData));
                glUnmapNamedBuffer(m_ssaoDebugVisBuffer->m_ssbo);

                // draw debug sample points
                int numDebugPoints = m_ssaoDebugVisData.numSamplePoints;
                if (!m_freezeDebugLines)
                {
                    m_ssaoSamplePoints.reset();
                    for (int i = 0; i < numDebugPoints; ++i)
                    {
                        m_ssaoSamplePoints.push(vec4ToVec3(m_ssaoDebugVisData.intermSamplePoints[i]));
                    }
                }
                m_ssaoSamplePoints.draw();

                // draw debug lines
                {
                    glm::vec3 v0, v1;
                    v0 = vec4ToVec3(m_ssaoDebugVisData.samplePointWS);
                    v1 = v0 + vec4ToVec3(m_ssaoDebugVisData.normal);
                    if (!m_freezeDebugLines)
                        m_ssaoDebugVisLines.normal.setVerts(v0, v1);
                    m_ssaoDebugVisLines.normal.draw();
                }

                {
                    glm::vec3 v0, v1;
                    v0 = vec4ToVec3(m_ssaoDebugVisData.samplePointWS);
                    v1 = v0 + vec4ToVec3(m_ssaoDebugVisData.projectedNormal);
                    if (!m_freezeDebugLines)
                        m_ssaoDebugVisLines.projectedNormal.setVerts(v0, v1);
                    m_ssaoDebugVisLines.projectedNormal.draw();
                }

                {
                    glm::vec3 v0, v1;
                    v0 = vec4ToVec3(m_ssaoDebugVisData.samplePointWS);
                    v1 = v0 + vec4ToVec3(m_ssaoDebugVisData.wo);
                    if (!m_freezeDebugLines)
                        m_ssaoDebugVisLines.wo.setVerts(v0, v1);
                    m_ssaoDebugVisLines.wo.draw();
                }
                
                {
                    glm::vec3 v0, v1;
                    v0 = vec4ToVec3(m_ssaoDebugVisData.samplePointWS);
                    v1 = v0 + vec4ToVec3(m_ssaoDebugVisData.sliceDir);
                    if (!m_freezeDebugLines)
                        m_ssaoDebugVisLines.sliceDir.setVerts(v0, v1);
                    m_ssaoDebugVisLines.sliceDir.draw();
                }

                int numDebugLines = m_ssaoDebugVisData.numSampleLines;
                for (int i = 0; i < numDebugLines; ++i)
                {
                    glm::vec3 v0, v1;
                    v0 = vec4ToVec3(m_ssaoDebugVisData.samplePointWS);
                    v1 = vec4ToVec3(m_ssaoDebugVisData.sampleVec[i]);
                    if (!m_freezeDebugLines)
                    {
                        m_ssaoDebugVisLines.samples[i].setVerts(v0, v1);
                    }
                    m_ssaoDebugVisLines.samples[i].draw();
                }
            }
#endif
        }
    }
}