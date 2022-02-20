#include "RenderPass.h"
#include "GraphicsSystem.h"

namespace Cyan
{
    //- static initialization of class static data members

    static QuadMesh s_quadMesh;
    // scene
    // Shader* ScenePass::s_sceneNormalDepthShader = nullptr;
    Texture* ScenePass::ssaoBlurVertTex = nullptr;
    Texture* ScenePass::ssaoBlurHoriTex = nullptr;
    RenderTarget* ScenePass::ssaoBlurRt = nullptr;
    // post process resolve
    Shader* PostProcessResolvePass::s_finalCompositeShader = 0;
    MaterialInstance* PostProcessResolvePass::s_matl = 0;
    // bloom
    RenderTarget* BloomPass::s_bloomSetupRT = 0;
    Shader* BloomPass::s_bloomSetupShader = 0;
    MaterialInstance* BloomPass::s_bloomSetupMatl = 0;
    Shader* BloomPass::s_bloomDsShader = 0;
    MaterialInstance* BloomPass::s_bloomDsMatl = 0;
    Shader* BloomPass::s_bloomUsShader = 0;
    MaterialInstance* BloomPass::s_bloomUsMatl = 0;
    Shader* BloomPass::s_gaussianBlurShader = 0;
    MaterialInstance* BloomPass::s_gaussianBlurMatl = 0;
    BloomPass::BloomSurface BloomPass::s_bloomDsSurfaces[BloomPass::kNumBloomDsPass] = {0};          // downsample
    BloomPass::BloomSurface BloomPass::s_bloomUsSurfaces[BloomPass::kNumBloomDsPass] = {0};          // upsample
    // textured quad
    Shader* TexturedQuadPass::m_shader = 0;
    MaterialInstance* TexturedQuadPass::m_matl = 0;
    // directional shadow
    RenderTarget* DirectionalShadowPass::s_depthRenderTarget = 0;
    Shader* DirectionalShadowPass::s_directShadowShader = 0;
    MaterialInstance* DirectionalShadowPass::s_directShadowMatl = 0;
    CascadedShadowMap DirectionalShadowPass::m_cascadedShadowMap = { };
    Texture* DirectionalShadowPass::m_horizontalBlurTex = nullptr;
    Texture* DirectionalShadowPass::m_verticalBlurTex = nullptr;
    // gaussian blur
    Shader* GaussianBlurPass::m_gaussianBlurShader = nullptr;
    MaterialInstance* GaussianBlurPass::m_gaussianBlurMatl = nullptr;

    QuadMesh* getQuadMesh()
    {
        return &s_quadMesh;
    }

    void onRendererInitialized(glm::vec2 windowSize)
    {
        u32 windowWidth = static_cast<u32>(windowSize.x);
        u32 windowHeight = static_cast<u32>(windowSize.y);

        s_quadMesh.init(glm::vec2(0.f, 0.f), glm::vec2(1.0f, 1.0f));

        ScenePass::onInit();
        BloomPass::onInit(windowWidth, windowHeight);
        PostProcessResolvePass::onInit();
        TexturedQuadPass::onInit();
        DirectionalShadowPass::onInit();
        GaussianBlurPass::onInit();
    }

    ScenePass::ScenePass(RenderTarget* renderTarget, Viewport viewport, Scene* scene)
        : RenderPass(renderTarget, viewport), m_scene(scene)
    {

    }

    ScenePass::~ScenePass()
    {
        m_renderTarget = nullptr;
        m_scene = nullptr;
    }

    void ScenePass::onInit()
    {
        // s_sceneNormalDepthShader = createShader("SceneNormalDepthShader", "../../shader/scene_normal_depth.vs", "../../shader/scene_normal_depth.fs");
        TextureSpec spec = { };
        Texture* ssaoTex = Renderer::getSingletonPtr()->m_ssaoTexture;
        spec.m_type = Texture::Type::TEX_2D;
        spec.m_width = 1280;
        spec.m_height = 720;
        spec.m_dataType = Texture::DataType::Float;
        spec.m_format = Texture::ColorFormat::R16G16B16;
        spec.m_min = Texture::Filter::LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_numMips = 1;
        ssaoBlurHoriTex = TextureManager::getSingletonPtr()->createTexture("SSAOVertBlur", spec);
        ssaoBlurRt = createRenderTarget(spec.m_width, spec.m_height);
    }

