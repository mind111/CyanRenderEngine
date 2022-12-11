#pragma once

#include "Common.h"
#include "Mesh.h"
#include "GpuLights.h"
#include "LightProbe.h"
#include "ShaderStorageBuffer.h"

namespace Cyan
{
    struct Scene;
    struct Skybox;
    struct SceneView;
    struct LinearAllocator;
    struct ICamera;
    struct GfxContext;
    struct ILightRenderable;
    struct DirectionalLight;
    struct SkyLight;
    struct PointLight;

    struct PackedGeometry
    {
        PackedGeometry() {}
        PackedGeometry(const Scene& scene);

        std::vector<Mesh*> meshes;
        std::unordered_map<std::string, Mesh*> meshMap;
        std::unordered_map<std::string, u32> submeshMap;

        struct Vertex
        {
            glm::vec4 pos;
            glm::vec4 normal;
            glm::vec4 tangent;
            glm::vec4 texCoord;
        };
        // unified geometry data
        ShaderStorageBuffer<DynamicSsboData<Vertex>> vertexBuffer;
        ShaderStorageBuffer<DynamicSsboData<u32>> indexBuffer;

        struct SubmeshDesc
        {
            u32 baseVertex = 0;
            u32 baseIndex = 0;
            u32 numVertices = 0;
            u32 numIndices = 0;
        };
        ShaderStorageBuffer<DynamicSsboData<SubmeshDesc>> submeshes;
    };

    /** todo:
    * should copying RenderableScene be allowed? if so then basically every ssbo will be shared between the copies and need to be reference counted
    * to avoid a copy's destructor release shared ssbo, causing all sorts of artifact and weird bugs.
    */

    /**
    * A Scene representation that only contains renderable data.
    */
    struct RenderableScene {
        struct View {
            glm::mat4 view;
            glm::mat4 projection;
            f32 ssao;
            glm::vec3 dummy;
        };

        struct InstanceDesc {
            u32 submesh = 0;
            u32 material = 0;
            u32 transform = 0;
            u32 padding = 0;
        };

        using ViewBuffer = ShaderStorageBuffer<StaticSsboData<View>>;
        using TransformBuffer = ShaderStorageBuffer<DynamicSsboData<glm::mat4>>;
        using InstanceBuffer = ShaderStorageBuffer<DynamicSsboData<InstanceDesc>>;
        using MaterialBuffer = ShaderStorageBuffer<DynamicSsboData<GpuMaterial>>;
        using DrawCallBuffer = ShaderStorageBuffer<DynamicSsboData<u32>>;
        using DirectionalLightBuffer = ShaderStorageBuffer<DynamicSsboData<GpuCSMDirectionalLight>>;
        using PointLightBuffer = ShaderStorageBuffer<DynamicSsboData<GpuPointLight>>;
        using SkyLightBuffer = ShaderStorageBuffer<DynamicSsboData<GpuSkyLight>>;

        friend class Renderer;

        RenderableScene();
        ~RenderableScene() { }
        RenderableScene(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator);
        RenderableScene(const RenderableScene& scene);
        RenderableScene& operator=(const RenderableScene& src);
        static void clone(RenderableScene& dst, const RenderableScene& src);

        /**
        * Submit rendering data to global gpu buffers
        */
        void upload();

        // bounding box
        BoundingBox3D aabb;

        // camera
        struct Camera
        {
            glm::vec3 eye;
            glm::vec3 lookAt;
            glm::vec3 right, forward, up;
            f32 fov, n, f, aspect;
            glm::mat4 view = glm::mat4(1.f);
            glm::mat4 projection = glm::mat4(1.f);
        } camera;

        // mesh instances
        std::vector<MeshInstance*> meshInstances;

        // lights
        std::vector<DirectionalLight*> directionalLights;
        std::vector<PointLight*> pointLights;
        std::vector<SkyLight*> skyLights;
        std::unique_ptr<DirectionalLightBuffer> directionalLightBuffer = nullptr;
        SkyLight* skyLight = nullptr;

        // skybox
        Skybox* skybox = nullptr;

        static PackedGeometry* packedGeometry;
        std::unique_ptr<ViewBuffer> viewBuffer = nullptr;
        std::unique_ptr<TransformBuffer> transformBuffer = nullptr;
        std::unique_ptr<InstanceBuffer> instanceBuffer = nullptr;
        std::unique_ptr<MaterialBuffer> materialBuffer = nullptr;
        std::unique_ptr<DrawCallBuffer> drawCallBuffer = nullptr;

    private:
        u32 getMaterialID(MeshInstance* meshInstance, u32 submeshIndex);
    };
}
