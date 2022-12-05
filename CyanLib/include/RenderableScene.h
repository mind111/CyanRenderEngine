#pragma once

#include "Common.h"
#include "Mesh.h"
#include "Light.h"
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
    * should copying SceneRenderable be allowed? if so then basically every ssbo will be shared between the copies and need to be reference counted
    * to avoid a copy's destructor release shared ssbo, causing all sorts of artifact and weird bugs.
    */

    /**
    * A Scene representation that only contains renderable data.
    */
    struct SceneRenderable {
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

        struct Material {
            u64 diffuseMapHandle;
            u64 normalMapHandle;
            u64 metallicRoughnessMapHandle;
            u64 occlusionMapHandle;
            glm::vec4 kAlbedo;
            glm::vec4 kMetallicRoughness;
            glm::uvec4 flags;
        };

        using ViewBuffer = ShaderStorageBuffer<StaticSsboData<View>>;
        using TransformBuffer = ShaderStorageBuffer<DynamicSsboData<glm::mat4>>;
        using InstanceBuffer = ShaderStorageBuffer<DynamicSsboData<InstanceDesc>>;
        using MaterialBuffer = ShaderStorageBuffer<DynamicSsboData<Material>>;
        using DrawCallBuffer = ShaderStorageBuffer<DynamicSsboData<u32>>;

        friend class Renderer;

        SceneRenderable();
        ~SceneRenderable() { }
        SceneRenderable(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator);
        SceneRenderable(const SceneRenderable& scene);
        SceneRenderable& operator=(const SceneRenderable& src);
        static void clone(SceneRenderable& dst, const SceneRenderable& src);

        /**
        * Submit rendering data to global gpu buffers
        */
        void upload(GfxContext* ctx);

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
        Skybox* skybox = nullptr;
        std::vector<std::shared_ptr<ILightRenderable>> lights;
        IrradianceProbe* irradianceProbe = nullptr;
        ReflectionProbe* reflectionProbe = nullptr;

        static PackedGeometry* packedGeometry;
        std::unique_ptr<ViewBuffer> viewBuffer = nullptr;
        std::unique_ptr<TransformBuffer> transformBuffer = nullptr;
        std::unique_ptr<InstanceBuffer> instanceBuffer = nullptr;
        std::unique_ptr<DrawCallBuffer> drawCallBuffer = nullptr;
        std::unique_ptr<MaterialBuffer> materialBuffer = nullptr;
    private:
        u32 getMaterialID(MeshInstance* meshInstance, u32 submeshIndex);
    };
}