    void ScenePass::render()
    {
        auto renderer = Renderer::getSingletonPtr();
        auto ctx = getCurrentGfxCtx();

        // todo: moving to deferred 
        // scene albedo & depth & normal pass
        {
            i32 drawBuffers[2] = { 1, 2 }; 
            ctx->setRenderTarget(m_renderTarget, drawBuffers, 2);
            // clear buffers to proper values
            glm::vec4 normalBufferClear(0.f, 0.f, 0.f, 1.f);
            glm::vec4 depthBufferClear(1.f);
            glClearBufferfv(GL_COLOR, 0, &normalBufferClear.x);
            glClearBufferfv(GL_COLOR, 1, &depthBufferClear.x);
            ctx->setViewport(m_viewport);
            renderer->renderSceneDepthNormal(m_scene, m_scene->getActiveCamera());
        }

        // ssao pass
        // todo: move this to a proper place
        {
            renderer->renderSSAO(m_scene->getActiveCamera());
            // gaussian blur
            for (u32 i = 0; i < 1; ++i)
            {
                void* mem = renderer->m_frameAllocator.alloc(sizeof(GaussianBlurPass));
                GaussianBlurInput input = { 0, 3 };
                GaussianBlurPass* ssaoBlurPass = new (mem) GaussianBlurPass(
                    ssaoBlurRt,
                    { 0, 0, renderer->m_ssaoTexture->m_width, renderer->m_ssaoTexture->m_height },
                    renderer->m_ssaoTexture,
                    ssaoBlurHoriTex,
                    renderer->m_ssaoTexture,
                    input
                );
                ssaoBlurPass->render();
            }
        }

        // full lighting pass
        {
            i32 drawBuffers[5] = {0, -1, -1, -1, -1 };
            ctx->setRenderTarget(m_renderTarget, drawBuffers, 5);
            ctx->setViewport(m_viewport);
            renderer->renderScene(m_scene, m_scene->getActiveCamera());
        }
    }

    GaussianBlurPass::GaussianBlurPass(RenderTarget* renderTarget, Viewport viewport, Texture* srcTex, Texture* horizontal, Texture* vertical, GaussianBlurInput params)
        : RenderPass(renderTarget, viewport), 
        m_srcTexture(srcTex), 
        m_horiztontalTex(horizontal), 
        m_verticalTex(vertical),
        m_params(params)
    {

    }

    void GaussianBlurPass::onInit()
    {
        if (!m_gaussianBlurShader)
        {
            m_gaussianBlurShader = createShader("GaussianBlurShader", "../../shader/shader_gaussian_blur.vs", "../../shader/shader_gaussian_blur.fs");
            m_gaussianBlurMatl = createMaterial(m_gaussianBlurShader)->createInstance();
        }
    }

    void GaussianBlurPass::render()
    {
        auto ctx = getCurrentGfxCtx();
        m_renderTarget->attachTexture(m_verticalTex, 0);
        m_renderTarget->attachTexture(m_horiztontalTex, 1);
        ctx->setDepthControl(DepthControl::kDisable);
        ctx->setVertexArray(s_quadMesh.m_vertexArray);
        m_gaussianBlurMatl->set("kernelIndex", m_params.kernelIndex);
        m_gaussianBlurMatl->set("radius", m_params.radius);

        // horizontal pass
        {
            ctx->setRenderTarget(m_renderTarget, 1);
            ctx->setShader(m_gaussianBlurShader);
            m_gaussianBlurMatl->bindTexture("srcImage", m_srcTexture);
            m_gaussianBlurMatl->set("horizontal", 1.0f);
            m_gaussianBlurMatl->bind();
            ctx->drawIndexAuto(s_quadMesh.m_vertexArray->numVerts());
        }
        // vertical pass
        {
            ctx->setRenderTarget(m_renderTarget, 0);
            ctx->setShader(m_gaussianBlurShader);
            ctx->setVertexArray(s_quadMesh.m_vertexArray);
            m_gaussianBlurMatl->bindTexture("srcImage", m_horiztontalTex);
            m_gaussianBlurMatl->set("horizontal", 0.0f);
            m_gaussianBlurMatl->bind();
            ctx->drawIndexAuto(s_quadMesh.m_vertexArray->numVerts());
        }
        ctx->setDepthControl(DepthControl::kEnable);
    }

    Texture* GaussianBlurPass::getBlurOutput()
    {
        // final blur output is written to the texture for vertical pass
        return m_verticalTex;
    }

    BloomPass::BloomPass(RenderTarget* rt, Viewport vp, BloomPassInput inputs) 
        : RenderPass(rt, vp), m_inputs(inputs)
    {

    }

