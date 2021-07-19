#pragma once

#include <vector>
#include <queue>
#include "glm.hpp"

#include "Camera.h"
#include "Texture.h"
#include "Entity.h"
#include "Light.h"
#include "Material.h"
#include "LightProbe.h"

struct SceneNode
{
    SceneNode* m_parent;
    std::vector<SceneNode*> m_child;
    Entity* m_entity;
    void addChild(SceneNode* child);

    SceneNode* find(const char* name)
    {
        // breath first search
        std::queue<SceneNode*> nodesQueue;
        nodesQueue.push(this);
        while (!nodesQueue.empty())
        {
            SceneNode* node = nodesQueue.front();
            nodesQueue.pop();
            if (strcmp(node->m_entity->m_name, name) == 0)
            {
                return node;
            }
            for (auto* child : node->m_child)
            {
                nodesQueue.push(child);
            }
        }
        return nullptr;
    }

    SceneNode* removeChild(const char* name)
    {
        SceneNode* nodeToRemove = find(name);
        if (nodeToRemove)
        {
            SceneNode* parent = nodeToRemove->m_parent;
            u32 index = 0;
            for (u32 i = 0; i < parent->m_child.size(); ++i)
            {
                if (parent->m_child[i] == nodeToRemove) 
                {
                    index = i;
                    break;
                }
            }
            nodeToRemove->m_parent = nullptr;
            parent->m_child.erase(parent->m_child.begin() + index);
        }
        return nodeToRemove;
    }

    void update();
};

// TODO: implement this
struct LightingEnvironment
{
    std::vector<PointLight> pLights;
    std::vector<DirectionalLight> dLights;
};

struct Scene 
{
    static const u32 kMaxNumPointLights = 20u;
    static const u32 kMaxNumDirLights = 20u;
    Camera mainCamera;
    u32 m_currentEnvMap;
    std::vector<Entity*> entities;
    std::vector<PointLight> pLights;
    std::vector<DirectionalLight> dLights;
    RegularBuffer* m_pointLightsBuffer;
    RegularBuffer* m_dirLightsBuffer;
    LightProbe*    m_currentProbe;
    LightProbe*    m_lastProbe;
    std::string    m_name;
    SceneNode*     m_root;

    // TODO: custom update method this scene
    // void update()
};

class SceneManager {
public:
    static void updateSceneGraph(Scene* scene);
    static void updateDirLights(Scene* scene);
    static void updatePointLights(Scene* scene);
    static void setLightProbe(Scene* scene, LightProbe* probe);
    static void createDirectionalLight(Scene* scene, glm::vec3 color, glm::vec3 direction, float intensity);
    static void createPointLight(Scene* scene, glm::vec3 color, glm::vec3 position, float intensity);
    static Entity* createEntity(Scene* scene, const char* entityName, const char* meshName, Transform transform, bool hasTransform);
    static Entity* getEntity(Scene* scene, u32 id) 
    {
        return scene->entities[id];
    }
};

extern SceneManager sceneManager;