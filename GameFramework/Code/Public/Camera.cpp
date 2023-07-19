#include "Camera.h"
#include "GfxInterface.h"

namespace Cyan
{
    Camera::Camera(const glm::uvec2& renderResolution)
        : m_resolution(renderResolution)
    {
        m_sceneRenderer = ISceneRenderer::create();
    }

    Camera::~Camera() { }

    void Camera::onRenderStart()
    {
        updateSceneViewInfo();
    }

    void Camera::render(IScene* scene)
    {
        if (bPower)
        {
            onRenderStart();
            if (scene != nullptr)
            {
                m_sceneRenderer->render(m_sceneRender.get(), scene, m_sceneViewInfo);
            }
            onRenderFinish();
        }
    }

    void Camera::onRenderFinish()
    {

    }

    void Camera::updateSceneViewInfo()
    {
        if (m_numRenderedFrames > 0)
        {
            m_sceneViewInfo.prevFrameViewMatrix = m_sceneViewInfo.viewMatrix;
            m_sceneViewInfo.prevFrameProjectionMatrix = m_sceneViewInfo.projectionMatrix;
            m_sceneViewInfo.prevFrameCameraPosition = m_sceneViewInfo.cameraPosition;
        }

        m_sceneViewInfo.resolution = m_resolution;
        m_sceneViewInfo.aspectRatio = m_cameraViewInfo.m_perspective.aspectRatio;
        m_sceneViewInfo.viewMatrix = m_cameraViewInfo.viewMatrix();
        m_sceneViewInfo.projectionMatrix = m_cameraViewInfo.projectionMatrix();
        m_sceneViewInfo.cameraPosition = m_cameraViewInfo.m_worldSpacePosition;
        m_sceneViewInfo.cameraRight = m_cameraViewInfo.worldSpaceRight();
        m_sceneViewInfo.cameraForward = m_cameraViewInfo.worldSpaceForward();
        m_sceneViewInfo.cameraUp = m_cameraViewInfo.worldSpaceUp();
        m_sceneViewInfo.frameCount = m_numRenderedFrames;
        m_sceneViewInfo.elapsedTime = m_elapsedTime;
        m_sceneViewInfo.deltaTime = m_deltaTime;
    }
}