#include <unordered_map>

#include "Lights.h"
#include "RenderableScene.h"
#include "Camera.h"
#include "Scene.h"
#include "CyanRenderer.h"
#include "LightComponents.h"
#include "GpuLights.h"
#include "AssetManager.h"
#include "Mesh.h"
#include "Material.h"

namespace Cyan
{
    RenderableScene::Camera::Camera(const PerspectiveCamera& inCamera)
    {
        eye = inCamera.position;
        lookAt = inCamera.lookAt;
        right = inCamera.right();
        forward = inCamera.forward();
        up = inCamera.up();
        n = inCamera.n;
        f = inCamera.f;
        fov = inCamera.fov;
        aspect = inCamera.aspectRatio;
        view = inCamera.view();
        projection = inCamera.projection();
    }

    RenderableScene::RenderableScene(const Scene* inScene, const SceneView& sceneView)
    {
        aabb = inScene->m_aabb;

        // todo: make this work with orthographic camera as well
        camera = Camera(sceneView.camera);

        // todo: cpu side culling can be done here, cull the instances within the scene to view frustum ...?
        // build list of mesh instances, transforms
        for (auto staticMesh : inScene->m_staticMeshes)
        {
            glm::mat4 transformMat = staticMesh->getWorldTransformMatrix();
            transforms.push_back(transformMat);
            meshInstances.push_back(staticMesh->getMeshInstance());
        }

        // build list of lights
        for (auto lightComponent : inScene->m_lightComponents)
        {
            if (std::string(lightComponent->getTag()) == std::string("DirectionalLightComponent"))
            {
                auto directionalLightComponent = static_cast<DirectionalLightComponent*>(lightComponent);
                sunLight = directionalLightComponent->directionalLight.get();
            }
            else if (std::string(lightComponent->getTag()) == std::string("PointLightComponent"))
            {

            }
        }

        // todo: these two need to be turned into entities
        skybox = inScene->skybox;
        skyLight = inScene->skyLight;

        // build instance descriptors
        std::unordered_map<std::string, i32> materialMap;
        for (u32 i = 0; i < meshInstances.size(); ++i)
        {
            auto mesh = meshInstances[i]->mesh;
            for (u32 sm = 0; sm < mesh->numSubmeshes(); ++sm)
            {
                Instance instance = { };
                instance.transform = i;

                auto submesh = mesh->getSubmesh(sm);
                if (submesh->bInitialized)
                {
                    // todo: properly handle other types of geometries
                    if (dynamic_cast<Triangles*>(submesh->geometry.get()))
                    {
                        instance.submesh = submesh->index;
                    }
                }

                // todo: properly handle material
                auto material = meshInstances[i]->getMaterial(sm);
                auto entry = materialMap.find(material->name);
                if (entry != materialMap.end())
                {
                    instance.material = entry->second;
                }
                else
                {
                    materials.emplace_back(material->buildGpuMaterial());
                    u32 materialIndex = materials.size() - 1;
                    materialMap.insert({ material->name, materialIndex });
                    instance.material = materialIndex;
                }

                instances.push_back(instance);
            }
        }

        // build the lookup table that maps draw call index to instance index for each draw
        if (!instances.empty())
        {
            struct InstanceDescSortKey
            {
                inline bool operator() (const Instance& lhs, const Instance& rhs)
                {
                    return (lhs.submesh < rhs.submesh);
                }
            };
            // todo: this sort maybe too slow at some point if the number instances get large
            // sort the instance buffer to group submesh instances that share the same submesh right next to each other
            std::sort(instances.begin(), instances.end(), InstanceDescSortKey());

            instanceLUT.push_back(0);
            auto& prev = instances[0];
            for (i32 i = 1; i < instances.size(); ++i)
            {
                if (instances[i].submesh != prev.submesh)
                {
                    instanceLUT.push_back(i);
                }
                prev = instances[i];
            }
            instanceLUT.push_back(instances.size());
        }

        // create gpu side buffers to host cpu side data
        viewBuffer = std::make_unique<ShaderStorageBuffer>("ViewBuffer", sizeof(View));
        viewBuffer->write(view, 0);
        if (!instances.empty())
        {
            assert(transforms.size() == instances.size());
            transformBuffer = std::make_unique<ShaderStorageBuffer>("TransformBuffer", sizeOfVector(transforms));
            transformBuffer->write(transforms, 0);
            instanceBuffer = std::make_unique<ShaderStorageBuffer>("InstanceBuffer", sizeOfVector(instances));
            instanceBuffer->write(instances, 0);
            instanceLUTBuffer = std::make_unique<ShaderStorageBuffer>("InstanceLUTBuffer", sizeOfVector(instanceLUT));
            instanceLUTBuffer->write(instanceLUT, 0);
        }
        if (auto csmDirectionalLight = dynamic_cast<CSMDirectionalLight*>(sunLight))
        {
            directionalLightBuffer = std::make_unique<ShaderStorageBuffer>("DirectionalLightBuffer", sizeof(GpuCSMDirectionalLight));
            directionalLightBuffer->write(csmDirectionalLight->buildGpuLight(), 0);
        }

        std::vector<IndirectDrawArrayCommand> indirectDrawCommands;
        for (i32 draw = 0; draw < instanceLUT.size() - 1; ++draw)
        {
            IndirectDrawArrayCommand command = { };
            command.first = 0;
            u32 instance = instanceLUT[draw];
            u32 submesh = instances[instance].submesh;
            command.count = StaticMesh::g_submeshes[submesh].numIndices;
            command.instanceCount = instanceLUT[draw + 1] - instanceLUT[draw];
            command.baseInstance = 0;
            indirectDrawCommands.push_back(command);
        }
        indirectDrawBuffer = std::make_unique<IndirectDrawBuffer>(indirectDrawCommands);
    }

