#include "ShadowMaps.h"
#include "Scene.h"
#include "SceneRenderer.h"
#include "RenderPass.h"
#include "Shader.h"
#include "MathLibrary.h"

namespace Cyan
{
    static void calcPerspectiveViewFrustum(const CameraViewInfo& camera, f32 n, f32 f, glm::vec3& na, glm::vec3& nb, glm::vec3& nc, glm::vec3& nd, glm::vec3& fa, glm::vec3& fb, glm::vec3& fc, glm::vec3& fd)
    {
        if (camera.m_projectionMode == CameraViewInfo::ProjectionMode::kPerspective)
        {
            glm::vec3 nCenter = camera.m_worldSpacePosition + camera.worldSpaceForward() * n;
            glm::vec3 fCenter = camera.m_worldSpacePosition + camera.worldSpaceForward() * f;

            glm::vec3 x = camera.worldSpaceRight();
            glm::vec3 y = camera.worldSpaceUp();

            f32 fov = camera.m_perspective.fov;
            f32 aspectRatio = camera.m_perspective.aspectRatio;

            // clock-wise
            // d --- c
            // |     |
            // a --- b
            glm::vec2 nExtent(0.f);
            nExtent.x = aspectRatio * n * glm::tan(glm::radians(fov * .5f));
            nExtent.y = n * glm::tan(glm::radians(fov * .5f));

            na = nCenter + -x * nExtent.x + -y * nExtent.y;
            nb = nCenter +  x * nExtent.x + -y * nExtent.y;
            nc = nCenter +  x * nExtent.x +  y * nExtent.y;
            nd = nCenter + -x * nExtent.x +  y * nExtent.y;

            glm::vec2 fExtent(0.f);
            fExtent.x = aspectRatio * f * glm::tan(glm::radians(fov * .5f));
            fExtent.y = f * glm::tan(glm::radians(fov * .5f));

            fa = fCenter + -x * fExtent.x + -y * fExtent.y;
            fb = fCenter +  x * fExtent.x + -y * fExtent.y;
            fc = fCenter +  x * fExtent.x +  y * fExtent.y;
            fd = fCenter + -x * fExtent.x +  y * fExtent.y;
        }
    }

    CascadedShadowMap::CascadedShadowMap()
    {
        // initialize cascades
        for (i32 i = 0; i < kNumCascades; ++i)
        {
            GHDepthTexture::Desc desc = GHDepthTexture::Desc::create(m_resolution.x, m_resolution.y, 1);
            m_cascades[i].depthTexture = std::move(GHDepthTexture::create(desc));
        }
    }

    CascadedShadowMap::~CascadedShadowMap()
    {

    }

    // helper function for converting world space AABB to light's view space
    static AxisAlignedBoundingBox3D calcLightSpaceAABB(const glm::vec3& lightDirection, const AxisAlignedBoundingBox3D& worldSpaceAABB)
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
        AxisAlignedBoundingBox3D lightViewSpaceAABB = { };
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

