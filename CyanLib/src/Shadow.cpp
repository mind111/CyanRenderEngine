#include "Shadow.h"
#include "Scene.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
#include "Lights.h"
#include "AssetManager.h"

namespace Cyan
{
#if 0
    DirectionalShadowMap::DirectionalShadowMap(const DirectionalLight& inDirectionalLight)
        : lightDirection(inDirectionalLight.direction), resolution(4096, 4096)
    {
        // initialize cascades
        for (i32 i = 0; i < kNumCascades; ++i)
        {
            cascades[i].n = cascadeBoundries[i];
            cascades[i].f = cascadeBoundries[i + 1];
            GfxDepthTexture2D::Spec spec(resolution.x, resolution.y, 1);
            cascades[i].depthTexture = std::unique_ptr<GfxDepthTexture2D>(GfxDepthTexture2D::create(spec));
        }
    }

    void DirectionalShadowMap::render(Scene* inScene, Renderer* renderer) 
    {
        PerspectiveCamera* camera = dynamic_cast<PerspectiveCamera*>(inScene->m_mainCamera->getCamera());
        assert(camera);

        updateCascades(*camera);

        for (i32 i = 0; i < kNumCascades; ++i)
        {
            BoundingBox3D cascadeLightSpaceAABB = calcLightSpaceAABB(lightDirection, cascades[i].worldSpaceAABB);

            OrthographicCamera lightCamera(
                glm::vec3(0.f),
                -lightDirection,
                glm::vec3(0.f, 1.f, 0.f),
                cascadeLightSpaceAABB
            );

            cascades[i].lightSpaceProjection = lightCamera.projection();

            SceneView sceneView(inScene, &lightCamera, cascades[i].depthTexture.get());
            RenderableScene scene(inScene, sceneView);

            renderer->renderSceneDepthOnly(scene, cascades[i].depthTexture.get());
        }
    }

    void DirectionalShadowMap::updateCascades(const PerspectiveCamera& camera) 
    {
        // calculate cascade's near and far clipping plane
        for (u32 i = 0u; i < kNumCascades; ++i) 
        {
            cascades[i].n = camera.n + cascadeBoundries[i] * (camera.f - camera.n);
            cascades[i].f = camera.n + cascadeBoundries[i + 1] * (camera.f - camera.n);
        }

        // calculate cascade's world space aabb
        for (u32 i = 0; i < kNumCascades; ++i) 
        {
            Cascade& cascade = cascades[i];

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
            glm::vec3 cp = glm::normalize(camera.m_lookAt - camera.m_position) * N;
            glm::vec3 ca = cp + -up * dn + -right * dn * camera.aspectRatio;

            glm::vec3 na = camera.aspectRatio + ca;
            glm::vec3 nb = na + right * 2.f * dn * camera.aspectRatio;
            glm::vec3 nc = nb + up * 2.f * dn;
            glm::vec3 nd = na + up * 2.f * dn;

            f32 df = F * glm::tan(glm::radians(camera.fov));
            glm::vec3 cpf = glm::normalize(camera.m_lookAt - camera.m_position) * F;
            glm::vec3 caf = cpf + -up * df + -right * df * camera.aspectRatio;

            glm::vec3 fa = camera.m_position + caf;
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

    void DirectionalShadowMap::setShaderParameters(PixelShader* ps)
    {
        glm::mat4 lightSpaceView = glm::m_lookAt(glm::vec3(0.f), -lightDirection, glm::vec3(0.f, 1.f, 0.f));
        p->setUniform("directionalLight.shadowMap.lightSpaceView", lightSpaceView);
        // 0
        p->setUniform("directionalLight.shadowMap.cascades[0].n", cascades[0].n);
        p->setUniform("directionalLight.shadowMap.cascades[0].f", cascades[0].f);
        p->setUniform("directionalLight.shadowMap.cascades[0].lightSpaceProjection", cascades[0].lightSpaceProjection);
        p->setTexture("directionalLight.shadowMap.cascades[0].depthTexture", cascades[0].depthTexture.get());
        // 1
        p->setUniform("directionalLight.shadowMap.cascades[1].n", cascades[1].n);
        p->setUniform("directionalLight.shadowMap.cascades[1].f", cascades[1].f);
        p->setUniform("directionalLight.shadowMap.cascades[1].lightSpaceProjection", cascades[1].lightSpaceProjection);
        p->setTexture("directionalLight.shadowMap.cascades[1].depthTexture", cascades[1].depthTexture.get());
        // 2
        p->setUniform("directionalLight.shadowMap.cascades[2].n", cascades[2].n);
        p->setUniform("directionalLight.shadowMap.cascades[2].f", cascades[2].f);
        p->setUniform("directionalLight.shadowMap.cascades[2].lightSpaceProjection", cascades[2].lightSpaceProjection);
        p->setTexture("directionalLight.shadowMap.cascades[2].depthTexture", cascades[2].depthTexture.get());
        // 3
        p->setUniform("directionalLight.shadowMap.cascades[3].n", cascades[3].n);
        p->setUniform("directionalLight.shadowMap.cascades[3].f", cascades[3].f);
        p->setUniform("directionalLight.shadowMap.cascades[3].lightSpaceProjection", cascades[3].lightSpaceProjection);
        p->setTexture("directionalLight.shadowMap.cascades[3].depthTexture", cascades[3].depthTexture.get());
    }
#endif
}
