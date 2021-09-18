#include "RenderPass.h"
#include "cyanRenderer.h"

namespace Cyan
{
    //- static initialization of class static data members

    static QuadMesh s_quadMesh;
    // post process resolve
    Shader* PostProcessResolvePass::s_finalCompositeShader = 0;
    MaterialInstance* PostProcessResolvePass::s_matl = 0;
    // bloom
    RenderTarget* BloomPass::s_bloomSetupRT = 0;
    Shader* BloomPass::s_bloomSetupShader = 0;
    MaterialInstance* BloomPass::s_bloomSetupMatl = 0;
    BloomPass::BloomSurface BloomPass::s_bloomDsSurfaces[BloomPass::kNumBloomDsPass] = {0};          // downsample
    BloomPass::BloomSurface BloomPass::s_bloomUsSurfaces[BloomPass::kNumBloomDsPass] = {0};          // upsample
    Shader* TexturedQuadPass::m_shader = 0;
    MaterialInstance* TexturedQuadPass::m_matl = 0;

    void onRendererInitialized(glm::vec2 windowSize)
    {
        u32 windowWidth = static_cast<u32>(windowSize.x);
        u32 windowHeight = static_cast<u32>(windowSize.y);

        s_quadMesh.init(glm::vec2(0.f, 0.f), glm::vec2(1.0f, 1.0f));

        ScenePass::onInit();
        BloomPass::onInit(windowWidth, windowHeight);
        PostProcessResolvePass::onInit();
        TexturedQuadPass::onInit();
/*
        PostProcessResolvePass::s_finalCompositeShader = createShader("BlitShader", "../../shader/shader_blit.vs", "../../shader/shader_blit.fs");
        PostProcessResolvePass::s_matl = createMaterial(PostProcessResolvePass::s_finalCompositeShader)->createInstance();

        BloomPass::s_bloomSetupRT = createRenderTarget(windowWidth, windowHeight);
        TextureSpec spec = { };
        spec.m_width = windowWidth;
        spec.m_height = windowHeight;
        spec.m_type = Texture::Type::TEX_2D;
        spec.m_format = Texture::ColorFormat::R16G16B16A16; 
        spec.m_min = Texture::Filter::LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        BloomPass::s_bloomSetupRT->attachColorBuffer(createTexture("BloomSetupTexture", spec));
        BloomPass::s_bloomSetupShader = createShader("BloomSetupShader", "../../shader/shader_bloom_preprocess.vs", "../../shader/shader_bloom_preprocess.fs");
        BloomPass::s_bloomSetupMatl = createMaterial(BloomPass::s_bloomSetupShader)->createInstance();

        u32 numBloomTextures = 0u;
        auto initBloomBuffers = [&](u32 index, TextureSpec& spec) {
            BloomPass::s_bloomDsSurfaces[index].m_renderTarget = createRenderTarget(spec.m_width, spec.m_height);
            char buff[64];
            sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
            BloomPass::s_bloomDsSurfaces[index].m_pingPongColorBuffers[0] = createTextureHDR(buff, spec);
            sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
            BloomPass::s_bloomDsSurfaces[index].m_pingPongColorBuffers[1] = createTextureHDR(buff, spec);
            BloomPass::s_bloomDsSurfaces[index].m_renderTarget->attachColorBuffer(BloomPass::s_bloomDsSurfaces[index].m_pingPongColorBuffers[0]);
            BloomPass::s_bloomDsSurfaces[index].m_renderTarget->attachColorBuffer(BloomPass::s_bloomDsSurfaces[index].m_pingPongColorBuffers[1]);

            BloomPass::s_bloomUsSurfaces[index].m_renderTarget = createRenderTarget(spec.m_width, spec.m_height);
            sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
            BloomPass::s_bloomUsSurfaces[index].m_pingPongColorBuffers[0] = createTextureHDR(buff, spec);
            sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
            BloomPass::s_bloomUsSurfaces[index].m_pingPongColorBuffers[1] = createTextureHDR(buff, spec);
            BloomPass::s_bloomUsSurfaces[index].m_renderTarget->attachColorBuffer(BloomPass::s_bloomUsSurfaces[index].m_pingPongColorBuffers[0]);
            BloomPass::s_bloomUsSurfaces[index].m_renderTarget->attachColorBuffer(BloomPass::s_bloomUsSurfaces[index].m_pingPongColorBuffers[1]);
            spec.m_width /= 2;
            spec.m_height /= 2;
        };

        for (u32 i = 0u; i < BloomPass::kNumBloomDsPass; ++i) {
            initBloomBuffers(i, spec);
        }
        */
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

    }

    void ScenePass::render()
    {
        auto renderer = Renderer::getSingletonPtr();
        auto ctx = getCurrentGfxCtx();
        ctx->setRenderTarget(m_renderTarget, 0u);
        ctx->setViewport(m_viewport);
        renderer->renderScene(m_scene);
    }