    void BloomPass::onInit(u32 windowWidth, u32 windowHeight)
    {
        auto textureManger = TextureManager::getSingletonPtr();
        s_bloomSetupRT = createRenderTarget(windowWidth, windowHeight);
        TextureSpec spec = { };
        spec.m_width = windowWidth;
        spec.m_height = windowHeight;
        spec.m_type = Texture::Type::TEX_2D;
        spec.m_format = Texture::ColorFormat::R16G16B16A16; 
        spec.m_dataType = Texture::DataType::Float;
        spec.m_min = Texture::Filter::LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        s_bloomSetupRT->attachColorBuffer(textureManger->createTexture("BloomSetupTexture", spec));
        s_bloomSetupShader = createShader("BloomSetupShader", "../../shader/shader_bloom_preprocess.vs", "../../shader/shader_bloom_preprocess.fs");
        s_bloomSetupMatl = createMaterial(BloomPass::s_bloomSetupShader)->createInstance();
        s_bloomDsShader = createShader("BloomDownSampleShader", "../../shader/shader_downsample.vs", "../../shader/shader_downsample.fs");
        s_bloomDsMatl = createMaterial(s_bloomDsShader)->createInstance();
        s_bloomUsShader = createShader("UpSampleShader", "../../shader/shader_upsample.vs", "../../shader/shader_upsample.fs");
        s_bloomUsMatl = createMaterial(s_bloomUsShader)->createInstance();
        s_gaussianBlurShader = createShader("GaussianBlurShader", "../../shader/shader_gaussian_blur.vs", "../../shader/shader_gaussian_blur.fs");
        s_gaussianBlurMatl = createMaterial(s_gaussianBlurShader)->createInstance();

        u32 numBloomTextures = 0u;
        auto initBloomBuffers = [&](u32 index, TextureSpec& spec) {
            BloomPass::s_bloomDsSurfaces[index].m_renderTarget = createRenderTarget(spec.m_width, spec.m_height);
            char buff[64];
            sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
            BloomPass::s_bloomDsSurfaces[index].m_pingPongColorBuffers[0] = textureManger->createTextureHDR(buff, spec);
            sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
            BloomPass::s_bloomDsSurfaces[index].m_pingPongColorBuffers[1] = textureManger->createTextureHDR(buff, spec);
            BloomPass::s_bloomDsSurfaces[index].m_renderTarget->attachColorBuffer(BloomPass::s_bloomDsSurfaces[index].m_pingPongColorBuffers[0]);
            BloomPass::s_bloomDsSurfaces[index].m_renderTarget->attachColorBuffer(BloomPass::s_bloomDsSurfaces[index].m_pingPongColorBuffers[1]);

            BloomPass::s_bloomUsSurfaces[index].m_renderTarget = createRenderTarget(spec.m_width, spec.m_height);
            sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
            BloomPass::s_bloomUsSurfaces[index].m_pingPongColorBuffers[0] = textureManger->createTextureHDR(buff, spec);
            sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
            BloomPass::s_bloomUsSurfaces[index].m_pingPongColorBuffers[1] = textureManger->createTextureHDR(buff, spec);
            BloomPass::s_bloomUsSurfaces[index].m_renderTarget->attachColorBuffer(BloomPass::s_bloomUsSurfaces[index].m_pingPongColorBuffers[0]);
            BloomPass::s_bloomUsSurfaces[index].m_renderTarget->attachColorBuffer(BloomPass::s_bloomUsSurfaces[index].m_pingPongColorBuffers[1]);
            spec.m_width /= 2;
            spec.m_height /= 2;
        };

        for (u32 i = 0u; i < BloomPass::kNumBloomDsPass; ++i) {
            initBloomBuffers(i, spec);
        }
    }

    Texture* BloomPass::getBloomOutput()
    {
        return BloomPass::s_bloomUsSurfaces[0].m_pingPongColorBuffers[0];
    }

    void BloomPass::bloomDownSample()
    {
        //TODO: more flexible raidus?
        i32 kernelRadii[6] = { 3, 4, 6, 7, 8, 9};
        GaussianBlurInput inputs = { };

        inputs.kernelIndex = 0;
        inputs.radius = kernelRadii[0];
        auto renderer = Renderer::getSingletonPtr();
        downSample(s_bloomSetupRT, 0, s_bloomDsSurfaces[0].m_renderTarget, 0);
        gaussianBlur(s_bloomDsSurfaces[0], inputs);
        for (u32 i = 0u; i < kNumBloomDsPass - 1; ++i) 
        {
            downSample(s_bloomDsSurfaces[i].m_renderTarget, 0, s_bloomDsSurfaces[i+1].m_renderTarget, 0);
            inputs.kernelIndex = static_cast<i32>(i+1);
            inputs.radius = kernelRadii[i+1];
            gaussianBlur(s_bloomDsSurfaces[i+1], inputs);
        }
        // copy result to output texture
    }

