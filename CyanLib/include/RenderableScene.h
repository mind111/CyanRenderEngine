#pragma once

#include "Common.h"
#include "GpuLights.h"
#include "LightProbe.h"
#include "ShaderStorageBuffer.h"
#include "Geometry.h"

namespace Cyan
{
    struct Scene;
    struct Skybox;
    struct SceneView;
    struct LinearAllocator;
    struct Camera;
    struct GfxContext;
    struct ILightRenderable;
    struct DirectionalLight;
    struct SkyLight;
    struct PointLight;
    struct MeshInstance;
    struct VertexArray;

    struct IndirectDrawArrayCommand
    {
        u32  count;
        u32  instanceCount;
        u32  first;
        i32  baseInstance;
    };

    struct IndirectDrawBuffer : public GpuResource
    {
        IndirectDrawBuffer(const std::vector<IndirectDrawArrayCommand>& inCommands)
            : commands(inCommands)
        {
            glCreateBuffers(1, &glObject);
            glNamedBufferData(getGpuResource(), sizeOfVector(commands), commands.data(), GL_STATIC_DRAW);
        }

        // performs a deep copy of gpu buffer data
        IndirectDrawBuffer(const IndirectDrawBuffer& src)
        {
            commands = src.commands;
            glCreateBuffers(1, &glObject);
            glNamedBufferData(getGpuResource(), sizeOfVector(commands), commands.data(), GL_STATIC_DRAW);
        }

        ~IndirectDrawBuffer() 
        { 
            GLuint buffers[] = { getGpuResource() };
            glDeleteBuffers(1, buffers);
        }

        void bind()
        {
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, getGpuResource());
        }

        void unbind()
        {
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
        }

        u32 numDrawcalls()
        {
            return commands.size();
        }

        std::vector<IndirectDrawArrayCommand> commands;
    };

    /**
    * An immutable scene representation that only contains renderable data.
    */
    struct RenderableScene 
    {
        struct View 
        {
            glm::mat4 view;
            glm::mat4 projection;
        };

        // todo: regardless of the material implementation, either using bindless texture or texture atlas, their GpuData representation should
        // be identical
        using TextureHandle = u64;
        struct Material
        {
            TextureHandle albedoMap;
            TextureHandle normalMap;
            TextureHandle metallicRoughnessMap;
            TextureHandle emissiveMap;
            TextureHandle occlusionMap;
            TextureHandle padding;
            glm::vec4 albedo;
            f32 metallic;
            f32 roughness;
            f32 emissive;
            u32 flag;
        };

        struct Instance 
        {
            u32 submesh = 0;
            u32 material = 0;
            u32 transform = 0;
            u32 padding = 0;
        };

        friend class Renderer;

        RenderableScene() = default;
        RenderableScene(const Scene* inScene, const SceneView& sceneView);
        RenderableScene(const RenderableScene& scene);
        RenderableScene& operator=(const RenderableScene& src);

        ~RenderableScene() { }

        void bind(GfxContext* ctx) const;

        // bounding box
        BoundingBox3D aabb;

        // view
        View view;

        // instances
        std::vector<MeshInstance*> meshInstances;
        std::vector<glm::mat4> transforms;
        std::vector<Instance> instances;
        std::vector<Material> materials;
        // use draw call id to get instance id
        std::vector<u32> instanceLUT;

        // lights
        DirectionalLight* sunLight = nullptr;
        std::vector<PointLight*> pointLights;
        SkyLight* skyLight = nullptr;
        Skybox* skybox = nullptr;

        // dummy vao to suppress an error during multiDrawIndirect
        VertexArray* dummyVertexArray = nullptr;
        // gpu side buffers
        std::unique_ptr<ShaderStorageBuffer> viewBuffer = nullptr;
        std::unique_ptr<ShaderStorageBuffer> transformBuffer = nullptr;
        std::unique_ptr<ShaderStorageBuffer> materialBuffer = nullptr;
        std::unique_ptr<ShaderStorageBuffer> instanceBuffer = nullptr;
        std::unique_ptr<ShaderStorageBuffer> instanceLUTBuffer = nullptr;
        // todo: convert this to a general light buffer where all light data is stored
        std::unique_ptr<ShaderStorageBuffer> directionalLightBuffer = nullptr;
        std::unique_ptr<IndirectDrawBuffer> indirectDrawBuffer = nullptr;
    };
}
