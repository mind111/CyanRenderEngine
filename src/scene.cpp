#include <iostream>
#include <fstream>

#include "json.hpp"
#include "glm.hpp"
#include "stb_image.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "gtc/matrix_transform.hpp"

#include "Scene.h"

SceneManager sceneManager;

// TODO: For now, entityId cannot be reused
// NOTE: Reserve entityId 0 for null-entity
Entity* SceneManager::createEntity(Scene& scene)
{
    Entity e = { };
    e.entityId = scene.entities.size() > 0 ? scene.entities.size() + 1 : 1;
    // !PERF
    scene.entities.push_back(e);
    return &scene.entities[scene.entities.size() - 1]; 
}