    void BloomPass::gaussianBlur(BloomSurface src, GaussianBlurInput inputs)
    {
        auto ctx = Cyan::getCurrentGfxCtx();
        ctx->setDepthControl(Cyan::DepthControl::kDisable);
        ctx->setShader(s_gaussianBlurShader);
        ctx->setRenderTarget(src.m_renderTarget, 1u);
        ctx->setViewport({ 0u, 0u, src.m_renderTarget->m_width, src.m_renderTarget->m_height });

        // horizontal pass
        {
            s_gaussianBlurMatl->bindTexture("srcImage", src.m_pingPongColorBuffers[0]);
            s_gaussianBlurMatl->set("horizontal", 1.0f);
            s_gaussianBlurMatl->set("kernelIndex", inputs.kernelIndex);
            s_gaussianBlurMatl->set("radius", inputs.radius);
            s_gaussianBlurMatl->bind();
            ctx->setVertexArray(s_quadMesh.m_vertexArray);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            ctx->drawIndexAuto(s_quadMesh.m_vertexArray->numVerts());
            glFinish();
        }

        // vertical pass
        {
            ctx->setRenderTarget(src.m_renderTarget, 0u);
            s_gaussianBlurMatl->bindTexture("srcImage", src.m_pingPongColorBuffers[1]);
            s_gaussianBlurMatl->set("horizontal", 0.f);
            s_gaussianBlurMatl->set("kernelIndex", inputs.kernelIndex);
            s_gaussianBlurMatl->set("radius", inputs.radius);
            s_gaussianBlurMatl->bind();
            ctx->setVertexArray(s_quadMesh.m_vertexArray);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            ctx->drawIndexAuto(s_quadMesh.m_vertexArray->numVerts());
            ctx->setDepthControl(Cyan::DepthControl::kEnable);
            glFinish();
        }
    }

    void BloomPass::downSample(RenderTarget* src, u32 srcIdx, RenderTarget* dst, u32 dstIdx) {
        auto ctx = Cyan::getCurrentGfxCtx();
        ctx->setDepthControl(Cyan::DepthControl::kDisable);
        ctx->setRenderTarget(dst, dstIdx);
        ctx->setShader(s_bloomDsShader);
        ctx->setViewport({ 0u, 0u, dst->m_width, dst->m_height });
        s_bloomDsMatl->bindTexture("srcImage", src->m_colorBuffers[srcIdx]);
        s_bloomDsMatl->bind();
        ctx->setVertexArray(s_quadMesh.m_vertexArray);
        ctx->setPrimitiveType(PrimitiveType::TriangleList);
        ctx->drawIndexAuto(s_quadMesh.m_vertexArray->numVerts());
        ctx->setDepthControl(Cyan::DepthControl::kEnable);
        glFinish();
    }

    void BloomPass::upScale(RenderTarget* src, u32 srcIdx, RenderTarget* dst, u32 dstIdx, RenderTarget* blend, u32 blendIdx, u32 stageIdx) 
    {
        auto ctx = Cyan::getCurrentGfxCtx();
        ctx->setDepthControl(Cyan::DepthControl::kDisable);
        ctx->setRenderTarget(dst, dstIdx);
        ctx->setShader(s_bloomUsShader);
        ctx->setViewport({ 0u, 0u, dst->m_width, dst->m_height });
        s_bloomUsMatl->bindTexture("srcImage", src->m_colorBuffers[srcIdx]);
        s_bloomUsMatl->bindTexture("blendImage", blend->m_colorBuffers[blendIdx]);
        s_bloomUsMatl->set("stageIndex", stageIdx);
        s_bloomUsMatl->bind();
        ctx->setVertexArray(s_quadMesh.m_vertexArray);
        ctx->setPrimitiveType(PrimitiveType::TriangleList);
        ctx->drawIndexAuto(s_quadMesh.m_vertexArray->numVerts());
        ctx->setDepthControl(Cyan::DepthControl::kEnable);
        glFinish();
    }
    
