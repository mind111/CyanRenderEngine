#include "Shadow.h"
#include "Scene.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
#include "RenderableScene.h"
#include "DirectionalLight.h"
#include "AssetManager.h"

namespace Cyan
{
    RasterDirectShadowManager::RasterDirectShadowManager()
        : m_sunShadowShader(nullptr), m_pointShadowShader(nullptr)
    {

    }

    void RasterDirectShadowManager::initialize()
    {
        m_sunShadowShader = ShaderManager::createShader({ ShaderType::kVsPs, "DirShadowShader", SHADER_SOURCE_PATH "shader_dir_shadow.vs", SHADER_SOURCE_PATH "shader_dir_shadow.fs" });
    }
    
    void RasterDirectShadowManager::finalize()
    {

    }

    void RasterDirectShadowManager::initShadowmap(NewCascadedShadowmap& csm, const glm::uvec2& resolution)
    {
        csm.depthRenderTarget = createDepthOnlyRenderTarget(resolution.x, resolution.y);

        auto initCascade = [this, csm](ShadowCascade& cascade, const glm::vec4& frustumColor) {
            ITextureRenderable::Spec spec = { };
            spec.width = csm.depthRenderTarget->width;
            spec.height = csm.depthRenderTarget->height;
            spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::D24S8;
            ITextureRenderable::Parameter parameter = { };
            parameter.minificationFilter = ITextureRenderable::Parameter::Filtering::NEAREST;
            parameter.magnificationFilter = ITextureRenderable::Parameter::Filtering::NEAREST;

            ITextureRenderable::Spec vsmSpec = { };
            vsmSpec.width = csm.depthRenderTarget->width;
            vsmSpec.height = csm.depthRenderTarget->height;
            vsmSpec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R32G32F;
#if 0
            for (auto& line : cascade.frustumLines)
            {
                line.init();
                line.setColor(frustumColor);
            }
#endif
            cascade.basicShadowmap.shadowmap = AssetManager::createDepthTexture("Shadowmap", spec.width, spec.height);
            cascade.vsm.shadowmap = AssetManager::createTexture2D("VSM", vsmSpec);
            cascade.aabb.init();
        };

        for (u32 i = 0; i < NewCascadedShadowmap::kNumCascades; ++i)
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

    void RasterDirectShadowManager::updateShadowCascade(ShadowCascade& cascade, const Camera& camera, const glm::mat4& view)
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
#if 0
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
#endif

        BoundingBox3D& aabb = cascade.aabb;
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
        // aabb.update();
    }

