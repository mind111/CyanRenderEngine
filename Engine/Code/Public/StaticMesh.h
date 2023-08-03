#pragma once

#include <mutex>

#include "Asset.h"
#include "Gfx.h"
#include "Geometry.h"
#include "Transform.h"

namespace Cyan
{
    // class Material;
    // class MaterialInstance;
    class Scene;
    class StaticMesh;
    class StaticMeshInstance;

    class StaticSubMesh
    {
    public:
        using Listener = std::function<void(StaticSubMesh* sm)>;

        StaticSubMesh(StaticMesh* parent, u32 index);
        ~StaticSubMesh();

        const std::string& getName() { return m_name; }
        // async loading related
        void onLoaded();
        void addListener(const Listener& listener);
        std::mutex m_listenersMutex;
        std::vector<Listener> m_listeners;
        std::atomic<bool> bLoaded = false;

        i32 numVertices() { return m_geometry->numVertices(); }
        i32 numIndices() { return m_geometry->numIndices(); }
        StaticMesh* getParentMesh() { return m_parent; }
        Geometry* getGeometry() { return m_geometry.get(); }
        /* This function can be called on worker thread */
        void setGeometry(std::unique_ptr<Geometry> geometry);

    private:
        StaticMesh* m_parent = nullptr;
        u32 m_index;
        std::string m_name;
        std::unique_ptr<Geometry> m_geometry = nullptr;
    };

    class StaticMesh : public Asset
    {
    public:
        friend class Scene;
        friend class SceneRenderer;

        StaticMesh(const char* name, u32 numSubMeshes);
        ~StaticMesh();

        StaticSubMesh* operator[](u32 index) 
        { 
            assert(index < m_numSubMeshes);
            return m_subMeshes[index].get();
        }

        StaticSubMesh* operator[](i32 index) 
        { 
            assert(static_cast<u32>(index) < m_numSubMeshes);
            return m_subMeshes[index].get();
        }

        std::unique_ptr<StaticMeshInstance> createInstance(const Transform& localToWorld);
        void removeInstance(StaticMeshInstance* instance);

        u32 numSubMeshes() const { return m_numSubMeshes; }
    private:
        const u32 m_numSubMeshes;
        std::vector<std::unique_ptr<StaticSubMesh>> m_subMeshes;

        /* Instance Management */
        std::queue<i32> m_freeInstanceIDList;
        std::vector<StaticMeshInstance*> m_instances;
    };

    class Material;
    class MaterialInstance;
    class StaticSubMeshInstance;
    class Scene;

    class StaticMeshInstance
    {
    public:
        friend class Scene;

        StaticMeshInstance(StaticMesh* parent, i32 instanceID, const std::string& instanceKey, const Transform& localToWorld);
        ~StaticMeshInstance();

        void addToScene(Scene* scene);
        void onAddedToScene();
        void removeFromScene(Scene* scene);

        StaticMesh* getParentMesh() { return m_parent; }

        i32 getInstanceID() { return m_instanceID; }
        const std::string& getInstanceKey() { return m_instanceKey; }
        std::string getSubMeshInstanceKey(u32 subMeshIndex);

        const Transform& getLocalToWorldTransform() { return m_localToWorldTransform; }
        const glm::mat4& getLocalToWorldMatrix() { return m_localToWorldMatrix; }
        MaterialInstance* getMaterial(u32 slot);

        void setLocalToWorldTransform(const Transform& localToWorld);
        void setMaterial(u32 slot, MaterialInstance* mi);
    private:
        i32 m_instanceID = -1;
        std::string m_instanceKey;
        StaticMesh* m_parent = nullptr;
        Transform m_localToWorldTransform;
        glm::mat4 m_localToWorldMatrix;

        std::mutex m_materialMutex;
        // todo: this should really shader_ptr once the ref counting system is implemented for Assets 
        std::vector<MaterialInstance*> m_materials;

        /* read and write to these member should happen on the rendering thread */
        Scene* m_scene = nullptr;
    };
}