    void BloomPass::bloomUpscale()
    {
        i32 kernelRadii[6] = { 3, 4, 6, 7, 8, 9};
        GaussianBlurInput inputs = { };
        auto renderer = Renderer::getSingletonPtr();
        inputs.kernelIndex = 5;
        inputs.radius = kernelRadii[5];
        gaussianBlur(s_bloomDsSurfaces[kNumBloomDsPass-2], inputs);

        u32 stageIndex = 0;
        upScale(s_bloomDsSurfaces[kNumBloomDsPass - 1].m_renderTarget, 0, 
                s_bloomUsSurfaces[kNumBloomDsPass-2].m_renderTarget, 0, 
                s_bloomDsSurfaces[kNumBloomDsPass-2].m_renderTarget, 0,
                stageIndex);

        for (u32 i = kNumBloomDsPass - 2; i > 0 ; --i) {
            inputs.kernelIndex = static_cast<i32>(i);
            inputs.radius = kernelRadii[i];
            gaussianBlur(s_bloomDsSurfaces[i-1], inputs);

            stageIndex = kNumBloomDsPass - 1 - i;
            upScale(s_bloomUsSurfaces[i].m_renderTarget, 0, s_bloomUsSurfaces[i-1].m_renderTarget, 0, s_bloomDsSurfaces[i-1].m_renderTarget, 0, stageIndex);
        }
    }

    void BloomPass::render()
    {
        auto ctx = Cyan::getCurrentGfxCtx();
        // bloom setup pass
        {
            ctx->setRenderTarget(s_bloomSetupRT, 0u);
            ctx->setShader(s_bloomSetupShader);
            ctx->setViewport({ 0u, 0u, s_bloomSetupRT->m_width, s_bloomSetupRT->m_height });
            s_bloomSetupMatl->bindTexture("quadSampler", m_inputs.sceneTexture);
            s_bloomSetupMatl->bind();
            ctx->setVertexArray(s_quadMesh.m_vertexArray);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            ctx->setDepthControl(Cyan::DepthControl::kDisable);
            ctx->drawIndexAuto(6u);
            ctx->setDepthControl(Cyan::DepthControl::kEnable);
            glFinish();
        }
        // downsample
        {
            bloomDownSample();
        }
        // upsample and composite
        {
            bloomUpscale();
        }
    }

    PostProcessResolvePass::PostProcessResolvePass(RenderTarget* rt, Viewport vp, PostProcessResolveInput inputs)
        : RenderPass(rt, vp), m_inputs(inputs)
    {

    }

    void PostProcessResolvePass::onInit()
    {
        s_finalCompositeShader = createShader("BlitShader", "../../shader/shader_blit.vs", "../../shader/shader_blit.fs");
        s_matl = createMaterial(PostProcessResolvePass::s_finalCompositeShader)->createInstance();
    }

    void PostProcessResolvePass::render()
    {
        // post-processing pass
        s_matl->set("exposure", m_inputs.exposure);
        auto ctx = Cyan::getCurrentGfxCtx();
        // final resolve & blit to default frame buffer
        ctx->setRenderTarget(m_renderTarget, 0u);
        ctx->setViewport({0u, 0u, m_viewport.m_width, m_viewport.m_height});
        ctx->setShader(s_finalCompositeShader);
        if (m_inputs.bloomTexture)
        {
            s_matl->bindTexture("bloomSampler_0", m_inputs.bloomTexture);
        }
        s_matl->set("bloom", m_inputs.bloom);
        s_matl->set("bloomInstensity", m_inputs.bloomIntensity);
        s_matl->bindTexture("quadSampler", m_inputs.sceneTexture);
        s_matl->bind();
        ctx->setVertexArray(s_quadMesh.m_vertexArray);
        ctx->setPrimitiveType(PrimitiveType::TriangleList);
        ctx->setDepthControl(DepthControl::kDisable);
        ctx->drawIndexAuto(6u);
        ctx->setDepthControl(DepthControl::kEnable);
    }
    
    DirectionalShadowPass::DirectionalShadowPass(RenderTarget* renderTarget, Viewport viewport, const Camera& camera, const DirectionalLight& light, const std::vector<Entity*>& shadowCasters)
        : RenderPass(renderTarget, viewport), m_light(light), m_camera(camera), m_shadowCasters(shadowCasters)
    {

    }

