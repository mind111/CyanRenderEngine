#include "Shadow.h"
#include "Scene.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
#include "RenderableScene.h"
#include "DirectionalLight.h"
#include "AssetManager.h"

namespace Cyan
{
    IDirectionalShadowmap::IDirectionalShadowmap(const DirectionalLight& inDirectionalLight)
        : quality(Quality::kHigh), lightDirection(inDirectionalLight.direction)
    {
        switch (inDirectionalLight.shadowQuality)
        {
        case DirectionalLight::ShadowQuality::kLow:
        case DirectionalLight::ShadowQuality::kMedium:
        case DirectionalLight::ShadowQuality::kHigh:
        {
            resolution.x = 4096;
            resolution.y = 4096;
        } break;
        default:
            break;
        }
    }

    BoundingBox3D IDirectionalShadowmap::calcLightSpaceAABB(const glm::vec3& inLightDirection, const BoundingBox3D& worldSpaceAABB)
    {
        // derive the 8 vertices of the aabb from pmin and pmax
        // d -- c
        // |    |
        // a -- b
        // f stands for front while b stands for back
        f32 width = worldSpaceAABB.pmax.x - worldSpaceAABB.pmin.x;
        f32 height = worldSpaceAABB.pmax.y - worldSpaceAABB.pmin.y;
        f32 depth = worldSpaceAABB.pmax.z - worldSpaceAABB.pmin.z;
        glm::vec3 fa = vec4ToVec3(worldSpaceAABB.pmax) + glm::vec3(-width, -height, 0.f);
        glm::vec3 fb = vec4ToVec3(worldSpaceAABB.pmax) + glm::vec3(0.f, -height, 0.f);
        glm::vec3 fc = vec4ToVec3(worldSpaceAABB.pmax);
        glm::vec3 fd = vec4ToVec3(worldSpaceAABB.pmax) + glm::vec3(-width, 0.f, 0.f);

        glm::vec3 ba = fa + glm::vec3(0.f, 0.f, -depth);
        glm::vec3 bb = fb + glm::vec3(0.f, 0.f, -depth);
        glm::vec3 bc = fc + glm::vec3(0.f, 0.f, -depth);
        glm::vec3 bd = fd + glm::vec3(0.f, 0.f, -depth);

        glm::mat4 lightSpaceView = glm::lookAt(glm::vec3(0.f), -inLightDirection, glm::vec3(0.f, 1.f, 0.f));
        BoundingBox3D lightSpaceAABB = { };
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fa, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fb, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fc, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fd, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(ba, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(bb, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(bc, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(bd, 1.f));
        return lightSpaceAABB;
    }

    DirectionalShadowmap::DirectionalShadowmap(const DirectionalLight& inDirectionalLight)
        : IDirectionalShadowmap(inDirectionalLight)
    {
        char depthTextureName[64] = { };
        sprintf_s(depthTextureName, "DirectionalShadowmapTexture_%dx%d", resolution.x, resolution.y);
        depthTexturePtr = std::unique_ptr<DepthTexture>(AssetManager::createDepthTexture(depthTextureName, resolution.x, resolution.y));
    }

    void DirectionalShadowmap::render(const Scene& scene, const BoundingBox3D& worldSpaceAABB, Renderer& renderer)
    {
        // create render target
        std::unique_ptr<RenderTarget> depthRenderTargetPtr(createDepthOnlyRenderTarget(depthTexturePtr->width, depthTexturePtr->height));
        depthRenderTargetPtr->setDepthBuffer(depthTexturePtr.get());
        depthRenderTargetPtr->clear({ { 0u } });

        Camera camera = { };
        camera.view = glm::lookAt(glm::vec3(0.f), -lightDirection, glm::vec3(0.f, 1.f, 0.f));
        // convert world space aabb to light space
        BoundingBox3D lightSpaceAABB = calcLightSpaceAABB(lightDirection, worldSpaceAABB);
        lightSpaceProjection = glm::orthoLH(lightSpaceAABB.pmin.x, lightSpaceAABB.pmax.x, lightSpaceAABB.pmin.y, lightSpaceAABB.pmax.y, lightSpaceAABB.pmax.z, lightSpaceAABB.pmin.z);;
        camera.projection = lightSpaceProjection;

        SceneView sceneView(scene, camera, depthRenderTargetPtr.get(), { { 0u } }, { 0u, 0u, depthRenderTargetPtr->width, depthRenderTargetPtr->height }, EntityFlag_kVisible | EntityFlag_kCastShadow);
        SceneRenderable renderableScene(&scene, sceneView, renderer.getFrameAllocator());

        renderer.renderSceneDepthOnly(renderableScene, sceneView);
    }

    void DirectionalShadowmap::setShaderParameters(Shader* shader)
    {

    }

    VarianceShadowmap::VarianceShadowmap(const DirectionalLight& inDirectionalLight)
        : IDirectionalShadowmap(inDirectionalLight)
    {
        // create depth texture
        {
            char depthTextureName[64] = { };
            sprintf_s(depthTextureName, "VarianceShadowmapDepthTexture_%dx%d", resolution.x, resolution.y);
            depthTexturePtr = std::unique_ptr<DepthTexture>(AssetManager::createDepthTexture(depthTextureName, resolution.x, resolution.y));
        }

        // create depth squared texture
        {
            char depthSquaredTextureName[64] = { };
            sprintf_s(depthSquaredTextureName, "VarianceShadowmapDepthSquaredTexture_%dx%d", resolution.x, resolution.y);
            depthSquaredTexturePtr = std::unique_ptr<DepthTexture>(AssetManager::createDepthTexture(depthSquaredTextureName, resolution.x, resolution.y));
        }
    }

    CascadedShadowmap::CascadedShadowmap(const DirectionalLight& inDirectionalLight)
        : IDirectionalShadowmap(inDirectionalLight)
    {
        // initialize cascades' shadowmap
        for (u32 i = 0; i < kNumCascades; ++i)
        {
            cascades[i].shadowmapPtr = std::make_unique<DirectionalShadowmap>(inDirectionalLight);
        }
    }

    // todo: think about how to efficiently create/destroy transient gpu resources such as texture and render target
    void CascadedShadowmap::render(const Scene& scene, const BoundingBox3D& aabb, Renderer& renderer)
    {
        updateCascades(scene.camera);
        for (u32 i = 0; i < kNumCascades; ++i)
        {
            cascades[i].shadowmapPtr->render(scene, cascades[i].worldSpaceAABB, renderer);
        }
    }

    void CascadedShadowmap::setShaderParameters(Shader* shader)
    {
#if 0
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
            shader->setUniform(projectionMatrixName, cascades[i].shadowmapPtr->getLightProjection());
            shader->setTexture(samplerName, cascades[i].shadowmapPtr->getDepth);
        }
#endif
    }

    void CascadedShadowmap::updateCascades(const Camera& camera)
    {
        // calculate cascade's near and far clipping plane
        f32 t[4] = { 0.1f, 0.3f, 0.6f, 1.f };
        cascades[0].n = camera.n;
        cascades[0].f = (1.0f - t[0]) * camera.n + t[0] * camera.f;
        for (u32 i = 1u; i < kNumCascades; ++i)
        {
            cascades[i].n = cascades[i - 1].f;
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
        f32 N = fabs(cascade.n);
        f32 F = fabs(cascade.f);

        static f32 fixedProjRadius = 0.f;

        /** note
            scale in z axis to enclose a bigger range in z, since occluder in another
            frusta might cast shadow on current frusta!! stabilize the projection matrix in xy-plane
        */
        if ((F - N) > 2.f * F * glm::tan(glm::radians(camera.fov)) * camera.aspectRatio)
        {
            fixedProjRadius = 0.5f * (F - N) * 1.2f;
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

        f32 df = F * glm::tan(glm::radians(camera.fov));
        glm::vec3 cpf = glm::normalize(camera.lookAt - camera.position) * F;
        glm::vec3 caf = cpf + -camera.up * df + -camera.right * df * camera.aspectRatio;

        glm::vec3 fa = camera.position + caf;
        glm::vec3 fb = fa + camera.right * 2.f * df * camera.aspectRatio;
        glm::vec3 fc = fb + camera.up * 2.f * df;
        glm::vec3 fd = fa + camera.up * 2.f * df;

        BoundingBox3D aabb = { };
        aabb.bound(na);
        aabb.bound(nb);
        aabb.bound(nc);
        aabb.bound(nd);
        aabb.bound(fa);
        aabb.bound(fb);
        aabb.bound(fc);
        aabb.bound(fd);

        // TODO: improve this procedure for fixing the projection size 
        f32 midX = .5f * (aabb.pmin.x + aabb.pmax.x);
        f32 midY = .5f * (aabb.pmin.y + aabb.pmax.y);
        glm::vec3 mid = 0.5f * (aabb.pmin + aabb.pmax);

        // snap to texel increments
        mid = glm::floor(mid);
        aabb.pmin = glm::vec4(mid - glm::vec3(fixedProjRadius), 1.f);
        aabb.pmax = glm::vec4(mid + glm::vec3(fixedProjRadius), 1.f);

        cascade.worldSpaceAABB = aabb;
    }
}
