#include "SceneRender.h"
#include "GfxTexture.h"
#include "Camera.h"
#include "Scene.h"

namespace Cyan
{
    SceneRender::Output::Output(const glm::uvec2& renderResolution)
        : resolution(renderResolution)
    {
        // depth
        {
            GfxDepthTexture2D::Spec spec(resolution.x, resolution.y, 1);
            depth = std::unique_ptr<GfxDepthTexture2D>(GfxDepthTexture2D::create(spec, Sampler2D()));
        }
        // normal
        {
            GfxTexture2D::Spec spec(resolution.x, resolution.y, 1, PF_RGB32F);
            normal = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
        }
        // lighting
        {
            GfxTexture2D::Spec spec(resolution.x, resolution.y, 1, PF_RGB16F);
            albedo = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            metallicRoughness = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            directLighting = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            directDiffuseLighting = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            indirectLighting = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            lightingOnly = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            ao = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            aoHistory = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            color = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            resolvedColor = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
        }
    }

    SceneRender::ViewParameters::ViewParameters(Scene* scene, Camera* camera)
        : renderResolution(camera->m_renderResolution)
        , aspectRatio((f32)renderResolution.x / renderResolution.y)
        , viewMatrix(camera->view())
        , projectionMatrix(camera->projection())
        , cameraPosition(camera->m_position)
        , cameraRight(camera->right())
        , cameraForward(camera->forward())
        , cameraUp(camera->up())
        , frameCount(scene->getFrameCount())
        , elapsedTime(scene->getElapsedTime())
        , deltaTime(scene->getDeltaTime())
    {

    }

    void SceneRender::ViewParameters::setShaderParameters(ProgramPipeline* p) const
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

    void SceneRender::update()
    {
        // update view parameters
        if (m_camera != nullptr)
        {
            m_viewParameters.renderResolution = m_camera->m_renderResolution;
            m_viewParameters.aspectRatio = (f32)m_camera->m_renderResolution.x / m_camera->m_renderResolution.y;
            m_viewParameters.viewMatrix = m_camera->view();
            m_viewParameters.projectionMatrix = m_camera->projection();
            m_viewParameters.cameraPosition = m_camera->m_position;
            m_viewParameters.cameraRight = m_camera->right();
            m_viewParameters.cameraForward = m_camera->forward();
            m_viewParameters.cameraUp = m_camera->up();
        }
    }

    SceneRender::SceneRender(Scene* scene, Camera* camera)
        : m_scene(scene), m_camera(camera), m_viewParameters(scene, camera)
    {
        m_output = std::make_unique<Output>(camera->m_renderResolution);
        m_camera->setSceneRender(this);
    }
}