    /**
    * the copy constructor performs a deep copy instead of simply copying over
    * pointers / references
    */
    RenderableScene::RenderableScene(const RenderableScene& src)
    {
        aabb = src.aabb;
        camera = src.camera;
        meshInstances = src.meshInstances;
        skybox = src.skybox;
        skyLight = src.skyLight;
        sunLight = src.sunLight;
        view = src.view;
        transforms = src.transforms;
        instances = src.instances;
        instanceLUT = src.instanceLUT;
        viewBuffer.reset(new ShaderStorageBuffer(*src.viewBuffer));
        transformBuffer.reset(new ShaderStorageBuffer(*src.transformBuffer));
        instanceBuffer.reset(new ShaderStorageBuffer(*src.instanceBuffer));
        instanceLUTBuffer.reset(new ShaderStorageBuffer(*src.instanceLUTBuffer));
        directionalLightBuffer.reset(new ShaderStorageBuffer(*src.directionalLightBuffer));
        indirectDrawBuffer.reset(new IndirectDrawBuffer(*src.indirectDrawBuffer));
    }

    RenderableScene& RenderableScene::operator=(const RenderableScene& src)
    {
        aabb = src.aabb;
        camera = src.camera;
        meshInstances = src.meshInstances;
        skybox = src.skybox;
        skyLight = src.skyLight;
        sunLight = src.sunLight;
        view = src.view;
        transforms = src.transforms;
        instances = src.instances;
        instanceLUT = src.instanceLUT;
        viewBuffer.reset(new ShaderStorageBuffer(*src.viewBuffer));
        transformBuffer.reset(new ShaderStorageBuffer(*src.transformBuffer));
        instanceBuffer.reset(new ShaderStorageBuffer(*src.instanceBuffer));
        instanceLUTBuffer.reset(new ShaderStorageBuffer(*src.instanceLUTBuffer));
        directionalLightBuffer.reset(new ShaderStorageBuffer(*src.directionalLightBuffer));
        indirectDrawBuffer.reset(new IndirectDrawBuffer(*src.indirectDrawBuffer));
        return *this;
    }

    void RenderableScene::bind(GfxContext* ctx)
    {
        // todo: implement this
        ctx->setShaderStorageBuffer(viewBuffer.get());
        ctx->setShaderStorageBuffer(transformBuffer.get());
        ctx->setShaderStorageBuffer(materialBuffer.get());
        ctx->setShaderStorageBuffer(instanceBuffer.get());
        ctx->setShaderStorageBuffer(directionalLightBuffer.get());
        ctx->setShaderStorageBuffer(instanceLUTBuffer.get());
    }
}