    void CascadedShadowMap::render(Scene* scene, DirectionalLight* light, const CameraViewInfo& cameraInfo)
    {
        if (light == nullptr)
        {
            return;
        }

        if (cameraInfo.m_projectionMode == CameraViewInfo::ProjectionMode::kPerspective)
        {
            m_lightDirection = light->m_direction;

            // calculate cascade's near and far clipping plane
            for (u32 i = 0u; i < kNumCascades; ++i) 
            {
                m_cascades[i].n = cameraInfo.m_perspective.n + cascadeBoundries[i] * (cameraInfo.m_perspective.f - cameraInfo.m_perspective.n);
                m_cascades[i].f = cameraInfo.m_perspective.n + cascadeBoundries[i + 1] * (cameraInfo.m_perspective.f - cameraInfo.m_perspective.n);

                // setup light view camera
                CameraViewInfo& camera = m_cascades[i].lightCamera;
                camera.setProjectionMode(CameraViewInfo::ProjectionMode::kOrthographic);

                // todo: worldUp needs to be careful of being in the same direction as camera's worldSpaceUp
                glm::vec3 worldUp = CameraViewInfo::worldUpVector();
                camera.m_worldSpacePosition = glm::vec3(0.f);
                camera.m_worldUp = worldUp;
                glm::vec3 forward = camera.m_worldSpaceForward = glm::normalize(-m_lightDirection);
                glm::vec3 right = camera.m_worldSpaceRight = glm::cross(forward, worldUp);
                camera.m_worldSpaceUp = glm::cross(right, forward);
                m_lightViewMatrix = glm::lookAt(glm::vec3(0.f), -normalize(m_lightDirection), worldUp);
            }

            for (u32 i = 0; i < kNumCascades; ++i)
            {
                Cascade& cascade = m_cascades[i];

                glm::vec3 cameraWorldSpacePosition = cameraInfo.m_worldSpacePosition;
                glm::vec3 nearPlaneCenterPosition = cameraWorldSpacePosition + cameraInfo.worldSpaceForward() * cascade.n;
                glm::vec3 farPlaneCenterPosition = cameraWorldSpacePosition + cameraInfo.worldSpaceForward() * cascade.f;
                cascade.worldSpaceSphereBoundCenter = (nearPlaneCenterPosition + farPlaneCenterPosition) * .5f;

                glm::vec3 na, nb, nc, nd, fa, fb, fc, fd;
                calcPerspectiveViewFrustum(cameraInfo, cascade.n, cascade.f, na, nb, nc, nd, fa, fb, fc, fd);

                // calc max distance to cascade's vertices;
                f32 maxDistance = glm::distance(cascade.worldSpaceSphereBoundCenter, na);
                maxDistance = glm::max(maxDistance, glm::distance(cascade.worldSpaceSphereBoundCenter, nb));
                maxDistance = glm::max(maxDistance, glm::distance(cascade.worldSpaceSphereBoundCenter, nc));
                maxDistance = glm::max(maxDistance, glm::distance(cascade.worldSpaceSphereBoundCenter, nd));
                maxDistance = glm::max(maxDistance, glm::distance(cascade.worldSpaceSphereBoundCenter, fa));
                maxDistance = glm::max(maxDistance, glm::distance(cascade.worldSpaceSphereBoundCenter, fb));
                maxDistance = glm::max(maxDistance, glm::distance(cascade.worldSpaceSphereBoundCenter, fc));
                maxDistance = glm::max(maxDistance, glm::distance(cascade.worldSpaceSphereBoundCenter, fd));
                cascade.worldSpaceSphereBoundRadius = maxDistance;

                // calc the view volume for this cascade
                glm::vec3 viewSpaceSphereCenter = m_lightViewMatrix * glm::vec4(cascade.worldSpaceSphereBoundCenter, 1.f);
                AxisAlignedBoundingBox3D viewSpaceAABB;
                glm::vec3 min = viewSpaceSphereCenter - glm::vec3(cascade.worldSpaceSphereBoundRadius);
                glm::vec3 max = viewSpaceSphereCenter + glm::vec3(cascade.worldSpaceSphereBoundRadius);
                viewSpaceAABB.bound(min);
                viewSpaceAABB.bound(max);

                // hack begin: 
                // Adjust the zNear projection plane for the first cascade to avoid missing
                // capturing offscreen occluders outside of the view frustum. Only doing this for the 
                // first cascade as I only saw this artifact happens for the first cascade. If we
                // have a sphere bound that bounds the whole scene, then maybe the 
                // most robust approach that guarantees no occluders are missed in the shadow cascade,
                // is to first build a light space AABB bounding the scene's sphere bound, and then
                // move the near projection plane for shadow in Z direction (light direction) to
                // the zNear of the whole scene's light space AABB, but doing so may cause having too low
                // of a depth precision. Anyways, should make a note about this and worry about it later.
                if (i == 0)
                {
                    const f32 kZNearOffset = 40.f;
                    viewSpaceAABB.pmax.z += kZNearOffset;
                }
                // hack end 
                cascade.lightCamera.m_orthographic.viewSpaceAABB = viewSpaceAABB;
            }

            auto renderer = SceneRenderer::get();
            // calculate cascade's world space aabb
            for (u32 i = 0; i < kNumCascades; ++i)
            {
                char markerName[64];
                sprintf_s(markerName, "CSM Cascade[%u]", i);
                GPU_DEBUG_SCOPE(csmMarker, markerName);

                // todo: this is slightly awkward, should each cascade onws a SceneCamera instead? and 
                // these extra SceneCamera for each cascade only tweak camera view related data
                const CameraViewInfo& camera = m_cascades[i].lightCamera;
                SceneView::State viewState;
                viewState.resolution = m_resolution;
                viewState.aspectRatio = (f32)m_resolution.x / m_resolution.y;
                viewState.cameraForward = camera.worldSpaceForward();
                viewState.cameraRight = camera.worldSpaceRight();
                viewState.cameraUp = camera.worldSpaceUp();
                viewState.cameraPosition = camera.m_worldSpacePosition;
                viewState.viewMatrix = camera.viewMatrix();
                viewState.projectionMatrix = camera.projectionMatrix();

                renderer->renderSceneDepth(m_cascades[i].depthTexture.get(), scene, viewState);
            }

            m_lastPrimaryViewCamera = cameraInfo;
        }
        else
        {
            cyanError("CascadedShadowMap only works for perspective camera for now!");
            assert(0);
        }
    }