    BloomPass::BloomPass(RenderTarget* rt, Viewport vp, BloomPassInputs inputs) 
        : RenderPass(rt, vp), m_inputs(inputs)
    {

    }

    void BloomPass::onInit(u32 windowWidth, u32 windowHeight)
    {
        s_bloomSetupRT = createRenderTarget(windowWidth, windowHeight);
        TextureSpec spec = { };
        spec.m_width = windowWidth;
        spec.m_height = windowHeight;
        spec.m_type = Texture::Type::TEX_2D;
        spec.m_format = Texture::ColorFormat::R16G16B16A16; 
        spec.m_min = Texture::Filter::LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        s_bloomSetupRT->attachColorBuffer(createTexture("BloomSetupTexture", spec));
        s_bloomSetupShader = createShader("BloomSetupShader", "../../shader/shader_bloom_preprocess.vs", "../../shader/shader_bloom_preprocess.fs");
        s_bloomSetupMatl = createMaterial(BloomPass::s_bloomSetupShader)->createInstance();

        u32 numBloomTextures = 0u;
        auto initBloomBuffers = [&](u32 index, TextureSpec& spec) {
            BloomPass::s_bloomDsSurfaces[index].m_renderTarget = createRenderTarget(spec.m_width, spec.m_height);
            char buff[64];
            sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
            BloomPass::s_bloomDsSurfaces[index].m_pingPongColorBuffers[0] = createTextureHDR(buff, spec);
            sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
            BloomPass::s_bloomDsSurfaces[index].m_pingPongColorBuffers[1] = createTextureHDR(buff, spec);
            BloomPass::s_bloomDsSurfaces[index].m_renderTarget->attachColorBuffer(BloomPass::s_bloomDsSurfaces[index].m_pingPongColorBuffers[0]);
            BloomPass::s_bloomDsSurfaces[index].m_renderTarget->attachColorBuffer(BloomPass::s_bloomDsSurfaces[index].m_pingPongColorBuffers[1]);

            BloomPass::s_bloomUsSurfaces[index].m_renderTarget = createRenderTarget(spec.m_width, spec.m_height);
            sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
            BloomPass::s_bloomUsSurfaces[index].m_pingPongColorBuffers[0] = createTextureHDR(buff, spec);
            sprintf_s(buff, "BloomTexture%u", numBloomTextures++);
            BloomPass::s_bloomUsSurfaces[index].m_pingPongColorBuffers[1] = createTextureHDR(buff, spec);
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
        Renderer::GaussianBlurInputs inputs = { };

        inputs.kernelIndex = 0;
        inputs.radius = kernelRadii[0];
        auto renderer = Renderer::getSingletonPtr();
        renderer->downSample(s_bloomSetupRT, 0, s_bloomDsSurfaces[0].m_renderTarget, 0);
        renderer->gaussianBlur(s_bloomDsSurfaces[0], inputs);
        for (u32 i = 0u; i < kNumBloomDsPass - 1; ++i) 
        {
            Renderer::getSingletonPtr()->downSample(s_bloomDsSurfaces[i].m_renderTarget, 0, s_bloomDsSurfaces[i+1].m_renderTarget, 0);
            inputs.kernelIndex = static_cast<i32>(i+1);
            inputs.radius = kernelRadii[i+1];
            Renderer::getSingletonPtr()->gaussianBlur(s_bloomDsSurfaces[i+1], inputs);
        }
        // copy result to output texture
    }
    
    void BloomPass::bloomUpscale()
    {
        i32 kernelRadii[6] = { 3, 4, 6, 7, 8, 9};
        Renderer::GaussianBlurInputs inputs = { };
        auto renderer = Renderer::getSingletonPtr();
        inputs.kernelIndex = 5;
        inputs.radius = kernelRadii[5];
        renderer->gaussianBlur(s_bloomDsSurfaces[kNumBloomDsPass-2], inputs);

        Renderer::UpScaleInputs upScaleInputs = { };
        upScaleInputs.stageIndex = 0;

        renderer->upSample(s_bloomDsSurfaces[kNumBloomDsPass - 1].m_renderTarget, 0, 
                s_bloomUsSurfaces[kNumBloomDsPass-2].m_renderTarget, 0, 
                s_bloomDsSurfaces[kNumBloomDsPass-2].m_renderTarget, 0,
                upScaleInputs);

        for (u32 i = kNumBloomDsPass - 2; i > 0 ; --i) {
            inputs.kernelIndex = static_cast<i32>(i);
            inputs.radius = kernelRadii[i];
            renderer->gaussianBlur(s_bloomDsSurfaces[i-1], inputs);

            upScaleInputs.stageIndex = kNumBloomDsPass - 1 - i;
            renderer->upSample(s_bloomUsSurfaces[i].m_renderTarget, 0, s_bloomUsSurfaces[i-1].m_renderTarget, 0, s_bloomDsSurfaces[i-1].m_renderTarget, 0, upScaleInputs);
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

    PostProcessResolvePass::PostProcessResolvePass(RenderTarget* rt, Viewport vp, PostProcessResolveInputs inputs)
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