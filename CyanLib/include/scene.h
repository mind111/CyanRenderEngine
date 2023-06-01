#pragma once

#include <vector>
#include <queue>
#include <stack>
#include <memory>

#include <glm/glm.hpp>

#include "Camera.h"
#include "Texture.h"
#include "Mesh.h"

namespace Cyan 
{
#if 0
    class Scene 
    {
    public:
        Scene(const char* sceneName, f32 cameraAspectRatio);

        void update();

        // entities
        Entity* createEntity(const char* name, const Transform& transform, Entity* inParent = nullptr, u32 properties = (EntityFlag_kDynamic | EntityFlag_kVisible | EntityFlag_kCastShadow));
        StaticMeshEntity* createStaticMeshEntity(const char* name, const Transform& transform, StaticMesh* inMesh, Entity* inParent = nullptr, u32 properties = (EntityFlag_kDynamic | EntityFlag_kVisible | EntityFlag_kCastShadow));
        CameraEntity* createPerspectiveCamera(const char* name, const Transform& transform, const glm::vec3& inLookAt, const glm::vec3& inWorldUp, f32 inFov, f32 inN, f32 inF, f32 inAspectRatio, Entity* inParent = nullptr, u32 properties = (EntityFlag_kDynamic));

        // lights
        DirectionalLightEntity* createDirectionalLight(const char* name, const glm::vec3& direction, const glm::vec4& colorAndIntensity);
        SkyLight* createSkyLight(const char* name, const glm::vec4& colorAndIntensity);
        SkyLight* createSkyLight(const char* name, const char* srcHDRI);
        SkyLight* createSkyLightFromSkybox(Skybox* srcSkybox);
        void createPointLight(const char* name, const glm::vec3 position, const glm::vec4& colorAndIntensity);

        // skybox
        Skybox* createSkybox(const char* name, const char* srcHDRIPath, const glm::uvec2& resolution);

        // getters
        Entity* getEntity(u32 index) { return m_entities[index]; }
        Entity* getEntity(const char* name) {
            for (auto& entity : m_entities) {
                if (strcmp(name, entity->name.c_str()) == 0) {
                    return entity;
                }
            }
            return nullptr;
        }

        std::string m_name;
        BoundingBox3D m_aabb;

        /**
        * Currently active camera that will be used to render the scene, a scene can potentially
        * has multiple cameras but only one will be active at any moment.
        */
        std::unique_ptr<CameraEntity> m_mainCamera = nullptr;

        // scene hierarchy
        Entity* m_rootEntity = nullptr;
        std::vector<Entity*> m_entities;

        // lights
        DirectionalLightEntity* m_directionalLight = nullptr;
        // todo: implement SkyLightEntity and SkyboxEntity, so that these two can be merged into "m_entities"
        SkyLight* skyLight = nullptr;
        Skybox* skybox = nullptr;

        // rendering related book keeping
        std::vector<StaticMeshEntity*> m_staticMeshes;
        std::vector<ILightComponent*> m_lightComponents;
    };
#else
    class World;
    class Camera;
    class ProgramPipeline;

    class SceneRender
    {
    public:

        struct Output
        {
            Output(const glm::uvec2& inRenderResolution);

            glm::uvec2 resolution;
            std::unique_ptr<GfxDepthTexture2D> HiZ;
            std::unique_ptr<GfxDepthTexture2D> depth;
            std::unique_ptr<GfxTexture2D> normal;
            std::unique_ptr<GfxTexture2D> albedo;
            std::unique_ptr<GfxTexture2D> metallicRoughness;
            std::unique_ptr<GfxTexture2D> directLighting;
            std::unique_ptr<GfxTexture2D> directDiffuseLighting;
            std::unique_ptr<GfxTexture2D> indirectLighting;
            std::unique_ptr<GfxTexture2D> aoHistory;
            std::unique_ptr<GfxTexture2D> ao;
            std::unique_ptr<GfxTexture2D> bentNormal;
            std::unique_ptr<GfxTexture2D> irradiance;
            std::unique_ptr<GfxTexture2D> color;
        };

        struct ViewParameters
        {
            ViewParameters(Scene* scene, Camera* camera);

            void setShaderParameters(ProgramPipeline* p) const;

            glm::uvec2 renderResolution;
            f32 aspectRatio;
            glm::mat4 viewMatrix;
            glm::mat4 projectionMatrix;
            glm::vec3 cameraPosition;
            glm::vec3 cameraLookAt;
            glm::vec3 cameraRight;
            glm::vec3 cameraForward;
            glm::vec3 cameraUp;
            i32 frameCount;
            f32 elapsedTime;
            f32 deltaTime;
        };

        SceneRender(Scene* scene, Camera* camera);
        ~SceneRender() { }

        void update();

        GfxDepthTexture2D* depth() { return m_output->depth.get(); }
        GfxTexture2D* normal() { return m_output->normal.get(); }
        GfxTexture2D* albedo() { return m_output->albedo.get(); }
        GfxTexture2D* metallicRoughness() { return m_output->metallicRoughness.get(); }
        GfxTexture2D* directLighting() { return m_output->directLighting.get(); }
        GfxTexture2D* indirectLighting() { return m_output->indirectLighting.get(); }
        GfxTexture2D* ao() { return m_output->ao.get(); }
        GfxTexture2D* color() { return m_output->color.get(); }

        Scene* m_scene = nullptr;
        Camera* m_camera = nullptr;
        ViewParameters m_viewParameters;

    protected:
        std::unique_ptr<Output> m_output = nullptr;
    };

    class Scene
    {
    public:
        Scene(World* world);
        ~Scene() { }

        void addCamera(Camera* camera);
        void addStaticMeshInstance(StaticMesh::Instance* staticMeshInstance);
        void removeStaticMeshInstance() { }

        void addDirectionalLight() { }
        void removeDirectionalLight() { }

        i32 getFrameCount();
        f32 getElapsedTime();
        f32 getDeltaTime();

        World* m_world = nullptr;
        std::vector<Camera*> m_cameras;
        std::vector<std::shared_ptr<SceneRender>> m_renders;
        std::vector<StaticMesh::Instance*> m_staticMeshInstances;

        // todo: lights
    };
#endif
}