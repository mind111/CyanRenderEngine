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

    // TODO: what to do in default constructor?
    Renderer::Renderer()
        : m_frameAllocator(1024u * 1024u),  // 1 megabytes
        m_lighting{0, 0, 0, false},
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
        m_voxelData{0}
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
        m_sceneColorTextureSSAA = createTextureHDR("SceneColorTexSSAA", spec);
        m_sceneColorRTSSAA = createRenderTarget(m_SSAAWidth, m_SSAAHeight);
        m_sceneColorRTSSAA->attachColorBuffer(m_sceneColorTextureSSAA);

        // scene color render targets 
        spec.m_width = m_windowWidth;
        spec.m_height = m_windowHeight;
        m_sceneColorTexture = createTextureHDR("SceneColorTexture", spec);
        m_sceneColorRenderTarget = createRenderTarget(m_windowWidth, m_windowHeight);
        m_sceneColorRenderTarget->attachColorBuffer(m_sceneColorTexture);
        m_outputColorTexture = createTextureHDR("FinalColorTexture", spec);
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
        m_voxelColorTexture = createTexture("Voxelization", voxelSpec);
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
            m_voxelVisColorTexture = createTexture("VoxelVis", visSpec);
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
        m_voxelData.m_albedo = createTexture3D("VoxelAlbedo", voxelDataSpec);
        m_voxelData.m_normal = createTexture3D("VoxelNormal", voxelDataSpec);
        m_voxelData.m_emission = createTexture3D("VoxelEmission", voxelDataSpec);
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

        // m_bloomPreprocessShader = Cyan::createShader("BloomPreprocessShader", "../../shader/shader_bloom_preprocess.vs", "../../shader/shader_bloom_preprocess.fs");
        // m_gaussianBlurShader = Cyan::createShader("GaussianBlurShader", "../../shader/shader_gaussian_blur.vs", "../../shader/shader_gaussian_blur.fs");
    }

    void Renderer::init(glm::vec2 windowSize)
    {
        Cyan::init();
        onRendererInitialized(windowSize);

        m_viewport = { static_cast<u32>(0u), 
                       static_cast<u32>(0u), 
                       static_cast<u32>(windowSize.x), 
                       static_cast<u32>(windowSize.y) };

        u_model = createUniform("s_model", Uniform::Type::u_mat4);
        u_cameraView = createUniform("s_view", Uniform::Type::u_mat4);
        u_cameraProjection = createUniform("s_projection", Uniform::Type::u_mat4);
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
            Material* matlClass = matl->m_template;
            Shader* shader = matlClass->m_shader;
            // TODO: this is obviously redundant
            ctx->setShader(matlClass->m_shader);
            if (modelMatrix)
            {
                // TODO: this is obviously redundant
                Cyan::setUniform(u_model, &(*modelMatrix)[0][0]);
                ctx->setUniform(u_model);
            }
            ctx->setUniform(u_cameraView);
            ctx->setUniform(u_cameraProjection);
            UsedBindingPoints used = matl->bind();
            Mesh::SubMesh* sm = mesh->m_subMeshes[i];
            ctx->setVertexArray(sm->m_vertexArray);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            if (ctx->m_vertexArray->m_ibo != static_cast<u32>(-1)) {
                ctx->drawIndex(ctx->m_vertexArray->m_numIndices);
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
            // TODO: verify if shader storage binding points are global
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

    // @ returns a copy is slow
    BoundingBox3f computeSceneAABB(Scene* scene)
    {
        BoundingBox3f sceneAABB;
        // compute scene AABB in view space
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
                    glm::mat4 view = glm::lookAt(scene->mainCamera.position, scene->mainCamera.lookAt, glm::vec3(0.f, 1.f, 0.f));
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
        m_voxelVisMatl->set("cameraPos", &scene->mainCamera.position.x);
        m_voxelVisMatl->set("cameraLookAt", &scene->mainCamera.lookAt.x);
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

    void Renderer::addDirectionalShadowPass()
    {

    }

    void Renderer::addPostProcessPasses()
    {
        addBloomPass();

        RenderTarget* renderTarget = m_outputRenderTarget;
        Texture* sceneColorTexture = m_bSuperSampleAA ? m_sceneColorTextureSSAA : m_sceneColorTexture;
        void* preallocatedAddr = m_frameAllocator.alloc(sizeof(PostProcessResolvePass));
        // placement new for initialization
        Viewport viewport = { 0u, 0u, renderTarget->m_width, renderTarget->m_height };
        PostProcessResolveInputs inputs = { m_exposure, 0.f, 1.0f, sceneColorTexture, BloomPass::getBloomOutput() };
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
        BloomPassInputs inputs = {sceneColorTexture, m_bloomOutput };
        // placement new for initialization
        BloomPass* pass = new (preallocatedAddr) BloomPass(nullptr, viewport, inputs);
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
            // update lighting data if necessary
            // TODO: implement these
            auto updateLighting = []()
            {

            };

            // update lighting data if material can be lit
            if (materialType->m_dataFieldsFlag && (1 << Material::DataFields::Lit))
            {
                for (u32 sm = 0; sm < numSubMeshs; ++sm)
                {
                    meshInstance->m_matls[sm]->set("numPointLights", sizeofVector(*m_lighting.m_pLights) / (u32)sizeof(PointLight));
                    meshInstance->m_matls[sm]->set("numDirLights",   sizeofVector(*m_lighting.m_dirLights) / (u32)sizeof(DirectionalLight));
                }
            }
            // update light probe data if necessary
            if (materialType->m_dataFieldsFlag && (1 << Material::DataFields::Probe))
            {
                if (m_lighting.bUpdateProbeData)
                {
                    for (u32 sm = 0; sm < numSubMeshs; ++sm)
                    {
                        // update probe texture bindings
                        meshInstance->m_matls[sm]->bindTexture("irradianceDiffuse", m_lighting.m_probe->m_diffuse);
                        meshInstance->m_matls[sm]->bindTexture("irradianceSpecular", m_lighting.m_probe->m_specular);
                        meshInstance->m_matls[sm]->bindTexture("brdfIntegral", m_lighting.m_probe->m_brdfIntegral);
                    }
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
        m_renderState.clearRenderPasses();
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
    }

    void Renderer::endRender()
    {
        // reset render target
        Cyan::getCurrentGfxCtx()->setRenderTarget(nullptr, 0u);
    }

    void Renderer::renderScene(Scene* scene)
    {
        // camera
        Camera& camera = scene->mainCamera;
        setUniform(u_cameraView, (void*)&camera.view[0]);
        setUniform(u_cameraProjection, (void*)&camera.projection[0]);

        // lights
        if (!scene->pLights.empty())
        {
            setBuffer(scene->m_pointLightsBuffer, scene->pLights.data(), sizeofVector(scene->pLights));
        }
        if (!scene->dLights.empty())
        {
            setBuffer(scene->m_dirLightsBuffer, scene->dLights.data(), sizeofVector(scene->dLights));
        }
        
        m_lighting.m_pLights = &scene->pLights;
        m_lighting.m_dirLights = &scene->dLights; 
        m_lighting.m_probe = scene->m_currentProbe;
        // determine if probe data should be update for this frame
        m_lighting.bUpdateProbeData = (!scene->m_lastProbe || (scene->m_currentProbe->m_baseCubeMap->m_id != scene->m_lastProbe->m_baseCubeMap->m_id));
        scene->m_lastProbe = scene->m_currentProbe;
        // entities 
        for (auto entity : scene->entities)
        {
            drawEntity(entity);
        }
    }
}