    void CascadedShadowMap::setShaderParameters(Pipeline* p)
    {
        p->setUniform("directionalLight.csm.lightViewMatrix", m_lightViewMatrix);
        for (i32 i = 0; i < CascadedShadowMap::kNumCascades; ++i)
        {
            char nName[64];
            sprintf_s(nName, "directionalLight.csm.cascades[%d].n", i);
            p->setUniform(nName, m_cascades[i].n);
            char fName[64];
            sprintf_s(fName, "directionalLight.csm.cascades[%d].f", i);
            p->setUniform(fName, m_cascades[i].f);
            char lightProjectionMatrixName[64];
            sprintf_s(lightProjectionMatrixName, "directionalLight.csm.cascades[%d].lightProjectionMatrix", i);
            p->setUniform(lightProjectionMatrixName, m_cascades[i].lightCamera.projectionMatrix());
            char depthTextureName[64];
            sprintf_s(depthTextureName, "directionalLight.csm.cascades[%d].depthTexture", i);
            // todo: there is a bug here, passing these name as temp chara  
            p->setTexture(depthTextureName, m_cascades[i].depthTexture.get());
        }
    }

#if 0
    void CascadedShadowMap::debugDrawCascadeBounds(SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        auto renderer = Renderer::get();

        auto debugDraw = [renderer, render, viewParameters, this]() {
            auto camera = m_lastPrimaryViewCamera;
            glm::vec3 na, nb, nc, nd, fa, fb, fc, fd;
            calcPerspectiveViewFrustum(camera, camera.m_perspective.n, camera.m_perspective.f, na, nb, nc, nd, fa, fb, fc, fd);

            glm::vec4 red(1.f, 0.f, 0.f, 1.f);
            glm::vec4 green(0.f, 1.f, 0.f, 1.f);
            glm::vec4 blue(0.f, 0.f, 1.f, 1.f);
            std::vector<Renderer::DebugPrimitiveVertex> vertices;
            Renderer::DebugPrimitiveVertex v = { glm::vec4(camera.m_worldSpacePosition, 1.f), red };
            vertices.push_back(v);
            vertices.push_back({ glm::vec4(fa, 1.f), red });
            vertices.push_back(v);
            vertices.push_back({ glm::vec4(fb, 1.f), red });
            vertices.push_back(v);
            vertices.push_back({ glm::vec4(fc, 1.f), red });
            vertices.push_back(v);
            vertices.push_back({ glm::vec4(fd, 1.f), red });

            for (i32 i = 0; i < kNumCascades; ++i)
            {
                glm::vec3 cna, cnb, cnc, cnd, cfa, cfb, cfc, cfd;
                calcPerspectiveViewFrustum(camera, m_cascades[i].n, m_cascades[i].f, cna, cnb, cnc, cnd, cfa, cfb, cfc, cfd);
                glm::vec4 color = glm::mix(green, red, (f32)i / kNumCascades);
                vertices.push_back({ glm::vec4(cna, 1.f), color });
                vertices.push_back({ glm::vec4(cnb, 1.f), color });
                vertices.push_back({ glm::vec4(cnb, 1.f), color });
                vertices.push_back({ glm::vec4(cnc, 1.f), color });
                vertices.push_back({ glm::vec4(cnc, 1.f), color });
                vertices.push_back({ glm::vec4(cnd, 1.f), color });
                vertices.push_back({ glm::vec4(cnd, 1.f), color });
                vertices.push_back({ glm::vec4(cna, 1.f), color });

                // renderer->debugDrawWireframeSphere(render->debugColor(), render->depth(), m_cascades[i].worldSpaceSphereBoundCenter, m_cascades[i].worldSpaceSphereBoundRadius, color, viewParameters);
            }

            vertices.push_back({ glm::vec4(fa, 1.f), red });
            vertices.push_back({ glm::vec4(fb, 1.f), red });
            vertices.push_back({ glm::vec4(fb, 1.f), red });
            vertices.push_back({ glm::vec4(fc, 1.f), red });
            vertices.push_back({ glm::vec4(fc, 1.f), red });
            vertices.push_back({ glm::vec4(fd, 1.f), red });
            vertices.push_back({ glm::vec4(fd, 1.f), red });
            vertices.push_back({ glm::vec4(fa, 1.f), red });

            renderer->debugDrawWorldSpaceLines(render->debugColor(), render->depth(), viewParameters, vertices);
        };

        renderer->addDebugDraw(debugDraw);
    }