    void RasterDirectShadowManager::render(NewCascadedShadowmap& csm, Scene* scene, const DirectionalLight& sunLight, const std::vector<Entity*>& shadowCasters)
    {
#if 0
        const auto& camera = scene->camera;
        f32 t[4] = { 0.1f, 0.3f, 0.6f, 1.f };
        csm.cascades[0].n = camera.n;
        csm.cascades[0].f = (1.0f - t[0]) * camera.n + t[0] * camera.f;
        for (u32 i = 1u; i < NewCascadedShadowmap::kNumCascades; ++i)
        {
            csm.cascades[i].n = csm.cascades[i-1].f;
            csm.cascades[i].f = (1.f - t[i]) * camera.n + t[i] * camera.f;
        }
        csm.lightView = glm::lookAt(glm::vec3(0.f), glm::vec3(-sunLight.direction.x, -sunLight.direction.y, -sunLight.direction.z), glm::vec3(0.f, 1.f, 0.f));

        switch (csm.technique)
        {
        case kPCF_Shadow:
            for (u32 i = 0u; i < NewCascadedShadowmap::kNumCascades; ++i)
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
#endif
    }

    // todo: try to reduce custom rendering code that directly uses m_ctx
    void RasterDirectShadowManager::pcfShadow(NewCascadedShadowmap& csm, u32& cascadeIndex, Scene* scene, const glm::mat4& lightView, const std::vector<Entity*>& shadowCasters)
    {
        auto& cascade = csm.cascades[cascadeIndex];
        auto aabb = cascade.aabb;
        glm::mat4 lightProjection = glm::orthoLH(aabb.pmin.x, aabb.pmax.x, aabb.pmin.y, aabb.pmax.y, aabb.pmax.z, aabb.pmin.z);
        cascade.lightProjection = lightProjection;

        auto renderer = Renderer::get();
        csm.depthRenderTarget->setDepthBuffer(cascade.basicShadowmap.shadowmap);
        csm.depthRenderTarget->clear({});

        for (u32 i = 0; i < shadowCasters.size(); i++)
        {
            shadowCasters[i]->visit([this, renderer, cascade, lightView, &csm](SceneComponent* node) {
                if (auto meshInst = node->getAttachedMesh())
                {
                    renderer->submitMesh(
                        csm.depthRenderTarget, 
                        { { 0 } },
                        false,
                        { 0u, 0u, csm.depthRenderTarget->width, csm.depthRenderTarget->height },
                        GfxPipelineState(),
                        meshInst->parent,
                        m_sunShadowShader,
                        [node, lightView, cascade](RenderTarget* renderTarget, Shader* shader) {
                            shader->setUniform("transformIndex", node->globalTransform)
                                     .setUniform("sunLightView", lightView)
                                     .setUniform("sunLightProjection", cascade.lightProjection);
                        });
                }
            });
        }
    }

    void RasterDirectShadowManager::vsmShadow()
    {

    }

    CascadedShadowmap::CascadedShadowmap(const DirectionalLight& inDirectionalLight)
        : IDirectionalShadowmap(), lightDirection(inDirectionalLight.direction)
    {
        u32 width = 0u, height = 0u;
        switch (inDirectionalLight.shadowQuality)
        {
        case DirectionalLight::ShadowQuality::kLow:
        case DirectionalLight::ShadowQuality::kMedium:
        case DirectionalLight::ShadowQuality::kHigh:
        {
            width = 4096;
            height = 4096;
        } break;
        default:
            break;
        }

        // initialize cascades' shadowmap
        for (u32 i = 0; i < kNumCascades; ++i)
        {
            char depthTextureName[64] = { };
            sprintf_s(depthTextureName, "CascadedShadowmap.cascades[%d]_%dx%d", i, width, height);
            cascades[i].shadowmap = AssetManager::createDepthTexture(depthTextureName, width, height);
        }
    }

    // todo: think about how to efficiently create/destroy transient gpu resources such as texture and render target
    void CascadedShadowmap::render(const Scene& scene, Renderer& renderer)
    {
        updateCascades(scene.camera);

        // create render target
        std::unique_ptr<RenderTarget> depthRenderTargetPtr(createDepthOnlyRenderTarget(cascades[0].shadowmap->width, cascades[0].shadowmap->height));
        for (u32 i = 0; i < kNumCascades; ++i)
        {
            depthRenderTargetPtr->setDepthBuffer(cascades[i].shadowmap);

            // todo: this is just a temporary hack camera should be orthographic
            Camera camera = { };
            camera.view = glm::lookAt(glm::vec3(0.f), -lightDirection, glm::vec3(0.f, 1.f, 0.f));
            camera.projection = glm::orthoLH(cascades[i].aabb.pmin.x, cascades[i].aabb.pmax.x, cascades[i].aabb.pmin.y, cascades[i].aabb.pmax.y, cascades[i].aabb.pmax.z, cascades[i].aabb.pmin.z);
            cascades[i].lightProjection = camera.projection;
            SceneView sceneView(scene, camera, depthRenderTargetPtr.get(), { }, { 0u, 0u, depthRenderTargetPtr->width, depthRenderTargetPtr->height }, EntityFlag_kVisible | EntityFlag_kCastShadow);
            SceneRenderable renderableScene(&scene, sceneView, renderer.getFrameAllocator());

            // render
            renderer.renderSceneDepthOnly(renderableScene, sceneView);
        }
    }

    void CascadedShadowmap::setShaderParameters(Shader* shader)
    {
        for (u32 i = 0; i < kNumCascades; ++i)
        {
            char samplerName[64] = { };
            char projectionMatrixName[64] = { };
            char nearClippingPlaneName[64] = { };
            char farClippingPlaneName[64] = { };

            sprintf_s(samplerName, "csm.cascades[%d].shadowmap", i);
            sprintf_s(projectionMatrixName, "csm.cascades[%d].lightProjection", i);
            sprintf_s(nearClippingPlaneName, "csm.cascades[%d].n", i);
            sprintf_s(farClippingPlaneName, "csm.cascades[%d].f", i);

            shader->setUniform(nearClippingPlaneName, cascades[i].n);
            shader->setUniform(farClippingPlaneName, cascades[i].f);
            shader->setUniform(projectionMatrixName, cascades[i].lightProjection);
            shader->setTexture(samplerName, cascades[i].shadowmap);
        }
    }

    void CascadedShadowmap::updateCascades(const Camera& camera)
    {
        // calculate cascade's near and far clipping plane
        f32 t[4] = { 0.1f, 0.3f, 0.6f, 1.f };
        cascades[0].n = camera.n;
        cascades[0].f = (1.0f - t[0]) * camera.n + t[0] * camera.f;
        for (u32 i = 1u; i < kNumCascades; ++i)
        {
            cascades[i].n = cascades[i-1].f;
            cascades[i].f = (1.f - t[i]) * camera.n + t[i] * camera.f;
        }

        // calculate cascade's aabb
        for (u32 i = 0; i < kNumCascades; ++i)
        {
            calcCascadeAABB(cascades[i], camera);
        }
    }

    void CascadedShadowmap::calcCascadeAABB(Cascade& cascade, const Camera& camera)
    {
        glm::mat4 lightView = glm::lookAt(glm::vec3(0.f), -lightDirection, glm::vec3(0.f, 1.f, 0.f));

        f32 N = fabs(cascade.n);
        f32 F = fabs(cascade.f);

        static f32 fixedProjRadius = 0.f;
        // Note(Min): scale in z axis to enclose a bigger range in z, since occluder in another 
        // frusta might cast shadow on current frusta!!
        // stabilize the projection matrix in xy-plane
        if ((F-N) > 2.f * F * glm::tan(glm::radians(camera.fov)) * camera.aspectRatio)
        {
           fixedProjRadius = 0.5f * (F-N) * 1.2f;
        }
        else
        {
           fixedProjRadius = F * glm::tan(glm::radians(camera.fov)) * camera.aspectRatio * 1.2f;
        }

        f32 dn = N * glm::tan(glm::radians(camera.fov));
        // compute lower left corner of near clipping plane
        glm::vec3 cp = glm::normalize(camera.lookAt - camera.position) * N;
        glm::vec3 ca = cp + -camera.up * dn + -camera.right * dn * camera.aspectRatio;

        glm::vec3 na = camera.position + ca;
        glm::vec3 nb = na + camera.right * 2.f * dn * camera.aspectRatio;
        glm::vec3 nc = nb + camera.up * 2.f * dn;
        glm::vec3 nd = na + camera.up * 2.f * dn;

        glm::vec4 nav4 = lightView * glm::vec4(na, 1.f);
        glm::vec4 nbv4 = lightView * glm::vec4(nb, 1.f);
        glm::vec4 ncv4 = lightView * glm::vec4(nc, 1.f);
        glm::vec4 ndv4 = lightView * glm::vec4(nd, 1.f);

        f32 df = F * glm::tan(glm::radians(camera.fov));
        glm::vec3 cpf = glm::normalize(camera.lookAt - camera.position) * F;
        glm::vec3 caf = cpf + -camera.up * df + -camera.right * df  * camera.aspectRatio;

        glm::vec3 fa = camera.position + caf;
        glm::vec3 fb = fa + camera.right * 2.f * df * camera.aspectRatio;
        glm::vec3 fc = fb + camera.up * 2.f * df;
        glm::vec3 fd = fa + camera.up * 2.f * df;
        glm::vec4 fav4 = lightView * glm::vec4(fa, 1.f);
        glm::vec4 fbv4 = lightView * glm::vec4(fb, 1.f);
        glm::vec4 fcv4 = lightView * glm::vec4(fc, 1.f);
        glm::vec4 fdv4 = lightView * glm::vec4(fd, 1.f);

        BoundingBox3D& aabb = cascade.aabb;
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
        // aabb.update();
    }
}
