#include "CyanRenderer.h"
#include "SceneCamera.h"
#include "Shader.h"

namespace Cyan
{
    SceneCamera::SceneCamera(const glm::uvec2& resolution)
        : m_resolution(resolution), m_scene(nullptr)
    {
        m_renderMode = VM_RESOLVED_SCENE_COLOR;
        m_camera = Camera();
        m_render = std::make_unique<SceneRender>(m_resolution);
    }

    SceneCamera::~SceneCamera()
    {

    }

    void SceneCamera::render()
    {
        if (m_bPower)
        {
            // update view parameters
            m_viewParameters.update(*this);

            if (m_scene != nullptr)
            {
                auto renderer = Renderer::get();
                renderer->renderSceneDepthPrepass(m_render->depth(), m_scene, m_viewParameters);
                renderer->renderSceneGBuffer(m_render->albedo(), m_render->normal(), m_render->metallicRoughness(), m_render->depth(), m_scene, m_viewParameters);
                renderer->renderSceneLighting(m_scene, m_render.get(), m_camera, m_viewParameters);
                renderer->postprocess(m_render->resolvedColor(), m_render->color());
                renderer->drawDebugObjects(m_render.get());
            }

            m_numRenderedFrames++;
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
        p->setUniform("viewParameters.projectionMatrix", projectionMatrix);
        p->setUniform("viewParameters.cameraPosition", cameraPosition);
        p->setUniform("viewParameters.cameraRight", cameraRight);
        p->setUniform("viewParameters.cameraForward", cameraForward);
        p->setUniform("viewParameters.cameraUp", cameraUp);
        p->setUniform("viewParameters.frameCount", frameCount);
        p->setUniform("viewParameters.elapsedTime", elapsedTime);
        p->setUniform("viewParameters.deltaTime", deltaTime);
    }
}