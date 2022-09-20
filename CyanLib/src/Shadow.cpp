#include "Shadow.h"
#include "Scene.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
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

    DirectionalShadowmap::DirectionalShadowmap(const DirectionalLight& inDirectionalLight, const char* depthTextureNamePrefix)
        : IDirectionalShadowmap(inDirectionalLight)
    {
        char depthTextureBuffer[64] = { };
        sprintf_s(depthTextureBuffer, "DirectionalShadowmapTexture_%dx%d", resolution.x, resolution.y);
        std::string depthTextureName(depthTextureBuffer);
        if (depthTextureNamePrefix)
        {
            depthTextureName = std::string(depthTextureNamePrefix) + depthTextureName;
        }
        depthTexture = AssetManager::createDepthTexture(depthTextureName.c_str(), resolution.x, resolution.y);
    }

    void DirectionalShadowmap::render(ICamera* inCamera, const Scene& scene, RenderableScene& renderableScene, Renderer& renderer)
    {
        renderableScene.camera.view = inCamera->view();
        renderableScene.camera.projection = inCamera->projection();

        lightSpaceProjection = inCamera->projection();

        // glDisable(GL_CULL_FACE);
        renderer.renderSceneDepthOnly(renderableScene, depthTexture);
        // glEnable(GL_CULL_FACE);
    }

    void DirectionalShadowmap::setShaderParameters(Shader* shader, const char* uniformNamePrefix)
    {
        std::string inPrefix(uniformNamePrefix);
        shader->setUniform((inPrefix + ".shadowmap.lightSpaceProjection").c_str(), lightSpaceProjection);
        GLboolean isResident = GL_FALSE;
        GLuint textures[1] = { depthTexture->getGpuResource() };
#if BINDLESS_TEXTURE
        if (glIsTextureHandleResidentARB(depthTexture->glHandle) == GL_FALSE)
        {
            glMakeTextureHandleResidentARB(depthTexture->glHandle);
        }
#endif
        shader->setUniform((inPrefix + ".shadowmap.depthTextureHandle").c_str(), depthTexture->glHandle);
    }

    VarianceShadowmap::VarianceShadowmap(const DirectionalLight& inDirectionalLight)
        : IDirectionalShadowmap(inDirectionalLight)
    {
        // create depth texture
        {
            char depthTextureName[64] = { };
            sprintf_s(depthTextureName, "VarianceShadowmapDepthTexture_%dx%d", resolution.x, resolution.y);
            depthTexture = AssetManager::createDepthTexture(depthTextureName, resolution.x, resolution.y);
        }

        // create depth squared texture
        {
            char depthSquaredTextureName[64] = { };
            sprintf_s(depthSquaredTextureName, "VarianceShadowmapDepthSquaredTexture_%dx%d", resolution.x, resolution.y);
            depthSquaredTexture = AssetManager::createDepthTexture(depthSquaredTextureName, resolution.x, resolution.y);
        }
    }

    CascadedShadowmap::CascadedShadowmap(const DirectionalLight& inDirectionalLight)
        : IDirectionalShadowmap(inDirectionalLight)
    {
        // initialize cascades
        for (u32 i = 0; i < kNumCascades; ++i)
        {
            char shadowmapNameBuffer[64] = { };
            sprintf_s(shadowmapNameBuffer, "CSM_%d_", i);
            cascades[i].n = cascadeBoundries[i];
            cascades[i].f = cascadeBoundries[i + 1];
            switch (inDirectionalLight.implemenation)
            {
            case DirectionalLight::Implementation::kCSM_Basic:
                cascades[i].shadowmapPtr = std::make_unique<DirectionalShadowmap>(inDirectionalLight, shadowmapNameBuffer);
                break;
            case DirectionalLight::Implementation::kCSM_VarianceShadowmap:
                cascades[i].shadowmapPtr = std::make_unique<VarianceShadowmap>(inDirectionalLight);
                break;
            }
        }
    }

    void CascadedShadowmap::render(ICamera* inCamera, const Scene& scene, RenderableScene& renderableScene, Renderer& renderer)
    {
        if (auto viewCamera = dynamic_cast<PerspectiveCamera*>(scene.camera->getCamera()))
        {
            // calculate cascades based on camera view frustum
            updateCascades(*viewCamera);

            for (u32 i = 0; i < kNumCascades; ++i)
            {
                BoundingBox3D lightSpaceAABB = calcLightSpaceAABB(lightDirection, cascades[i].worldSpaceAABB);
                OrthographicCamera camera(
                    glm::vec3(0.f),
                    -lightDirection,
                    glm::vec3(0.f, 1.f, 0.f),
                    lightSpaceAABB
                );
                cascades[i].shadowmapPtr->render(&camera, scene, renderableScene, renderer);
            }
        }
    }

    void CascadedShadowmap::setShaderParameters(Shader* shader, const char* uniformNamePrefix)
    {
        std::string inPrefix(uniformNamePrefix);
        for (u32 i = 0; i < kNumCascades; ++i)
        {
            char cascadePrefixBuffer[64] = { };
            sprintf_s(cascadePrefixBuffer, ".csm.cascades[%d]", i);
            std::string cascadePrefixStr(cascadePrefixBuffer);
            std::string nearClippingPlaneName = inPrefix + cascadePrefixStr + std::string(".n");
            std::string farClippingPlaneName = inPrefix + cascadePrefixStr + std::string(".f");
            shader->setUniform(nearClippingPlaneName.c_str(), cascades[i].n);
            shader->setUniform(farClippingPlaneName.c_str(), cascades[i].f);

            std::string csmPrefix = inPrefix + cascadePrefixStr;
            cascades[i].shadowmapPtr->setShaderParameters(shader, csmPrefix.c_str());
        }
    }

    void CascadedShadowmap::updateCascades(PerspectiveCamera& camera)
    {
        // calculate cascade's near and far clipping plane
        for (u32 i = 0u; i < kNumCascades; ++i)
        {
            cascades[i].n = camera.n + cascadeBoundries[i] * (camera.f - camera.n);
            cascades[i].f = camera.n + cascadeBoundries[i + 1] * (camera.f - camera.n);
        }

        // calculate cascade's aabb
        for (u32 i = 0; i < kNumCascades; ++i)
        {
            calcCascadeAABB(cascades[i], camera);
        }
    }

    void CascadedShadowmap::calcCascadeAABB(Cascade& cascade, PerspectiveCamera& camera)
    {
        f32 N = fabs(cascade.n);
        f32 F = fabs(cascade.f);
        static f32 fixedProjRadius = 0.f;
        glm::vec3 right = camera.right();
        glm::vec3 up = camera.up();

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
        glm::vec3 ca = cp + -up * dn + -right * dn * camera.aspectRatio;

        glm::vec3 na = camera.position + ca;
        glm::vec3 nb = na + right * 2.f * dn * camera.aspectRatio;
        glm::vec3 nc = nb + up * 2.f * dn;
        glm::vec3 nd = na + up * 2.f * dn;

        f32 df = F * glm::tan(glm::radians(camera.fov));
        glm::vec3 cpf = glm::normalize(camera.lookAt - camera.position) * F;
        glm::vec3 caf = cpf + -up * df + -right * df * camera.aspectRatio;

        glm::vec3 fa = camera.position + caf;
        glm::vec3 fb = fa + right * 2.f * df * camera.aspectRatio;
        glm::vec3 fc = fb + up * 2.f * df;
        glm::vec3 fd = fa + up * 2.f * df;

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
