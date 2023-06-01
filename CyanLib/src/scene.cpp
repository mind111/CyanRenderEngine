#include <iostream>
#include <fstream>
#include <queue>

#include <tiny_gltf/json.hpp>
#include <glm/glm.hpp>
#include <stbi/stb_image.h>
#include <glm/gtc/matrix_transform.hpp>

#include "World.h"
#include "Mesh.h"
#include "CyanAPI.h"
#include "Scene.h"
#include "GraphicsSystem.h"
#include "LightComponents.h"

namespace Cyan
{
#if 0
    Scene::Scene(const char* inSceneName, f32 cameraAspectRatio)
        : m_name(inSceneName)
    {
        m_rootEntity = createEntity("Root", Transform());
        m_mainCamera = std::unique_ptr<CameraEntity>(createPerspectiveCamera(
            /*name=*/"Camera",
            /*transform=*/Transform {
                glm::vec3(0.f, 3.2f, 8.f)
            },
            /*inLookAt=*/glm::vec3(0., 1., -1.),
            /*inWorldUp=*/glm::vec3(0.f, 1.f, 0.f),
            /*inFov=*/45.f,
            2.f,
            150.f,
            cameraAspectRatio
        ));
    }

    // todo: optimize this ...
    void Scene::update()
    {
        m_mainCamera->update();

        // update transforms of all entities and scene components
        std::queue<Entity*> entities;
        entities.push(m_rootEntity);

        while (!entities.empty())
        {
            auto e = entities.front();
            entities.pop();
            for (auto child : e->childs)
            {
                entities.push(child);
            }

            if (auto parent = e->parent)
            {
                // update transform of this entity's root scene component
                glm::mat4 parentGlobalMatrix = parent->getRootSceneComponent()->getWorldTransformMatrix();
                auto sceneRoot = e->getRootSceneComponent();
                sceneRoot->m_worldTransform.fromMatrix(parentGlobalMatrix * sceneRoot->m_localTransform.toMatrix());

                // update transform of all owning scene components
                std::queue<Component*> components;
                for (auto child : sceneRoot->children)
                {
                    components.push(child);
                }

                while (!components.empty())
                {
                    auto c = components.front();
                    components.pop();
                    for (auto child : c->children)
                    {
                        components.push(child);
                    }
                    if (auto sc = dynamic_cast<SceneComponent*>(c))
                    {
                        auto parent = sc->parent;
                        if (parent)
                        {
                            auto parentSceneComponent = dynamic_cast<SceneComponent*>(parent);
                            while (!parentSceneComponent)
                            {
                                if (parent->parent)
                                {
                                    parent = parent->parent;
                                    parentSceneComponent = dynamic_cast<SceneComponent*>(parent);
                                }
                                else
                                {
                                    break;
                                }
                            }
                            assert(parentSceneComponent);
                            sc->m_worldTransform.fromMatrix(parentSceneComponent->getWorldTransformMatrix() * sc->m_localTransform.toMatrix());
                        }
                    }
                }
            }
        }
    }

    Entity* Scene::createEntity(const char* name, const Transform& transform, Entity* inParent, u32 properties)
    {
        Entity* entity = new Entity(this, name, transform, inParent, properties);
        m_entities.push_back(entity);
        return entity;
    }

    StaticMeshEntity* Scene::createStaticMeshEntity(const char* name, const Transform& transform, StaticMesh* inMesh, Entity* inParent, u32 properties)
    {
        auto staticMesh = new StaticMeshEntity(this, name, transform, inMesh, inParent, properties);
        m_entities.push_back(staticMesh);
        m_staticMeshes.push_back(staticMesh);
        return staticMesh;
    }

    CameraEntity* Scene::createPerspectiveCamera(const char* name, const Transform& transform, const glm::vec3& inLookAt, const glm::vec3& inWorldUp, f32 inFov, f32 inN, f32 inF, f32 inAspectRatio, Entity* inParent, u32 properties)
    {
        auto camera = new CameraEntity(this, name, transform, inLookAt, inWorldUp, inFov, inN, inF, inAspectRatio, inParent, properties);
        m_entities.push_back(camera);
        return camera;
    }

    DirectionalLightEntity* Scene::createDirectionalLight(const char* name, const glm::vec3& direction, const glm::vec4& colorAndIntensity)
    {
        assert(m_directionalLight == nullptr);
        // only allow one directional light exists in the scene
        if (m_directionalLight == nullptr)
        {
            DirectionalLightEntity* directionalLight = new DirectionalLightEntity(this, name, Transform(), nullptr, direction, colorAndIntensity, true);
            m_entities.push_back(directionalLight);
            m_directionalLight = directionalLight;
            return directionalLight;
        }
    }

    SkyLight* Scene::createSkyLight(const char* name, const glm::vec4& colorAndIntensity)
    {
        if (skyLight)
        {
            cyanError("There is already a skylight exist in scene %s", this->m_name);
            return nullptr;
        }
    }

    SkyLight* Scene::createSkyLightFromSkybox(Skybox* srcSkybox) 
    {
        return nullptr;
    }

    SkyLight* Scene::createSkyLight(const char* name, const char* srcHDRI) 
    { 
        if (skyLight)
        {
            cyanError("There is already a skylight exist in scene %s", this->m_name);
            return nullptr;
        }
        skyLight = new SkyLight(this, glm::vec4(1.f), srcHDRI);
        return skyLight;
    }

    Skybox* Scene::createSkybox(const char* name, const char* srcHDRIPath, const glm::uvec2& resolution) 
    {
        if (skybox) 
        {
            cyanError("There is already a skybox exist in scene %s", this->m_name);
            return nullptr;
        }
        skybox = new Skybox(name, srcHDRIPath, resolution);
        return skybox;
    }

    void Scene::createPointLight(const char* name, const glm::vec3 position, const glm::vec4& colorAndIntensity)
    {

    }
#else
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
            ao = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            aoHistory = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
        }
    }

    SceneRender::ViewParameters::ViewParameters(Scene* scene, Camera* camera)
        : renderResolution(camera->m_renderResolution)
        , aspectRatio((f32)renderResolution.x / renderResolution.y)
        , viewMatrix(camera->view())
        , projectionMatrix(camera->projection())
        , cameraPosition(camera->m_position)
        , cameraLookAt(camera->m_lookAt)
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
        p->setUniform("viewParameters.cameraLookAt", cameraLookAt);
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
            m_viewParameters.cameraLookAt = m_camera->m_lookAt;
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

    Scene::Scene(World* world)
        : m_world(world)
    {

    }

    void Scene::addCamera(Camera* camera)
    {
        m_cameras.push_back(camera);

        if (camera->bEnabledForRendering)
        {
            m_renders.push_back(std::make_shared<SceneRender>(this, camera));
        }
    }

    void Scene::addStaticMeshInstance(StaticMesh::Instance* staticMeshInstance)
    {
        m_staticMeshInstances.push_back(staticMeshInstance);
    }

    i32 Scene::getFrameCount()
    {
        if (m_world != nullptr)
        {
            return m_world->m_frameCount;
        }
        return -1;
    }

    f32 Scene::getElapsedTime()
    {
        if (m_world != nullptr)
        {
            return m_world->m_elapsedTime;
        }
        return 0.f;
    }

    f32 Scene::getDeltaTime()
    {
        if (m_world != nullptr)
        {
            return m_world->m_deltaTime;
        }
        return 0.f;
    }
#endif
}
