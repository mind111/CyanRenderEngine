#include "SceneCameraComponent.h"
#include "Entity.h"
#include "World.h"

namespace Cyan
{
    SceneCameraComponent::SceneCameraComponent(const char* name, const Transform& transform)
        : SceneComponent(name, transform)
    {
        m_sceneCamera = std::make_unique<SceneCamera>();
    }

    SceneCameraComponent::~SceneCameraComponent()
    {

    }

    void SceneCameraComponent::update()
    {
        if (m_sceneCamera != nullptr)
        {
            m_sceneCamera->incrementFrameCount();
        }
    }

    void SceneCameraComponent::setOwner(Entity* owner)
    {
        SceneComponent::setOwner(owner);

        World* world = m_owner->getWorld();
        if (world != nullptr)
        {
            world->addSceneCamera(m_sceneCamera.get());
        }
    }

    void SceneCameraComponent::onTransformUpdated()
    {
        CameraViewInfo& cameraViewInfo = m_sceneCamera->m_cameraViewInfo;
        // update camera's transform relative to parent
        Transform localSpaceTransform = getLocalSpaceTransform();
        glm::mat4 localTransformMatrix = localSpaceTransform.toMatrix();
        cameraViewInfo.m_localSpaceRight = glm::normalize(localTransformMatrix * glm::vec4(CameraViewInfo::localRight, 0.f));
        cameraViewInfo.m_localSpaceUp = glm::normalize(localTransformMatrix * glm::vec4(CameraViewInfo::localUp, 0.f));
        cameraViewInfo.m_localSpaceForward = glm::normalize(localTransformMatrix * glm::vec4(CameraViewInfo::localForward, 0.f));

        // update camera's global transform
        Transform worldSpaceTransform = getWorldSpaceTransform();
        glm::mat4 worldSpaceMatrix = worldSpaceTransform.toMatrix();
        cameraViewInfo.m_worldSpaceRight = glm::normalize(worldSpaceMatrix * glm::vec4(CameraViewInfo::localRight, 0.f));
        cameraViewInfo.m_worldSpaceUp = glm::normalize(worldSpaceMatrix * glm::vec4(CameraViewInfo::localUp, 0.f));
        cameraViewInfo.m_worldSpaceForward = glm::normalize(worldSpaceMatrix * glm::vec4(CameraViewInfo::localForward, 0.f));
        cameraViewInfo.m_worldSpacePosition = worldSpaceTransform.translation;
    }

    void SceneCameraComponent::setRenderMode(const SceneCamera::RenderMode& renderMode)
    {
        m_sceneCamera->m_renderMode = renderMode;
    }

    void SceneCameraComponent::setResolution(const glm::uvec2& resolution)
    {
        m_sceneCamera->m_resolution = resolution;
        m_sceneCamera->m_cameraViewInfo.m_perspective.aspectRatio = (f32)m_sceneCamera->m_resolution.x / m_sceneCamera->m_resolution.y;
    }

    void SceneCameraComponent::setProjectionMode(CameraViewInfo::ProjectionMode& pm)
    {

    }

    void SceneCameraComponent::setNearClippingPlane(f32 n)
    {
        m_sceneCamera->m_cameraViewInfo.m_perspective.n = n;
    }

    void SceneCameraComponent::setFarClippingPlane(f32 f)
    {

    }

    void SceneCameraComponent::setFOV(f32 fov)
    {

    }
}