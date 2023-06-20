#include "CyanRenderer.h"
#include "SceneCamera.h"
#include "Shader.h"

namespace Cyan
{
    const char* SceneCamera::renderModeName(const RenderMode& renderMode)
    {
        switch (renderMode)
        {
        case RenderMode::kSceneColor: return "SceneColor";
        case RenderMode::kResolvedSceneColor: return "SceneResolvedColor";
        case RenderMode::kSceneAlbedo: return "SceneAlbedo";
        case RenderMode::kSceneDepth: return "SceneDepth";
        case RenderMode::kSceneNormal: return "SceneNormal";
        case RenderMode::kSceneDirectLighting: return "SceneDirectLighting";
        case RenderMode::kSceneIndirectLighting: return "SceneIndirectLighting";
        case RenderMode::kSceneLightingOnly: return "ScceneLightingOnly";
        case RenderMode::kSSGIAO: return "SSGIAO";
        case RenderMode::kSSGIDiffuse: return "SSGIDiffuse";
        case RenderMode::kDebug: return "Debug";
        case RenderMode::kWireframe: return "Wireframe";
        default: assert(0);
        }
    }

    SceneCamera::SceneCamera(const glm::uvec2& resolution)
        : m_resolution(resolution), m_scene(nullptr)
    {
        m_renderMode = VM_RESOLVED_SCENE_COLOR;
        m_camera = Camera(resolution);
        m_render = std::make_unique<SceneRender>(m_resolution);
    }

    SceneCamera::~SceneCamera()
    {

    }

    void SceneCamera::onRenderStart()
    {
        // update view parameters
        m_viewParameters.update(*this);

        if (m_numRenderedFrames > 0)
        {
            auto renderer = Renderer::get();
            renderer->copyDepth(m_render->prevFrameDepth(), m_render->depth());
            renderer->blitTexture(m_render->prevFrameIndirectIrradiance(), m_render->indirectIrradiance());
        }
    }

    void SceneCamera::render()
    {
        if (m_bPower)
        {
            onRenderStart();
            if (m_scene != nullptr)
            {
                auto renderer = Renderer::get();
                renderer->renderSceneDepthPrepass(m_render->depth(), m_scene, m_viewParameters);
                // build hiz
                m_render->hiZ()->build(m_render->depth());
                renderer->renderSceneGBuffer(m_render->albedo(), m_render->normal(), m_render->metallicRoughness(), m_render->depth(), m_scene, m_viewParameters);
                renderer->renderSceneLighting(m_scene, m_render.get(), m_camera, m_viewParameters);
                renderer->postprocess(m_render->resolvedColor(), m_render->color());
                renderer->drawDebugObjects(m_render.get());
            }
            onRenderFinish();
        }
    }

    void SceneCamera::onRenderFinish()
    {
        m_numRenderedFrames++;
    }

    GfxTexture2D* SceneCamera::getRender()
    {
        switch (m_renderMode)
        {
            case RenderMode::kSceneColor: return m_render->color();
            case RenderMode::kResolvedSceneColor: return m_render->resolvedColor();
            case RenderMode::kSceneAlbedo: return m_render->albedo();
            case RenderMode::kSceneDepth: return m_render->depth();
            case RenderMode::kSceneNormal: return m_render->normal();
            case RenderMode::kSceneDirectLighting: return m_render->directLighting();
            case RenderMode::kSceneIndirectLighting: return m_render->indirectLighting();
            case RenderMode::kSSGIAO: return m_render->ao();
            case RenderMode::kSSGIDiffuse: return m_render->indirectIrradiance();
            case RenderMode::kDebug: return m_render->debugColor();
            case RenderMode::kWireframe:
            default: assert(0);
        }
    }

    void SceneCamera::setScene(Scene* scene)
    {
        // make sure that this camera that we are about to add is not in another scene
        assert(m_scene == nullptr);
        m_scene = scene;
    }

    void SceneCamera::ViewParameters::update(const SceneCamera& camera)
    {
        if (camera.m_numRenderedFrames > 0)
        {
            prevFrameViewMatrix = viewMatrix;
            prevFrameProjectionMatrix = projectionMatrix;
            prevFrameCameraPosition = cameraPosition;
        }

        renderResolution = camera.m_resolution;
        aspectRatio = camera.m_camera.m_perspective.aspectRatio;
        viewMatrix = camera.m_camera.viewMatrix();
        projectionMatrix = camera.m_camera.projectionMatrix();
        cameraPosition = camera.m_camera.m_worldSpacePosition;
        cameraRight = camera.m_camera.worldSpaceRight();
        cameraForward = camera.m_camera.worldSpaceForward();
        cameraUp = camera.m_camera.worldSpaceUp();
        frameCount = camera.m_numRenderedFrames;
        elapsedTime = camera.m_elapsedTime;
        deltaTime = camera.m_deltaTime;
    }

    void SceneCamera::ViewParameters::setShaderParameters(ProgramPipeline* p) const
    {
        p->setUniform("viewParameters.renderResolution", renderResolution);
        p->setUniform("viewParameters.aspectRatio", aspectRatio);
        p->setUniform("viewParameters.viewMatrix", viewMatrix);
        p->setUniform("viewParameters.prevFrameViewMatrix", prevFrameViewMatrix);
        p->setUniform("viewParameters.projectionMatrix", projectionMatrix);
        p->setUniform("viewParameters.prevFrameProjectionMatrix", prevFrameProjectionMatrix);
        p->setUniform("viewParameters.cameraPosition", cameraPosition);
        p->setUniform("viewParameters.prevFrameCameraPosition", prevFrameCameraPosition);
        p->setUniform("viewParameters.cameraRight", cameraRight);
        p->setUniform("viewParameters.cameraForward", cameraForward);
        p->setUniform("viewParameters.cameraUp", cameraUp);
        p->setUniform("viewParameters.frameCount", frameCount);
        p->setUniform("viewParameters.elapsedTime", elapsedTime);
        p->setUniform("viewParameters.deltaTime", deltaTime);
    }
}