#include "Mesh.h"
#include "SceneComponent.h"

namespace Cyan
{
    class Material;

    class StaticMeshComponent : public SceneComponent
    {
    public:
        StaticMeshComponent(const char* name, const Transform& localTransform, std::shared_ptr<StaticMesh> mesh);
        ~StaticMeshComponent() { }

        virtual void onTransformUpdated() override;

        StaticMesh::Instance* getMeshInstance() { return m_staticMeshInstance.get(); }
        void setMaterial(std::shared_ptr<Material> material, u32 index);
    private:
        std::shared_ptr<StaticMesh::Instance> m_staticMeshInstance = nullptr;
    };
}