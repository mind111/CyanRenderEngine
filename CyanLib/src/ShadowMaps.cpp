#include "ShadowMaps.h"
#include "Lights.h"
#include "Camera.h"
#include "Scene.h"
#include "CyanRenderer.h"

namespace Cyan
{
    CascadedShadowMap::CascadedShadowMap(DirectionalLight* directionalLight)
        : m_directionalLight(directionalLight)
    {
        assert(directionalLight != nullptr);
        m_lightDirection = m_directionalLight->m_direction;

        // initialize cascades
        for (i32 i = 0; i < kNumCascades; ++i)
        {
            m_cascades[i].n = cascadeBoundries[i];
            m_cascades[i].f = cascadeBoundries[i + 1];

            // setup light view camera
            m_cascades[i].camera = std::make_unique<OrthographicCamera>();
            m_cascades[i].camera->m_position = glm::vec3(0.f);
            glm::vec3 worldUp = m_cascades[i].camera->m_worldUp = glm::vec3(0.f, 1.f, 0.f);
            glm::vec3 forward = m_cascades[i].camera->m_forward = glm::normalize(-m_lightDirection);
            glm::vec3 right = m_cascades[i].camera->m_right = glm::cross(forward, worldUp);
            m_cascades[i].camera->m_up = glm::cross(right, forward);
            m_cascades[i].camera->m_renderResolution = m_resolution;
            m_cascades[i].camera->m_viewMode = VM_SCENE_DEPTH;

            GfxDepthTexture2D::Spec spec(m_resolution.x, m_resolution.y, 1);
            m_cascades[i].depthTexture = std::unique_ptr<GfxDepthTexture2D>(GfxDepthTexture2D::create(spec));
        }
    }

    CascadedShadowMap::~CascadedShadowMap()
    {

    }

    // helper function for converting world space AABB to light's view space
    static BoundingBox3D calcLightSpaceAABB(const glm::vec3& lightDirection, const BoundingBox3D& worldSpaceAABB)
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

        glm::mat4 lightSpaceView = glm::lookAt(glm::vec3(0.f), -lightDirection, glm::vec3(0.f, 1.f, 0.f));
        BoundingBox3D lightViewSpaceAABB = { };
        lightViewSpaceAABB.bound(lightSpaceView * glm::vec4(fa, 1.f));
        lightViewSpaceAABB.bound(lightSpaceView * glm::vec4(fb, 1.f));
        lightViewSpaceAABB.bound(lightSpaceView * glm::vec4(fc, 1.f));
        lightViewSpaceAABB.bound(lightSpaceView * glm::vec4(fd, 1.f));
        lightViewSpaceAABB.bound(lightSpaceView * glm::vec4(ba, 1.f));
        lightViewSpaceAABB.bound(lightSpaceView * glm::vec4(bb, 1.f));
        lightViewSpaceAABB.bound(lightSpaceView * glm::vec4(bc, 1.f));
        lightViewSpaceAABB.bound(lightSpaceView * glm::vec4(bd, 1.f));
        return lightViewSpaceAABB;
    }

    void CascadedShadowMap::update(PerspectiveCamera* camera)
    {
        m_lightViewMatrix = glm::lookAt(glm::vec3(0.f), -normalize(m_lightDirection), glm::vec3(0.f, 1.f, 0.f));

        // calculate cascade's near and far clipping plane
        for (u32 i = 0u; i < kNumCascades; ++i) 
        {
            m_cascades[i].n = camera->n + cascadeBoundries[i] * (camera->f - camera->n);
            m_cascades[i].f = camera->n + cascadeBoundries[i + 1] * (camera->f - camera->n);
        }

        // calculate cascade's world space aabb
        for (u32 i = 0; i < kNumCascades; ++i) 
        {
            Cascade& cascade = m_cascades[i];

            f32 N = fabs(cascade.n);
            f32 F = fabs(cascade.f);
            static f32 fixedProjRadius = 0.f;
            glm::vec3 right = camera->right();
            glm::vec3 up = camera->up();

            /** note
                scale in z axis to enclose a bigger range in z, since occluder in another
                frusta might cast shadow on current frusta!! stabilize the projection matrix in xy-plane
            */
            if ((F - N) > 2.f * F * glm::tan(glm::radians(camera->fov)) * camera->aspectRatio)
            {
                fixedProjRadius = 0.5f * (F - N) * 1.2f;
            }
            else
            {
                fixedProjRadius = F * glm::tan(glm::radians(camera->fov)) * camera->aspectRatio * 1.2f;
            }

            f32 dn = N * glm::tan(glm::radians(camera->fov));
            // compute lower left corner of near clipping plane
            glm::vec3 cp = camera->forward() * N;
            glm::vec3 ca = cp + -up * dn + -right * dn * camera->aspectRatio;

            glm::vec3 na = camera->aspectRatio + ca;
            glm::vec3 nb = na + right * 2.f * dn * camera->aspectRatio;
            glm::vec3 nc = nb + up * 2.f * dn;
            glm::vec3 nd = na + up * 2.f * dn;

            f32 df = F * glm::tan(glm::radians(camera->fov));
            glm::vec3 cpf = camera->forward() * F;
            glm::vec3 caf = cpf + -up * df + -right * df * camera->aspectRatio;

            glm::vec3 fa = camera->m_position + caf;
            glm::vec3 fb = fa + right * 2.f * df * camera->aspectRatio;
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
            cascade.camera->m_viewVolume = calcLightSpaceAABB(m_lightDirection, cascade.worldSpaceAABB);
        }
    }

    void CascadedShadowMap::render(Scene* scene, Camera* camera)
    {
        if (auto perspectiveCamera = dynamic_cast<PerspectiveCamera*>(camera))
        {
            update(perspectiveCamera);

            auto renderer = Renderer::get();
            // calculate cascade's world space aabb
            for (u32 i = 0; i < kNumCascades; ++i)
            {
                char markerName[64];
                sprintf_s(markerName, "CSM Cascade[%u]", i);
                GPU_DEBUG_SCOPE(csmMarker, markerName);

                SceneRender::ViewParameters viewParameters(scene, m_cascades[i].camera.get());
                renderer->renderSceneDepth(m_cascades[i].depthTexture.get(), scene, viewParameters);
            }
        }
        else
        {
            cyanError("CascadedShadowMap only works for perspective camera for now!");
            assert(0);
        }
    }
}
