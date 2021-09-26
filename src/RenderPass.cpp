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
    Shader* BloomPass::s_bloomDsShader = 0;
    MaterialInstance* BloomPass::s_bloomDsMatl = 0;
    Shader* BloomPass::s_bloomUsShader = 0;
    MaterialInstance* BloomPass::s_bloomUsMatl = 0;
    Shader* BloomPass::s_gaussianBlurShader = 0;
    MaterialInstance* BloomPass::s_gaussianBlurMatl = 0;
    BloomPass::BloomSurface BloomPass::s_bloomDsSurfaces[BloomPass::kNumBloomDsPass] = {0};          // downsample
    BloomPass::BloomSurface BloomPass::s_bloomUsSurfaces[BloomPass::kNumBloomDsPass] = {0};          // upsample
    Shader* TexturedQuadPass::m_shader = 0;
    MaterialInstance* TexturedQuadPass::m_matl = 0;

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
        spec.m_dataType = Texture::DataType::Float;
        spec.m_min = Texture::Filter::LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        s_bloomSetupRT->attachColorBuffer(createTexture("BloomSetupTexture", spec));
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
        GaussianBlurInputs inputs = { };

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

    void BloomPass::gaussianBlur(BloomSurface src, GaussianBlurInputs inputs)
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
        GaussianBlurInputs inputs = { };
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
    
    DirectionalShadowPass::DirectionalShadowPass(RenderTarget* renderTarget, Viewport viewport)
        : RenderPass(renderTarget, viewport)
    {

    }

    void DirectionalShadowPass::render()
    {
        Shader* shader = createShader("DirShadowShader", "../../shader/shader_dir_shadow.vs", "../../shader/shader_dir_shadow.fs");
        RenderTarget* rt = createDepthRenderTarget(1024, 1024);
        // setup render target to only has a depth buffer
        // build view matrix that transform the world to light's view space
        glm::mat4 lightView = glm::lookAt(glm::vec3(0.f), 
                                          glm::vec3(m_light.direction.x, m_light.direction.y, m_light.direction.z), glm::vec3(0.f, 1.f, 0.f));
        for (auto entity : m_scene->entities)
        {
            std::queue<SceneNode*> nodes;
            nodes.push(entity->m_sceneRoot);
            while(!nodes.empty())
            {
            }
        }
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