#include <iostream>
#include <fstream>

#include "json.hpp"
#include "glm.hpp"
#include "stb_image.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "gtc/matrix_transform.hpp"

#include "CyanAPI.h"
#include "Scene.h"

SceneManager sceneManager;

Entity* SceneManager::createEntity(Scene* scene, const char* meshName, Transform transform)
{
    using Cyan::Mesh;
    using Cyan::MaterialInstance;

    Mesh* mesh = Cyan::getMesh(meshName);
    u32 numSubMeshes = (u32)mesh->m_subMeshes.size();
    Entity* entity = (Entity*)CYAN_ALLOC(sizeof(Entity));

    // mesh
    entity->m_mesh = mesh; 
    // material instances
    entity->m_matls = (MaterialInstance**)CYAN_ALLOC(numSubMeshes * sizeof(MaterialInstance*));
    // id
    entity->m_entityId = scene->entities.size() > 0 ? scene->entities.size() + 1 : 1;
    // transform
    entity->m_xform.translation = transform.translation;
    entity->m_xform.qRot = transform.qRot;
    entity->m_xform.scale = transform.scale;

    scene->entities.push_back(entity);
    return entity; 
}

void SceneManager::createDirectionalLight(Scene& scene, glm::vec3 color, glm::vec3 direction, float intensity)
{
    scene.dLights.push_back(DirectionalLight{
        glm::vec4(color, intensity),
        glm::vec4(direction, 0.f)
    });
}

void SceneManager::createPointLight(Scene& scene, glm::vec3 color, glm::vec3 position, float intensity)
{
    scene.pLights.push_back(PointLight{
        glm::vec4(color, intensity),
        glm::vec4(position, 1.f)
    });
}