    void CascadedShadowMap::debugDrawCascadeRegion(SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        auto renderer = Renderer::get();

        auto debugDraw = [renderer, this, render, viewParameters]() {
            auto camera = m_lastPrimaryViewCamera;

            // color tint the cascade allocation
            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "DebugVisualizeCSMCascadePS", SHADER_SOURCE_PATH "visualize_csm_cascade_p.glsl");
            CreatePixelPipeline(p, "DebugVisualizeCascade", vs, ps);
            auto colorBuffer = render->debugColor();
            renderer->drawFullscreenQuad(
                glm::uvec2(colorBuffer->width, colorBuffer->height),
                [colorBuffer, render](RenderPass& pass) {
                    pass.setRenderTarget(colorBuffer, 0, false);
                    pass.setDepthBuffer(render->depth(), false);
                },
                p,
                [this, render, camera, viewParameters](ProgramPipeline* p) {
                    p->setTexture("sceneDepth", render->depth());
                    p->setTexture("sceneColor", render->resolvedColor());
                    p->setUniform("csmPrimaryViewViewMatrix", camera.viewMatrix());
                    p->setUniform("csmPrimaryViewProjectionMatrix", camera.projectionMatrix());
                    p->setUniform("viewMatrix", viewParameters.viewMatrix);
                    p->setUniform("projectionMatrix", viewParameters.projectionMatrix);
                    setShaderParameters(p);
                }
            );
        };

        renderer->addDebugDraw(debugDraw);
    }

    void CascadedShadowMap::debugDrawShadowCoords(SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        auto renderer = Renderer::get();
        auto debugDraw = [renderer, render, viewParameters, this]() {
            auto camera = m_lastPrimaryViewCamera;

            // color tint the rendering output with shadow space coordinates
            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "DebugVisualizeCSMShadowCoordPS", SHADER_SOURCE_PATH "visualize_csm_shadowcoord_p.glsl");
            CreatePixelPipeline(p, "DebugVisualizeShadowCoord", vs, ps);
            auto colorBuffer = render->debugColor();
            renderer->drawFullscreenQuad(
                glm::uvec2(colorBuffer->width, colorBuffer->height),
                [colorBuffer, render](RenderPass& pass) {
                    pass.setRenderTarget(colorBuffer, 0, false);
                    pass.setDepthBuffer(render->depth(), false);
                },
                p,
                [this, render, camera, viewParameters](ProgramPipeline* p) {
                    p->setTexture("sceneDepth", render->depth());
                    p->setTexture("sceneColor", render->resolvedColor());
                    p->setUniform("csmPrimaryViewViewMatrix", camera.viewMatrix());
                    p->setUniform("csmPrimaryViewProjectionMatrix", camera.projectionMatrix());
                    p->setUniform("viewMatrix", viewParameters.viewMatrix);
                    p->setUniform("projectionMatrix", viewParameters.projectionMatrix);
                    setShaderParameters(p);
                }
            );
        };

        renderer->addDebugDraw(debugDraw);
    }

    void CascadedShadowMap::debugDraw(SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        GPU_DEBUG_SCOPE(csmDebugDraw, "CSMDebugDraw");

        // note: There is an implicit drawing order here, the order has to be as is or else
        // the cascade visualization will be wrong.
        debugDrawShadowCoords(render, viewParameters);
        debugDrawCascadeBounds(render, viewParameters);
    }
#endif
}
