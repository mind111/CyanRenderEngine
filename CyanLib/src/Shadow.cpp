#include "Shadow.h"
#include "Scene.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
#include "RenderableScene.h"
#include "DirectionalLight.h"
#include "AssetManager.h"

namespace Cyan
{
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
            cascades[i].depthTexturePtr = std::unique_ptr<DepthTexture>(AssetManager::createDepthTexture(depthTextureName, width, height));
        }
    }

    // todo: think about how to efficiently create/destroy transient gpu resources such as texture and render target
    void CascadedShadowmap::render(const Scene& scene, Renderer& renderer)
    {
        updateCascades(scene.camera);

        // create render target
        std::unique_ptr<RenderTarget> depthRenderTargetPtr(createDepthOnlyRenderTarget(cascades[0].depthTexturePtr->width, cascades[0].depthTexturePtr->height));
        for (u32 i = 0; i < kNumCascades; ++i)
        {
            depthRenderTargetPtr->setDepthBuffer(cascades[i].depthTexturePtr.get());
            depthRenderTargetPtr->clear({ { 0u } });

            // todo: this is just a temporary hack camera should be orthographic
            Camera camera = { };
            camera.view = glm::lookAt(glm::vec3(0.f), -lightDirection, glm::vec3(0.f, 1.f, 0.f));
            camera.projection = glm::orthoLH(cascades[i].aabb.pmin.x, cascades[i].aabb.pmax.x, cascades[i].aabb.pmin.y, cascades[i].aabb.pmax.y, cascades[i].aabb.pmax.z, cascades[i].aabb.pmin.z);
            cascades[i].lightProjection = camera.projection;
            SceneView sceneView(scene, camera, depthRenderTargetPtr.get(), { { 0u } }, { 0u, 0u, depthRenderTargetPtr->width, depthRenderTargetPtr->height }, EntityFlag_kVisible | EntityFlag_kCastShadow);
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
            shader->setTexture(samplerName, cascades[i].depthTexturePtr.get());
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
        
        /** note
            scale in z axis to enclose a bigger range in z, since occluder in another 
            frusta might cast shadow on current frusta!! stabilize the projection matrix in xy-plane
        */
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
    }
}
