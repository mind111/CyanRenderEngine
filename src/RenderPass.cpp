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
    RenderTarget* DirectionalShadowPass::s_depthRenderTarget = 0;
    Shader* DirectionalShadowPass::s_directShadowShader = 0;
    MaterialInstance* DirectionalShadowPass::s_directShadowMatl = 0;
    ShadowMapData* DirectionalShadowPass::s_shadowMap = 0;
    std::vector<::Line> DirectionalShadowPass::m_frustumLines; 
    BoundingBox3f DirectionalShadowPass::m_frustumAABB; 

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
        renderer->renderScene(m_scene, m_scene->getActiveCamera());
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
    
    DirectionalShadowPass::DirectionalShadowPass(RenderTarget* renderTarget, Viewport viewport, Scene* scene, u32 dirLightIdx)
        : RenderPass(renderTarget, viewport), m_scene(scene)
    {
        m_light = scene->dLights[dirLightIdx];
    }

    void DirectionalShadowPass::onInit()
    {
        s_shadowMap = new ShadowMapData { };
        // TODO: depth buffer creation coupled with render target creation
        s_depthRenderTarget = createDepthRenderTarget(1024, 1024);
        s_directShadowShader = createShader("DirShadowShader", "../../shader/shader_dir_shadow.vs", "../../shader/shader_dir_shadow.fs");
        s_directShadowMatl = createMaterial(s_directShadowShader)->createInstance();
        TextureSpec spec = { };
        spec.m_width = 1024;
        spec.m_height = 1024;
        spec.m_format = Texture::ColorFormat::D24S8; // 32 bits
        spec.m_dataType = Texture::DataType::UNSIGNED_INT_24_8;
        spec.m_min = Texture::Filter::LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        s_shadowMap->shadowMap = createTexture("DepthTexture", spec);
        s_shadowMap->lightView = glm::mat4(1.f);
        s_shadowMap->lightProjection = glm::mat4(1.f);
        
        // debuging lines for camera's view frustum
        m_frustumLines.resize(15);
        auto renderer = Renderer::getSingletonPtr();
        for (auto& line : m_frustumLines)
        {
            line.init();
            line.setColor(glm::vec4(1.f, 0.f, 0.f, 1.f));
            line.setModel(glm::mat4(1.f));
        }
        m_frustumLines[13].setColor(glm::vec4(0.f, 0.f, 1.f, 1.f));
        m_frustumLines[14].setColor(glm::vec4(0.f, 1.f, 0.f, 1.f));
        m_frustumAABB.init();
    }

    ShadowMapData* DirectionalShadowPass::getShadowMap()
    {
        return s_shadowMap;
    }

    // TODO: stablize the projection matrix
    // compute view frustum's bounding box in world space
    BoundingBox3f DirectionalShadowPass::computeFrustumAABB(const Camera& camera, glm::mat4& view)
    {
        DirectionalShadowPass::m_frustumAABB.resetBound();
        f32 N = fabs(camera.n);
        f32 F = fabs(20.f);
        static f32 fixedProjSizeX = 0.f;
        static f32 fixedProjSizeY = 0.f;
        // stablize the projection matrix in xy-plane
        if ((F-N) > 2.f * F * glm::tan(glm::radians(camera.fov)) * camera.aspectRatio)
        {
            fixedProjSizeX = 0.5f * (F-N);
            fixedProjSizeY = F * glm::tan(glm::radians(camera.fov));
        }
        else
        {
            fixedProjSizeX = F * glm::tan(glm::radians(camera.fov)) * camera.aspectRatio;
            fixedProjSizeY = F * glm::tan(glm::radians(camera.fov));
        }

        f32 dn = N * glm::tan(glm::radians(camera.fov));
        // comptue lower left corner on near clipping plane
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
        m_frustumLines[0].setVerts(na, nb);
        m_frustumLines[1].setVerts(nb, nc);
        m_frustumLines[2].setVerts(nc, nd);
        m_frustumLines[3].setVerts(na, nd);
        m_frustumLines[4].setVerts(fa, fb);
        m_frustumLines[5].setVerts(fb, fc);
        m_frustumLines[6].setVerts(fc, fd);
        m_frustumLines[7].setVerts(fa, fd);
        m_frustumLines[8].setVerts(na, fa);
        m_frustumLines[9].setVerts(nb, fb);
        m_frustumLines[10].setVerts(nc, fc);
        m_frustumLines[11].setVerts(nd, fd);
        m_frustumLines[12].setVerts(camera.position, camera.position + camera.forward);
        m_frustumLines[13].setVerts(camera.position, camera.position + camera.up);
        m_frustumLines[14].setVerts(camera.position, camera.position + camera.right);

        m_frustumAABB.bound(nav4);
        m_frustumAABB.bound(nbv4);
        m_frustumAABB.bound(ncv4);
        m_frustumAABB.bound(ndv4);

        m_frustumAABB.bound(fav4);
        m_frustumAABB.bound(fbv4);
        m_frustumAABB.bound(fcv4);
        m_frustumAABB.bound(fdv4);

        // FIXME: this is bugged
        f32 midX = .5f * (m_frustumAABB.m_pMin.x + m_frustumAABB.m_pMax.x);
        f32 midY = .5f * (m_frustumAABB.m_pMin.y + m_frustumAABB.m_pMax.y);
        m_frustumAABB.m_pMin.x = -fixedProjSizeX + midX;
        m_frustumAABB.m_pMin.y = -fixedProjSizeY + midY;
        m_frustumAABB.m_pMax.x = fixedProjSizeX + midX;
        m_frustumAABB.m_pMax.y = fixedProjSizeY + midY;

        m_frustumAABB.computeVerts();
        return m_frustumAABB;
    }

    void DirectionalShadowPass::render()
    {
        // render debug view frustum
        u32 debugCameraIdx = (m_scene->activeCamera + 1) % 2;
        glm::mat4 lightView = glm::lookAt(glm::vec3(0.f), 
                                          glm::vec3(-m_light.direction.x, -m_light.direction.y, -m_light.direction.z), glm::vec3(0.f, 1.f, 0.f));

        u32 camIdx = 0u;
        Camera& camera = m_scene->cameras[camIdx];
        BoundingBox3f aabb = computeFrustumAABB(camera, lightView);
        m_frustumAABB.setModel(glm::inverse(lightView));
        // m_frustumAABB.setModel(glm::mat4(1.f));

        // Note:(Min) this projection matrix maps depth to [-1, 1] while the depth stored in the depth buffer is in range [0, 1]
        // a scale and bias is needed to convert the depth in order to comare the depth correctly.
        glm::mat4 lightProjection = glm::orthoLH(aabb.m_pMin.x, aabb.m_pMax.x, aabb.m_pMin.y, aabb.m_pMax.y, aabb.m_pMax.z, aabb.m_pMin.z);
        glm::vec4 test = lightProjection * glm::vec4(0.f, 0.f, 1.0f, 1.f);

        s_shadowMap->lightView = lightView;
        s_shadowMap->lightProjection = lightProjection;

        auto ctx = getCurrentGfxCtx();
        s_depthRenderTarget->setDepthBuffer(s_shadowMap->shadowMap);
        ctx->setRenderTarget(s_depthRenderTarget, 0u);
        glClear(GL_DEPTH_BUFFER_BIT);
        ctx->setShader(s_directShadowShader);
        s_directShadowMatl->set("lightView", &lightView[0][0]);
        s_directShadowMatl->set("lightProjection", &lightProjection[0][0]);
        ctx->setViewport({0u, 0u, s_depthRenderTarget->m_width, s_depthRenderTarget->m_height});
        for (auto entity : m_scene->entities)
        {
            std::queue<SceneNode*> nodes;
            nodes.push(entity->m_sceneRoot);
            while(!nodes.empty())
            {
                SceneNode* node = nodes.front(); 
                nodes.pop();
                for (auto child : node->m_child)
                {
                    nodes.push(child);
                }
                if (MeshInstance* meshInstance = node->m_meshInstance)
                {
                    // TODO: clean this up
                    if (node->m_hasAABB)
                    {
                        glm::mat4 model = node->getWorldTransform().toMatrix();
                        s_directShadowMatl->set("model", &model[0][0]);
                        s_directShadowMatl->bind();

                        u32 smIndex = 0u;
                        for (auto sm : meshInstance->m_mesh->m_subMeshes)
                        {
                            ctx->setVertexArray(sm->m_vertexArray);
                            if (sm->m_vertexArray->m_ibo != static_cast<u32>(-1))
                            {
                                ctx->drawIndex(sm->m_vertexArray->m_numIndices);
                            }
                            else
                            {
                                ctx->drawIndexAuto(sm->m_vertexArray->numVerts());
                            }
                            smIndex++;
                        }
                    }
                }
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