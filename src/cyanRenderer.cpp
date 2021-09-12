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

    void Renderer::initRenderTargets(u32 windowWidth, u32 windowHeight)
    {
        m_windowWidth = windowWidth;
        m_windowHeight = windowHeight;
        m_superSamplingRenderWidth = 2560u;
        m_superSamplingRenderHeight = 1440u;

        // super-sampling setup
        TextureSpec spec = { };
        spec.m_type = Texture::Type::TEX_2D;
        spec.m_format = Texture::ColorFormat::R16G16B16A16; 
        spec.m_width = m_superSamplingRenderWidth;
        spec.m_height = m_superSamplingRenderHeight;
        spec.m_min = Texture::Filter::LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        m_superSamplingColorBuffer = createTextureHDR("blit-SSAA-texture", spec);
        m_superSamplingRenderTarget = createRenderTarget(m_superSamplingRenderWidth, m_superSamplingRenderHeight);
        m_superSamplingRenderTarget->attachColorBuffer(m_superSamplingColorBuffer);

        // default rt for final blitting pass
        spec.m_width = m_windowWidth;
        spec.m_height = m_windowHeight;
        m_defaultColorBuffer = createTextureHDR("blit-texture", spec);
        m_defaultRenderTarget = createRenderTarget(m_windowWidth, m_windowHeight);
        m_defaultRenderTarget->attachColorBuffer(m_defaultColorBuffer);

        m_bloomPrefilterRT = createRenderTarget(m_windowWidth, m_windowHeight);
        m_bloomPrefilterRT->attachColorBuffer(createTextureHDR("bloom-prefilter-texture", spec));

        // bloom setup
        m_numBloomTextures = 0u;
        auto initBloomBuffers = [&](u32 index, TextureSpec& spec) {
            m_bloomDsSurfaces[index].m_renderTarget = createRenderTarget(spec.m_width, spec.m_height);
            char buff[64];
            sprintf_s(buff, "bloom-texture-%u", m_numBloomTextures++);
            m_bloomDsSurfaces[index].m_pingPongColorBuffers[0] = createTextureHDR(buff, spec);
            sprintf_s(buff, "bloom-texture-%u", m_numBloomTextures++);
            m_bloomDsSurfaces[index].m_pingPongColorBuffers[1] = createTextureHDR(buff, spec);
            m_bloomDsSurfaces[index].m_renderTarget->attachColorBuffer(m_bloomDsSurfaces[index].m_pingPongColorBuffers[0]);
            m_bloomDsSurfaces[index].m_renderTarget->attachColorBuffer(m_bloomDsSurfaces[index].m_pingPongColorBuffers[1]);

            m_bloomUsSurfaces[index].m_renderTarget = createRenderTarget(spec.m_width, spec.m_height);
            sprintf_s(buff, "bloom-texture-%u", m_numBloomTextures++);
            m_bloomUsSurfaces[index].m_pingPongColorBuffers[0] = createTextureHDR(buff, spec);
            sprintf_s(buff, "bloom-texture-%u", m_numBloomTextures++);
            m_bloomUsSurfaces[index].m_pingPongColorBuffers[1] = createTextureHDR(buff, spec);
            m_bloomUsSurfaces[index].m_renderTarget->attachColorBuffer(m_bloomUsSurfaces[index].m_pingPongColorBuffers[0]);
            m_bloomUsSurfaces[index].m_renderTarget->attachColorBuffer(m_bloomUsSurfaces[index].m_pingPongColorBuffers[1]);
            spec.m_width /= 2;
            spec.m_height /= 2;
        };

        spec.m_width /= 2;
        spec.m_height /= 2;
        for (u32 i = 0u; i < kNumBloomDsPass; ++i) {
            initBloomBuffers(i, spec);
        }
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

        m_bloomPreprocessShader = Cyan::createShader("BloomPreprocessShader", "../../shader/shader_bloom_preprocess.vs", "../../shader/shader_bloom_preprocess.fs");
        m_gaussianBlurShader = Cyan::createShader("GaussianBlurShader", "../../shader/shader_gaussian_blur.vs", "../../shader/shader_gaussian_blur.fs");
    }

    void Renderer::init(glm::vec2 viewportSize)
    {
        Cyan::init();
        m_viewport = { static_cast<u32>(0u), 
                       static_cast<u32>(0u), 
                       static_cast<u32>(viewportSize.x), 
                       static_cast<u32>(viewportSize.y) };

        u_model = createUniform("s_model", Uniform::Type::u_mat4);
        u_cameraView = createUniform("s_view", Uniform::Type::u_mat4);
        u_cameraProjection = createUniform("s_projection", Uniform::Type::u_mat4);
        // misc
        m_bSuperSampleAA = true;
        m_exposure = 1.f;
        m_bloom = true;
        m_frame = new Frame();

        // set back-face culling
        Cyan::getCurrentGfxCtx()->setCullFace(FrontFace::CounterClockWise, FaceCull::Back);

        // render targets
        initShaders();
        initRenderTargets(m_viewport.m_width, m_viewport.m_height);

        // blit mesh & material
        m_blitShader = createShader("BlitShader", "../../shader/shader_blit.vs", "../../shader/shader_blit.fs");
        m_blitMaterial = createMaterial(m_blitShader)->createInstance();
        m_blitMaterial->bindTexture("quadSampler", m_defaultColorBuffer);
        m_bloomPreprocessMatl = createMaterial(m_bloomPreprocessShader)->createInstance();
        m_gaussianBlurMatl = createMaterial(m_gaussianBlurShader)->createInstance();

        m_blitQuad = (BlitQuadMesh*)CYAN_ALLOC(sizeof(Renderer::BlitQuadMesh));
        m_blitQuad->m_vb = createVertexBuffer((void*)quadVerts, sizeof(quadVerts), 4 * sizeof(f32), 6u);
        m_blitQuad->m_va = createVertexArray(m_blitQuad->m_vb);
        u32 strideInBytes = 4 * sizeof(f32);
        u32 offset = 0;
        m_blitQuad->m_vb->m_vertexAttribs.push_back({
                VertexAttrib::DataType::Float, 2u, strideInBytes, offset, (f32*)m_blitQuad->m_vb->m_data + offset
        });
        offset += 2 * sizeof(f32);
        m_blitQuad->m_vb->m_vertexAttribs.push_back({
                VertexAttrib::DataType::Float, 2u, strideInBytes, offset, (f32*)m_blitQuad->m_vb->m_data + offset
        });
        m_blitQuad->m_va->init();
        m_blitQuad->m_matl = m_blitMaterial;
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

    // TODO:
    void Renderer::submitMesh(Mesh* mesh, glm::mat4 modelTransform)
    {
        for (u32 index = 0; index < mesh->m_subMeshes.size(); ++index)
        {
            DrawCall draw = {};
            // draw.m_uniformBegin = m_frame->m_uniformBuffer.m_pos;
            // MaterialInstance* material = mesh->m_subMeshes[index]->m_matl;
            // for ( : material)
            // {
            //     Cyan::setUniform();
            // }
            // draw.m_uniformEnd = m_frame->m_uniformBuffer.m_pos;

            draw.m_mesh = mesh;
            draw.m_index = index;
            draw.m_modelTransform = modelTransform;
            m_frame->m_renderQueue.push_back(draw);
        }
    }

    void Renderer::renderFrame()
    {
    #if 0
        auto ctx = Cyan::getCurrentGfxCtx();
        for (auto draw : m_frame->m_renderQueue)
        {
            auto sm = draw.m_mesh->m_subMeshes[draw.m_index];
            MaterialInstance* material = sm->m_matl;
            Material* matlClass = material->m_template;
            auto shader = matlClass->m_shader;

            ctx->setShader(shader);
            // TODO: This is dumb
            Cyan::setUniform(u_model, &draw.m_modelTransform[0][0]);
            ctx->setUniform(u_model);
            // for (auto uniform : shader->m_uniforms)
            // {
            //     ctx->setUniform(uniform);
            // }
            for (auto buffer : shader->m_buffers)
            {
                ctx->setBuffer(buffer);
            }

            u32 usedTextureUnit = material->bind();

            ctx->setVertexArray(sm->m_vertexArray);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            ctx->drawIndex(sm->m_numVerts);
            // NOTES: reset texture units because texture unit bindings are managed by gl context 
            // it won't change when binding different shaders
            for (int t = 0; t < usedTextureUnit; ++t)
            {
                ctx->setTexture(nullptr, t);
            }
        }
    #endif
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
            // modelMatrix = modelMatrix * node->m_meshInstance->m_mesh->m_normalization;
            modelMatrix = modelMatrix;
            drawMeshInstance(node->m_meshInstance, &modelMatrix);
        }
        for (auto* child : node->m_child)
        {
            SceneNode* childNode = dynamic_cast<SceneNode*>(child);
            drawSceneNode(childNode);
        }
    }

    void Renderer::endFrame()
    {
        m_frame->m_renderQueue.clear();
        Cyan::getCurrentGfxCtx()->flip();
    }

    void Renderer::beginRender()
    {
        // set render target to m_defaultRenderTarget
        auto ctx = Cyan::getCurrentGfxCtx();
        if (m_bSuperSampleAA)
        {
            ctx->setRenderTarget(m_superSamplingRenderTarget, 0u);
            m_offscreenRenderWidth = m_superSamplingRenderWidth;
            m_offscreenRenderHeight = m_superSamplingRenderHeight;
            ctx->setViewport({ 0u, 0u, 2560u, 1440u });
        }
        else 
        {
            ctx->setRenderTarget(m_defaultRenderTarget, 0u);
            m_offscreenRenderWidth = m_windowWidth;
            m_offscreenRenderHeight = m_windowHeight;
            ctx->setViewport(m_viewport);
        }

        // clear
        ctx->clear();
    }

    void Renderer::beginBloom()
    {
        auto ctx = Cyan::getCurrentGfxCtx();
        // bloom preprocess pass (blit the render results onto a smaller texture)
        RenderTarget* renderTarget = m_bloomPrefilterRT;
        Cyan::Texture* srcImage = nullptr;
        if (m_bSuperSampleAA) {
            srcImage = m_superSamplingColorBuffer;
        } else {
            srcImage = m_defaultColorBuffer;
        }
        ctx->setDepthControl(Cyan::DepthControl::kDisable);
        ctx->setRenderTarget(renderTarget, 0u);
        ctx->setShader(m_bloomPreprocessShader);
        ctx->setViewport({ 0u, 0u, renderTarget->m_width, renderTarget->m_height });
        m_bloomPreprocessMatl->bindTexture("quadSampler", srcImage);
        m_bloomPreprocessMatl->bind();
        ctx->setVertexArray(m_blitQuad->m_va);
        ctx->setPrimitiveType(PrimitiveType::TriangleList);
        ctx->drawIndexAuto(6u);
        ctx->setDepthControl(Cyan::DepthControl::kEnable);
        glFinish();
    }

    void Renderer::downSample(RenderTarget* src, u32 srcIdx, RenderTarget* dst, u32 dstIdx) {
        Shader* downSampleShader = Cyan::createShader("DownSampleShader", "../../shader/shader_downsample.vs", "../../shader/shader_downsample.fs");
        Cyan::MaterialInstance* matl = Cyan::createMaterial(downSampleShader)->createInstance();
        auto ctx = Cyan::getCurrentGfxCtx();
        ctx->setDepthControl(Cyan::DepthControl::kDisable);
        ctx->setRenderTarget(dst, dstIdx);
        ctx->setShader(downSampleShader);
        ctx->setViewport({ 0u, 0u, dst->m_width, dst->m_height });
        matl->bindTexture("srcImage", src->m_colorBuffers[srcIdx]);
        matl->bind();
        ctx->setVertexArray(m_blitQuad->m_va);
        ctx->setPrimitiveType(PrimitiveType::TriangleList);
        ctx->drawIndexAuto(6u);
        ctx->setDepthControl(Cyan::DepthControl::kEnable);
        glFinish();
    }

    void Renderer::gaussianBlur(BloomSurface surface, GaussianBlurInputs inputs) {
        auto ctx = Cyan::getCurrentGfxCtx();
        ctx->setDepthControl(Cyan::DepthControl::kDisable);
        ctx->setShader(m_gaussianBlurShader);
        ctx->setRenderTarget(surface.m_renderTarget, 1u);
        ctx->setViewport({ 0u, 0u, surface.m_renderTarget->m_width, surface.m_renderTarget->m_height });

        // horizontal pass
        {
            m_gaussianBlurMatl->bindTexture("srcImage", surface.m_pingPongColorBuffers[0]);
            m_gaussianBlurMatl->set("horizontal", 1.0f);
            m_gaussianBlurMatl->set("kernelIndex", inputs.kernelIndex);
            m_gaussianBlurMatl->set("radius", inputs.radius);
            m_gaussianBlurMatl->bind();
            ctx->setVertexArray(m_blitQuad->m_va);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            ctx->drawIndexAuto(6u);
            glFinish();
        }

        // vertical pass
        {
            ctx->setRenderTarget(surface.m_renderTarget, 0u);
            m_gaussianBlurMatl->bindTexture("srcImage", surface.m_pingPongColorBuffers[1]);
            m_gaussianBlurMatl->set("horizontal", 0.f);
            m_gaussianBlurMatl->set("kernelIndex", inputs.kernelIndex);
            m_gaussianBlurMatl->set("radius", inputs.radius);
            m_gaussianBlurMatl->bind();
            ctx->setVertexArray(m_blitQuad->m_va);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            ctx->drawIndexAuto(6u);
            ctx->setDepthControl(Cyan::DepthControl::kEnable);
            glFinish();
        }
    }

    void Renderer::upSample(RenderTarget* src, u32 srcIdx, RenderTarget* dst, u32 dstIdx, RenderTarget* blend, u32 blendIdx, UpScaleInputs inputs) 
    {
        Shader* upSampleShader = Cyan::createShader("UpSampleShader", "../../shader/shader_upsample.vs", "../../shader/shader_upsample.fs");
        Cyan::MaterialInstance* matl = Cyan::createMaterial(upSampleShader)->createInstance();
        auto ctx = Cyan::getCurrentGfxCtx();
        ctx->setDepthControl(Cyan::DepthControl::kDisable);
        ctx->setRenderTarget(dst, dstIdx);
        ctx->setShader(upSampleShader);
        ctx->setViewport({ 0u, 0u, dst->m_width, dst->m_height });
        matl->bindTexture("srcImage", src->m_colorBuffers[srcIdx]);
        matl->bindTexture("blendImage", blend->m_colorBuffers[blendIdx]);
        matl->set("stageIndex", inputs.stageIndex);
        matl->bind();
        ctx->setVertexArray(m_blitQuad->m_va);
        ctx->setPrimitiveType(PrimitiveType::TriangleList);
        ctx->drawIndexAuto(6u);
        ctx->setDepthControl(Cyan::DepthControl::kEnable);
        glFinish();
    }

    void Renderer::bloomDownSample()
    {
        //TODO: more flexible raidus?
        i32 kernelRadii[6] = { 3, 4, 6, 7, 8, 9};
        GaussianBlurInputs inputs = { };

        inputs.kernelIndex = 0;
        inputs.radius = kernelRadii[0];
        downSample(m_bloomPrefilterRT, 0, m_bloomDsSurfaces[0].m_renderTarget, 0);
        gaussianBlur(m_bloomDsSurfaces[0], inputs);
        for (u32 i = 0u; i < kNumBloomDsPass - 1; ++i) 
        {
            downSample(m_bloomDsSurfaces[i].m_renderTarget, 0, m_bloomDsSurfaces[i+1].m_renderTarget, 0);
            inputs.kernelIndex = static_cast<i32>(i+1);
            inputs.radius = kernelRadii[i+1];
            gaussianBlur(m_bloomDsSurfaces[i+1], inputs);
        }
    }

    void Renderer::bloomUpScale()
    {
        i32 kernelRadii[6] = { 3, 4, 6, 7, 8, 9};
        GaussianBlurInputs inputs = { };

        inputs.kernelIndex = 5;
        inputs.radius = kernelRadii[5];
        gaussianBlur(m_bloomDsSurfaces[kNumBloomDsPass-2], inputs);

        UpScaleInputs upScaleInputs = { };
        upScaleInputs.stageIndex = 0;

        upSample(m_bloomDsSurfaces[kNumBloomDsPass - 1].m_renderTarget, 0, 
            m_bloomUsSurfaces[kNumBloomDsPass-2].m_renderTarget, 0, 
            m_bloomDsSurfaces[kNumBloomDsPass-2].m_renderTarget, 0,
            upScaleInputs);

        for (u32 i = kNumBloomDsPass - 2; i > 0 ; --i) {
            inputs.kernelIndex = static_cast<i32>(i);
            inputs.radius = kernelRadii[i];
            gaussianBlur(m_bloomDsSurfaces[i-1], inputs);

            upScaleInputs.stageIndex = kNumBloomDsPass - 1 - i;
            upSample(m_bloomUsSurfaces[i].m_renderTarget, 0, m_bloomUsSurfaces[i-1].m_renderTarget, 0, m_bloomDsSurfaces[i-1].m_renderTarget, 0, upScaleInputs);
        }
    }

    void Renderer::bloom() {
        /* 
            TODO:
            * Add a knee function to attenuate luminance
            * Implement more advanced filters (right now both passes are using a simple box filter) when downsampling/upsampling, referring to a talk
              at siggraph 2014 "Next gen post processing pipeline in Cod: Advanced Warfare" 
            * Soft threshold
            * How to combat temporal stability?
        */

        // preprocess pass
        beginBloom();
        // downsampling
        bloomDownSample();
        // upscale and gather
        bloomUpScale();
    }

    void Renderer::blitPass()
    {
        // post-processing pass
        m_blitMaterial->set("exposure", m_exposure);
        auto ctx = Cyan::getCurrentGfxCtx();
        // bloom
        if (m_bloom)
        {
            if (m_bSuperSampleAA)
            {
                bloom();
            }
        }
        // final blit to default frame buffer
        ctx->setDepthControl(Cyan::DepthControl::kDisable);
        ctx->setRenderTarget(m_defaultRenderTarget, 0u);
        ctx->setViewport({0u, 0u, m_viewport.m_width, m_viewport.m_height});
        ctx->setShader(m_blitShader);
        if (m_bloom)
        {
            m_blitQuad->m_matl->bindTexture("bloomSampler_0", m_bloomUsSurfaces[0].m_pingPongColorBuffers[0]);
            m_blitQuad->m_matl->set("bloom", 1.f);
        }
        else
        {
            m_blitQuad->m_matl->set("bloom", 0.f);
        }       m_blitQuad->m_matl = m_blitMaterial;

        if (m_bSuperSampleAA)
        {
            m_blitQuad->m_matl->bindTexture("quadSampler", m_superSamplingColorBuffer);
        }
        else
        {
            m_blitQuad->m_matl->bindTexture("quadSampler", m_defaultColorBuffer);
        }
        m_blitQuad->m_matl->bind();
        ctx->setVertexArray(m_blitQuad->m_va);
        ctx->setPrimitiveType(PrimitiveType::TriangleList);
        ctx->drawIndexAuto(6u);
        ctx->setDepthControl(Cyan::DepthControl::kEnable);
    }
    
    void Renderer::endRender()
    {
        blitPass();
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