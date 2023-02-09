#include "Shadow.h"
#include "Scene.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
#include "Lights.h"
#include "AssetManager.h"

namespace Cyan
{
    u32 IDirectionalShadowMap::numDirectionalShadowMaps = 0;
    IDirectionalShadowMap::IDirectionalShadowMap(const DirectionalLight& inDirectionalLight)
        : lightDirection(inDirectionalLight.direction) 
    {
        resolution.x = 4096;
        resolution.y = 4096;
        numDirectionalShadowMaps++;
    }

    DirectionalShadowMap::DirectionalShadowMap(const DirectionalLight& inDirectionalLight)
        : IDirectionalShadowMap(inDirectionalLight) 
    {
        char depthTextureName[64] = { };
        sprintf_s(depthTextureName, "DirectionalShadowmapTexture_%u", IDirectionalShadowMap::numDirectionalShadowMaps);
        DepthTexture2D::Spec spec(resolution.x, resolution.y, 1);
        depthTexture = std::make_unique<DepthTexture2DBindless>(depthTextureName, spec);
        depthTexture->init();
    }

    void DirectionalShadowMap::render(const BoundingBox3D& lightSpaceAABB, RenderableScene& scene, Renderer* renderer) {
        OrthographicCamera lightCamera(
            glm::vec3(0.f),
            -lightDirection,
            glm::vec3(0.f, 1.f, 0.f),
            lightSpaceAABB
        );

        scene.camera.view = lightCamera.view();
        scene.camera.projection = lightCamera.projection();
        lightSpaceProjection = lightCamera.projection();

        renderer->renderSceneDepthOnly(scene, depthTexture.get());
    }

    void DirectionalShadowMap::setShaderParameters(Shader* shader, const char* uniformNamePrefix)
    {
        std::string inPrefix(uniformNamePrefix);
        shader->setUniform((inPrefix + ".shadowmap.lightSpaceProjection").c_str(), lightSpaceProjection);
#if BINDLESS_TEXTURE
        if (glIsTextureHandleResidentARB(depthTexture->glHandle) == GL_FALSE)
        {
            glMakeTextureHandleResidentARB(depthTexture->glHandle);
        }
#endif
        shader->setUniform((inPrefix + ".shadowmap.depthTextureHandle").c_str(), depthTexture->glHandle);
    }

    VarianceShadowMap::VarianceShadowMap(const DirectionalLight& inDirectionalLight)
        : IDirectionalShadowMap(inDirectionalLight)
    {
        // create depth texture
        {
            char depthTextureName[64] = { };
            sprintf_s(depthTextureName, "VarianceShadowmapDepthTexture_%dx%d", resolution.x, resolution.y);
            DepthTexture2D::Spec spec(resolution.x, resolution.y, 1);
            depthTexture = std::make_unique<DepthTexture2D>(depthTextureName, spec);
            depthTexture->init();
        }

        // create depth squared texture
        {
            char depthSquaredTextureName[64] = { };
            sprintf_s(depthSquaredTextureName, "VarianceShadowmapDepthSquaredTexture_%dx%d", resolution.x, resolution.y);
            DepthTexture2D::Spec spec(resolution.x, resolution.y, 1);
            depthSquaredTexture = std::make_unique<DepthTexture2D>(depthSquaredTextureName, spec);
            depthSquaredTexture->init();
        }
    }

    CascadedShadowMap::CascadedShadowMap(const DirectionalLight& inDirectionalLight)
        : IDirectionalShadowMap(inDirectionalLight)
    {
        // initialize cascades
        for (u32 i = 0; i < kNumCascades; ++i) {
            char shadowMapName[64] = { };
            cascades[i].n = cascadeBoundries[i];
            cascades[i].f = cascadeBoundries[i + 1];
            cascades[i].shadowMap = std::make_unique<DirectionalShadowMap>(inDirectionalLight);
        }
    }

    void CascadedShadowMap::render(const BoundingBox3D& lightSpaceAABB, RenderableScene& scene, Renderer* renderer) {
        // calculate cascades based on camera view frustum
        updateCascades(scene.camera);

        for (u32 i = 0; i < kNumCascades; ++i) {
            BoundingBox3D cascadeLightSpaceAABB = calcLightSpaceAABB(lightDirection, cascades[i].worldSpaceAABB);
            cascades[i].shadowMap->render(cascadeLightSpaceAABB, scene, renderer);
        }
    }

    void CascadedShadowMap::updateCascades(const RenderableScene::Camera& camera) {
        // calculate cascade's near and far clipping plane
        for (u32 i = 0u; i < kNumCascades; ++i) {
            cascades[i].n = camera.n + cascadeBoundries[i] * (camera.f - camera.n);
            cascades[i].f = camera.n + cascadeBoundries[i + 1] * (camera.f - camera.n);
        }

        // calculate cascade's aabb
        for (u32 i = 0; i < kNumCascades; ++i) {
            calcCascadeAABB(cascades[i], camera);
        }
    }

    void CascadedShadowMap::calcCascadeAABB(Cascade& cascade, const RenderableScene::Camera& camera) {
        f32 N = fabs(cascade.n);
        f32 F = fabs(cascade.f);
        static f32 fixedProjRadius = 0.f;
        glm::vec3 right = camera.right;
        glm::vec3 up = camera.up;

        /** note
            scale in z axis to enclose a bigger range in z, since occluder in another
            frusta might cast shadow on current frusta!! stabilize the projection matrix in xy-plane
        */
        if ((F - N) > 2.f * F * glm::tan(glm::radians(camera.fov)) * camera.aspect)
        {
            fixedProjRadius = 0.5f * (F - N) * 1.2f;
        }
        else
        {
            fixedProjRadius = F * glm::tan(glm::radians(camera.fov)) * camera.aspect * 1.2f;
        }

        f32 dn = N * glm::tan(glm::radians(camera.fov));
        // compute lower left corner of near clipping plane
        glm::vec3 cp = glm::normalize(camera.lookAt - camera.eye) * N;
        glm::vec3 ca = cp + -up * dn + -right * dn * camera.aspect;

        glm::vec3 na = camera.eye + ca;
        glm::vec3 nb = na + right * 2.f * dn * camera.aspect;
        glm::vec3 nc = nb + up * 2.f * dn;
        glm::vec3 nd = na + up * 2.f * dn;

        f32 df = F * glm::tan(glm::radians(camera.fov));
        glm::vec3 cpf = glm::normalize(camera.lookAt - camera.eye) * F;
        glm::vec3 caf = cpf + -up * df + -right * df * camera.aspect;

        glm::vec3 fa = camera.eye + caf;
        glm::vec3 fb = fa + right * 2.f * df * camera.aspect;
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