    void DirectionalShadowPass::onInit()
    {
        m_cascadedShadowMap.m_technique = ShadowTechnique::kPCF_Shadow;

        auto textureManager = TextureManager::getSingletonPtr();
        // TODO: depth buffer creation coupled with render target creation
        s_depthRenderTarget = createDepthRenderTarget(4096, 4096);
        s_directShadowShader = createShader("DirShadowShader", "../../shader/shader_dir_shadow.vs", "../../shader/shader_dir_shadow.fs");
        s_directShadowMatl = createMaterial(s_directShadowShader)->createInstance();

        TextureSpec depthSpec = { };
        depthSpec.m_width = s_depthRenderTarget->m_width;
        depthSpec.m_height = s_depthRenderTarget->m_height;
        depthSpec.m_format = Texture::ColorFormat::D24S8; // 32 bits
        depthSpec.m_type = Texture::Type::TEX_2D;
        depthSpec.m_min = Texture::Filter::NEAREST;
        depthSpec.m_mag = Texture::Filter::NEAREST;
        depthSpec.m_dataType = Texture::DataType::UNSIGNED_INT_24_8;
        depthSpec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        depthSpec.m_s = Texture::Wrap::CLAMP_TO_EDGE;

        TextureSpec vsmSpec = { };
        vsmSpec.m_width = s_depthRenderTarget->m_width;
        vsmSpec.m_height = s_depthRenderTarget->m_height;
        vsmSpec.m_format = Texture::ColorFormat::R32G32F;
        vsmSpec.m_type = Texture::Type::TEX_2D;
        vsmSpec.m_dataType = Texture::DataType::Float;
        vsmSpec.m_min = Texture::Filter::LINEAR;
        vsmSpec.m_mag = Texture::Filter::LINEAR;
        vsmSpec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        vsmSpec.m_s = Texture::Wrap::CLAMP_TO_EDGE;

        TextureSpec blurSpec = { };
        blurSpec.m_width = s_depthRenderTarget->m_width;
        blurSpec.m_height = s_depthRenderTarget->m_height;
        blurSpec.m_format = Texture::ColorFormat::R32G32F;
        blurSpec.m_type = Texture::Type::TEX_2D;
        blurSpec.m_dataType = Texture::DataType::Float;
        blurSpec.m_min = Texture::Filter::LINEAR;
        blurSpec.m_mag = Texture::Filter::LINEAR;
        blurSpec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        blurSpec.m_s = Texture::Wrap::CLAMP_TO_EDGE;

        m_horizontalBlurTex = textureManager->createTexture("GaussianBlurHori", blurSpec);
        m_verticalBlurTex = textureManager->createTexture("GaussianBlurVert", blurSpec);

        for (u32 s = 0u; s < kNumShadowCascades; ++s)
        {
            m_cascadedShadowMap.cascades[s].lightProjection = glm::mat4(1.f);
        }
        // initialize cascaded shadow maps
        m_cascadedShadowMap.lightView = glm::mat4(1.f);

        auto initCascade = [&](ShadowCascade& cascade, const glm::vec4& frustumColor) {
            for (auto& line : cascade.frustumLines)
            {
                line.init();
                line.setColor(frustumColor);
                cascade.basicShadowMap.shadowMap = textureManager->createTexture("ShadowMap", depthSpec);
                cascade.varianceShadowMap.shadowMap = textureManager->createTexture("VSM", vsmSpec);
            }
            cascade.aabb.init();
        };
        for (u32 i = 0; i < kNumShadowCascades; ++i)
        {
            ShadowCascade& cascade = m_cascadedShadowMap.cascades[i];
            switch (i)
            {
                case 0u:
                    initCascade(cascade, glm::vec4(1.f,0.f,0.f,1.f));
                    break;
                case 1u:
                    initCascade(cascade, glm::vec4(0.f,1.f,0.f,1.f));
                    break;
                case 2u:
                    initCascade(cascade, glm::vec4(0.f,0.f,1.f,1.f));
                    break;
                case 3u:
                    initCascade(cascade, glm::vec4(1.f,1.f,0.f,1.f));
                    break;
            }
        }
    }

