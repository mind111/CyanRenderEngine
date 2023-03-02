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
    struct ICamera;
    struct GfxContext;
    struct ILightRenderable;
    struct DirectionalLight;
    struct SkyLight;
    struct PointLight;
    struct MeshInstance;

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

        void bind(GfxContext* ctx);

        // bounding box
        BoundingBox3D aabb;

        // camera
        struct Camera
        {
            Camera() = default;
            Camera(const struct PerspectiveCamera& inCamera);

            glm::vec3 eye;
            glm::vec3 lookAt;
            glm::vec3 right, forward, up;
            f32 fov, n, f, aspect;
            glm::mat4 view = glm::mat4(1.f);
            glm::mat4 projection = glm::mat4(1.f);
        } camera;

        // todo: these cpu side things should be maintained/cached in Scene class, this way, no need to 
        // loop through entity array to rebuild them from scratch
        // mesh instances
        std::vector<MeshInstance*> meshInstances;

        // lights
        DirectionalLight* sunLight = nullptr;
        std::vector<PointLight*> pointLights;
        std::vector<SkyLight*> skyLights;
        SkyLight* skyLight = nullptr;
        Skybox* skybox = nullptr;

        // gpu side buffers
        View view;
        std::vector<glm::mat4> transforms;
        std::vector<Instance> instances;
        std::vector<Material> materials;
        // use draw call id to get instance id
        std::vector<u32> instanceLUT;
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
