#include "Shadow.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"

namespace Cyan
{
    ShadowmapManager::ShadowmapManager()
        : m_sunShadowShader(nullptr), m_pointShadowShader(nullptr), m_sunShadowMatl(nullptr)
    {
        initialize();
    }

    void ShadowmapManager::initialize()
    {
        // TODO: depth buffer creation coupled with render target creation
        m_sunShadowShader = createShader("DirShadowShader", "../../shader/shader_dir_shadow.vs", "../../shader/shader_dir_shadow.fs");
        m_sunShadowMatl = createMaterial(m_sunShadowShader)->createInstance();
    }
    
    void ShadowmapManager::finalize()
    {

    }

    void ShadowmapManager::initShadowmap(CascadedShadowmap& csm, const glm::uvec2& resolution)
    {
        csm.depthRenderTarget = createDepthRenderTarget(resolution.x, resolution.y);

        auto textureManager = TextureManager::getSingletonPtr();
        auto initCascade = [this, textureManager, csm](ShadowCascade& cascade, const glm::vec4& frustumColor) {
            TextureSpec spec = { };
            spec.width = csm.depthRenderTarget->width;
            spec.height = csm.depthRenderTarget->height;
            spec.format = Texture::ColorFormat::D24S8; // 32 bits
            spec.type = Texture::Type::TEX_2D;
            spec.min = Texture::Filter::NEAREST;
            spec.mag = Texture::Filter::NEAREST;
            spec.dataType = Texture::DataType::UNSIGNED_INT_24_8;
            spec.r = Texture::Wrap::CLAMP_TO_EDGE;
            spec.s = Texture::Wrap::CLAMP_TO_EDGE;

            TextureSpec vsmSpec = { };
            vsmSpec.width = csm.depthRenderTarget->width;
            vsmSpec.height = csm.depthRenderTarget->height;
            vsmSpec.format = Texture::ColorFormat::R32G32F;
            vsmSpec.type = Texture::Type::TEX_2D;
            vsmSpec.dataType = Texture::DataType::Float;
            vsmSpec.min = Texture::Filter::LINEAR;
            vsmSpec.mag = Texture::Filter::LINEAR;
            vsmSpec.r = Texture::Wrap::CLAMP_TO_EDGE;
            vsmSpec.s = Texture::Wrap::CLAMP_TO_EDGE;

            for (auto& line : cascade.frustumLines)
            {
                line.init();
                line.setColor(frustumColor);
                cascade.basicShadowmap.shadowmap = textureManager->createTexture("Shadowmap", spec);
                cascade.vsm.shadowmap = textureManager->createTexture("vsm", vsmSpec);
            }
            cascade.aabb.init();
        };

        for (u32 i = 0; i < CascadedShadowmap::kNumCascades; ++i)
        {
            ShadowCascade& cascade = csm.cascades[i];
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

    void ShadowmapManager::updateShadowCascade(ShadowCascade& cascade, const Camera& camera, const glm::mat4& view)
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
        aabb.reset();
        aabb.bound(nav4);
        aabb.bound(nbv4);
        aabb.bound(ncv4);
        aabb.bound(ndv4);
        aabb.bound(fav4);
        aabb.bound(fbv4);
        aabb.bound(fcv4);
        aabb.bound(fdv4);

        // TODO: improve this procedure for fixing the projection size 
        f32 midX = .5f * (aabb.pmin.x + aabb.pmax.x);
        f32 midY = .5f * (aabb.pmin.y + aabb.pmax.y);
        glm::vec3 mid = 0.5f * (aabb.pmin + aabb.pmax);

        // snap to texel increments
        mid = glm::floor(mid);
        aabb.pmin = glm::vec4(mid - glm::vec3(fixedProjRadius), 1.f);
        aabb.pmax = glm::vec4(mid + glm::vec3(fixedProjRadius), 1.f);
        aabb.update();
    }

    void ShadowmapManager::render(CascadedShadowmap& csm, Scene* scene, const DirectionalLight& sunLight, const std::vector<Entity*>& shadowCasters)
    {
        const auto& camera = scene->getActiveCamera();
        f32 t[4] = { 0.1f, 0.3f, 0.6f, 1.f };
        csm.cascades[0].n = camera.n;
        csm.cascades[0].f = (1.0f - t[0]) * camera.n + t[0] * camera.f;
        for (u32 i = 1u; i < CascadedShadowmap::kNumCascades; ++i)
        {
            csm.cascades[i].n = csm.cascades[i-1].f;
            csm.cascades[i].f = (1.f - t[i]) * camera.n + t[i] * camera.f;
        }
        csm.lightView = glm::lookAt(glm::vec3(0.f), glm::vec3(-sunLight.direction.x, -sunLight.direction.y, -sunLight.direction.z), glm::vec3(0.f, 1.f, 0.f));

        switch (csm.technique)
        {
        case kPCF_Shadow:
            for (u32 i = 0u; i < CascadedShadowmap::kNumCascades; ++i)
            {
                updateShadowCascade(csm.cascades[i], camera, csm.lightView);
                pcfShadow(csm, i, scene, csm.lightView, shadowCasters);
            }
            break;
        case kVariance_Shadow:
            vsmShadow();
            break;
        default:
            break;
        }
    }

    void ShadowmapManager::pcfShadow(CascadedShadowmap& csm, u32& cascadeIndex, Scene* scene, const glm::mat4& lightView, const std::vector<Entity*>& shadowCasters)
    {
        auto& cascade = csm.cascades[cascadeIndex];
        auto aabb = cascade.aabb;
        glm::mat4 lightProjection = glm::orthoLH(aabb.pmin.x, aabb.pmax.x, aabb.pmin.y, aabb.pmax.y, aabb.pmax.z, aabb.pmin.z);
        cascade.lightProjection = lightProjection;
        auto renderer = Renderer::getSingletonPtr();
        auto ctx = renderer->getGfxCtx();

        csm.depthRenderTarget->setDepthBuffer(cascade.basicShadowmap.shadowmap);
        ctx->setViewport({ 0u, 0u, csm.depthRenderTarget->width, csm.depthRenderTarget->height });
        ctx->setRenderTarget(csm.depthRenderTarget, { 0 });
        // need to clear depth to 1.0f
        ctx->setClearColor(glm::vec4(1.f));
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        ctx->setClearColor(glm::vec4(0.f));

        ctx->setShader(m_sunShadowShader);
        ctx->setPrimitiveType(PrimitiveType::TriangleList);
        for (u32 i = 0; i < shadowCasters.size(); i++)
        {
            renderer->executeOnEntity(shadowCasters[i], [this, ctx, renderer, cascade, lightView](SceneNode* node) {
                if (node->m_meshInstance)
                {
                    m_sunShadowMatl->set("transformIndex", node->globalTransform);
                    m_sunShadowMatl->set("sunLightView", &lightView[0][0]);
                    m_sunShadowMatl->set("sunLightProjection", &cascade.lightProjection[0][0]);
                    m_sunShadowMatl->bind();
                    renderer->drawMesh(node->m_meshInstance->m_mesh);
                }
            });
        }
    }

    void ShadowmapManager::vsmShadow()
    {

    }
}