    // TODO: stablize the projection matrix
    // compute view frustum's bounding box in world space
    void DirectionalShadowPass::computeCascadeAABB(ShadowCascade& cascade, const Camera& camera, glm::mat4& view)
    {
        f32 N = fabs(cascade.n);
        f32 F = fabs(cascade.f);

        static f32 fixedProjRadius = 0.f;
        // Note(Min): scale in z axis to enclose a bigger range in z, since occluder in another 
        // frusta might cast shadow on current frusta!!
        // stablize the projection matrix in xy-plane
        if ((F-N) > 2.f * F * glm::tan(glm::radians(camera.fov)) * camera.aspectRatio)
        {
           fixedProjRadius = 0.5f * (F-N) * 1.2f;
        }
        else
        {
           fixedProjRadius = F * glm::tan(glm::radians(camera.fov)) * camera.aspectRatio * 1.2f;
        }

        f32 dn = N * glm::tan(glm::radians(camera.fov));
        // comptue lower left corner of near clipping plane
        glm::vec3 cp = glm::normalize(camera.lookAt - camera.position) * N;
        glm::vec3 ca = cp + -camera.up * dn + -camera.right * dn * camera.aspectRatio;

        glm::vec3 na = camera.position + ca;
        glm::vec3 nb = na + camera.right * 2.f * dn * camera.aspectRatio;
        glm::vec3 nc = nb + camera.up * 2.f * dn;
        glm::vec3 nd = na + camera.up * 2.f * dn;

        glm::vec4 nav4 = view * glm::vec4(na, 1.f);
        glm::vec4 nbv4 = view * glm::vec4(nb, 1.f);
        glm::vec4 ncv4 = view * glm::vec4(nc, 1.f);
        glm::vec4 ndv4 = view * glm::vec4(nd, 1.f);

        f32 df = F * glm::tan(glm::radians(camera.fov));
        glm::vec3 cpf = glm::normalize(camera.lookAt - camera.position) * F;
        glm::vec3 caf = cpf + -camera.up * df + -camera.right * df  * camera.aspectRatio;

        glm::vec3 fa = camera.position + caf;
        glm::vec3 fb = fa + camera.right * 2.f * df * camera.aspectRatio;
        glm::vec3 fc = fb + camera.up * 2.f * df;
        glm::vec3 fd = fa + camera.up * 2.f * df;
        glm::vec4 fav4 = view * glm::vec4(fa, 1.f);
        glm::vec4 fbv4 = view * glm::vec4(fb, 1.f);
        glm::vec4 fcv4 = view * glm::vec4(fc, 1.f);
        glm::vec4 fdv4 = view * glm::vec4(fd, 1.f);

        // set debug line verts
        cascade.frustumLines[0].setVertices(na, nb);
        cascade.frustumLines[1].setVertices(nb, nc);
        cascade.frustumLines[2].setVertices(nc, nd);
        cascade.frustumLines[3].setVertices(na, nd);
        cascade.frustumLines[4].setVertices(fa, fb);
        cascade.frustumLines[5].setVertices(fb, fc);
        cascade.frustumLines[6].setVertices(fc, fd);
        cascade.frustumLines[7].setVertices(fa, fd);
        cascade.frustumLines[8].setVertices(na, fa);
        cascade.frustumLines[9].setVertices(nb, fb);
        cascade.frustumLines[10].setVertices(nc, fc);
        cascade.frustumLines[11].setVertices(nd, fd);

        BoundingBox3f& aabb = cascade.aabb;
        aabb.resetBound();
        aabb.bound(nav4);
        aabb.bound(nbv4);
        aabb.bound(ncv4);
        aabb.bound(ndv4);
        aabb.bound(fav4);
        aabb.bound(fbv4);
        aabb.bound(fcv4);
        aabb.bound(fdv4);

        // TODO: improve this procedure for fixing the projection size 
        f32 midX = .5f * (aabb.m_pMin.x + aabb.m_pMax.x);
        f32 midY = .5f * (aabb.m_pMin.y + aabb.m_pMax.y);
        glm::vec3 mid = 0.5f * (aabb.m_pMin + aabb.m_pMax);

        // snap to texel increments
        mid = glm::floor(mid);
        aabb.m_pMin = glm::vec4(mid - glm::vec3(fixedProjRadius), 1.f);
        aabb.m_pMax = glm::vec4(mid + glm::vec3(fixedProjRadius), 1.f);
        aabb.computeVerts();
    }

    void DirectionalShadowPass::renderCascade(ShadowCascade& cascade, glm::mat4& lightView)
    {
        auto aabb = cascade.aabb;
        glm::mat4 lightProjection = glm::orthoLH(aabb.m_pMin.x, aabb.m_pMax.x, aabb.m_pMin.y, aabb.m_pMax.y, aabb.m_pMax.z, aabb.m_pMin.z);
        cascade.lightProjection = lightProjection;
        glm::mat4 mat = glm::inverse(lightView);
        aabb.setModel(mat);
        auto ctx = getCurrentGfxCtx();
        s_depthRenderTarget->setDepthBuffer(cascade.basicShadowMap.shadowMap);
        switch (m_cascadedShadowMap.m_technique)
        {
            case ShadowTechnique::kVariance_Shadow:
                s_depthRenderTarget->attachTexture(cascade.varianceShadowMap.shadowMap, 0);
                break;
            case ShadowTechnique::kPCF_Shadow:
                // s_depthRenderTarget->attachTexture(cascade.basicShadowMap.shadowMap, 0);
                break;
        };
        ctx->setViewport({0u, 0u, s_depthRenderTarget->m_width, s_depthRenderTarget->m_height});
        ctx->setRenderTarget(s_depthRenderTarget, 0u);
        // need to clear depth to 1.0f
        ctx->setClearColor(glm::vec4(1.f));
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        ctx->setClearColor(glm::vec4(0.f));
        ctx->setShader(s_directShadowShader);
        ctx->setPrimitiveType(PrimitiveType::TriangleList);
        auto renderer = Renderer::getSingletonPtr();
        for (u32 i = 0; i < m_shadowCasters.size(); i++)
        {
            renderer->executeOnEntity(m_shadowCasters[i], [ctx, renderer, cascade, lightView](SceneNode* node) {
                if (node->m_meshInstance)
                {
                    s_directShadowMatl->set("transformIndex", node->globalTransform);
                    s_directShadowMatl->set("sunLightView", &lightView[0][0]);
                    s_directShadowMatl->set("sunLightProjection", &cascade.lightProjection[0][0]);
                    s_directShadowMatl->bind();
                    renderer->drawMesh(node->m_meshInstance->m_mesh);
                }
            });
        }
    }

