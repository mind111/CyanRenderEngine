#pragma once

#include <vector>
#include <queue>
#include <stack>
#include <memory>

#include <glm/glm.hpp>

#include "Allocator.h"
#include "Camera.h"
#include "Texture.h"
#include "LightEntities.h"
#include "Mesh.h"
#include "Material.h"
#include "LightProbe.h"
#include "SkyBox.h"
#include "Lights.h"
#include "StaticMeshEntity.h"

namespace Cyan 
{
    struct SceneComponent;
    struct ILightComponent;

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
        // todo: implement SkyLightEntity and SkyboxEntity, so that these two can be merged into "m_entities"
        SkyLight* skyLight = nullptr;
        Skybox* skybox = nullptr;

        // rendering related book keeping
        std::vector<StaticMeshEntity*> m_staticMeshes;
        std::vector<ILightComponent*> m_lightComponents;
    };
}