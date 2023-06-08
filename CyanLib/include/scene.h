#pragma once

#include <vector>
#include <queue>
#include <stack>
#include <memory>

#include <glm/glm.hpp>

#include "Camera.h"
#include "Texture.h"
#include "Mesh.h"
#include "Lights.h"

namespace Cyan 
{
    class World;
    class Camera;
    class ProgramPipeline;
    class SceneRender;

    class Scene
    {
    public:
        Scene(World* world);
        ~Scene() { }

        void addCamera(Camera* camera);
        void addStaticMeshInstance(StaticMesh::Instance* staticMeshInstance);
        void removeStaticMeshInstance() { }

        void addDirectionalLight(DirectionalLight* directionalLight);
        void removeDirectionalLight() { }

        i32 getFrameCount();
        f32 getElapsedTime();
        f32 getDeltaTime();

        World* m_world = nullptr;
        std::vector<Camera*> m_cameras;
        std::vector<std::shared_ptr<SceneRender>> m_renders;
        std::vector<StaticMesh::Instance*> m_staticMeshInstances;

        // todo: lights
        DirectionalLight* m_directionalLight = nullptr;
    };
}