    void DirectionalShadowPass::drawDebugLines(Uniform* view, Uniform* projection)
    {
        glm::mat4 mvp(1.f);
        for (u32 i = 0; i < kNumShadowCascades; ++i)
        {
            auto& cascade = m_cascadedShadowMap.cascades[i];
            cascade.aabb.setModel(glm::inverse(m_cascadedShadowMap.lightView));
            cascade.aabb.setViewProjection(view, projection);
            cascade.aabb.draw();
            for (auto& line : cascade.frustumLines)
            {
                line.draw(mvp);
            }
        }
    }

    void DirectionalShadowPass::render()
    {
        Camera& camera = m_camera;
        f32 t[4] = { 0.1f, 0.3f, 0.6f, 1.f };
        m_cascadedShadowMap.cascades[0].n = camera.n;
        m_cascadedShadowMap.cascades[0].f = (1.0f - t[0]) * camera.n + t[0] * camera.f;
        for (u32 i = 1u; i < DirectionalShadowPass::kNumShadowCascades; ++i)
        {
            m_cascadedShadowMap.cascades[i].n = m_cascadedShadowMap.cascades[i-1].f;
            m_cascadedShadowMap.cascades[i].f = (1.f - t[i]) * camera.n + t[i] * camera.f;
        }
        m_cascadedShadowMap.lightView = glm::lookAt(glm::vec3(0.f), glm::vec3(-m_light.direction.x, -m_light.direction.y, -m_light.direction.z), glm::vec3(0.f, 1.f, 0.f));
        for (u32 i = 0u; i < DirectionalShadowPass::kNumShadowCascades; ++i)
        {
            computeCascadeAABB(m_cascadedShadowMap.cascades[i], camera, m_cascadedShadowMap.lightView);
            renderCascade(m_cascadedShadowMap.cascades[i], m_cascadedShadowMap.lightView);
        }
        Renderer::getSingletonPtr()->updateSunShadow();
    }

    SSAOPass::SSAOPass(RenderTarget* renderTarget, Viewport vp, Texture* sceneDepthTexture, Texture* sceneNormalTexture)
        : RenderPass(renderTarget, vp), m_sceneDepthTexture(sceneDepthTexture), m_sceneNormalTexture(sceneNormalTexture)
    {

    }

    void SSAOPass::onInit()
    {

    }

    void SSAOPass::render()
    {

    }

    TexturedQuadPass::TexturedQuadPass(RenderTarget* renderTarget, Viewport vp, Texture* srcTex)
        : RenderPass(renderTarget, vp), m_srcTexture(srcTex)
    {

    }

    void TexturedQuadPass::onInit()
    {
        m_shader = createShader("TexturedQuadShader", "../../shader/shader_textured_quad.vs", "../../shader/shader_textured_quad.fs");
        m_matl = createMaterial(m_shader)->createInstance();
    }

    void TexturedQuadPass::render()
    {
        auto ctx = getCurrentGfxCtx();
        ctx->setRenderTarget(m_renderTarget, 0u);
        ctx->setViewport(m_viewport);
        ctx->setShader(TexturedQuadPass::m_shader);
        m_matl->bindTexture("srcTexture", m_srcTexture);
        m_matl->bind();
        ctx->setVertexArray(s_quadMesh.m_vertexArray);
        ctx->setPrimitiveType(PrimitiveType::TriangleList);
        ctx->setDepthControl(DepthControl::kDisable);
        ctx->drawIndexAuto(6u);
        ctx->setDepthControl(DepthControl::kEnable);
    }
}