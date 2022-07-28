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

    struct RenderableCamera
    {
        glm::mat4 view;
        glm::mat4 projection;
    };

    /**
    * A Scene representation that only contains renderable data.
    *     * this is meant to decouple rendering required scene data from the main Scene class 
    *     * hopefully can save the renderer from worrying about non-rendering related data
    *     * contains a scene's global data in gpu compatible format including
    *         * camera related view data
    *         * lights
    *         * instance transforms
    *         * (todo) material buffers
    */
    struct RenderableScene
    {
        // view data
        struct ViewData
        {
            glm::mat4 view;
            glm::mat4 projection;
            f32 ssao;
            glm::vec3 dummy;
        };

        using ViewSsbo = ShaderStorageBuffer<StaticSsboStruct<ViewData>>;
        using TransformSsbo = ShaderStorageBuffer<DynamicSsboStruct<glm::mat4>>;

        RenderableScene(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator);
        ~RenderableScene()
        { 
            glUnmapNamedBuffer(indirectDrawBuffer.buffer);
            glDeleteBuffers(1, &indirectDrawBuffer.buffer);
        }

        void setView(const glm::mat4& view) 
        {
            camera.view = view;
        }
        void setProjection(const glm::mat4& projection) 
        {
            camera.projection = projection;
        }

        /**
        * Submit rendering data to global gpu buffers
        */
        void submitSceneData(GfxContext* ctx);

        // view
        std::shared_ptr<ViewSsbo> viewSsboPtr = nullptr;

        // camera
        RenderableCamera camera;

        // mesh instances
        std::vector<MeshInstance*> meshInstances;
        std::shared_ptr<TransformSsbo> transformSsboPtr = nullptr;

        // lights
        Skybox* skybox = nullptr;
        std::vector<std::shared_ptr<ILightRenderable>> lights;
        IrradianceProbe* irradianceProbe = nullptr;
        ReflectionProbe* reflectionProbe = nullptr;

        struct Vertex
        {
            glm::vec4 pos;
            glm::vec4 normal;
            glm::vec4 tangent;
            glm::vec4 texCoord;
        };
        // unified geometry data
        ShaderStorageBuffer<DynamicSsboStruct<Vertex>> unifiedVertexBuffer;
        ShaderStorageBuffer<DynamicSsboStruct<u32>> unifiedIndexBuffer;

        struct IndirecDraw
        {
            GLuint buffer = -1;
            u32 sizeInBytes = 1024 * 1024 * 32;
            void* data = nullptr;
        } indirectDrawBuffer;

        struct SubmeshDesc
        {
            u32 baseVertex = 0;
            u32 baseIndex = 0;
            u32 numVertices = 0;
            u32 numIndices = 0;
        };
        ShaderStorageBuffer<DynamicSsboStruct<SubmeshDesc>> submeshes;

        // todo: implement this
        struct MaterialDesc
        {
            // offsets into the global texture array
            u32 albedoMap;
            u32 normalMap;
            u32 roughnessMap;
            u32 metallicMap;
            u32 metallicRoughnessMap;
            u32 occlusionMap;
            glm::vec3 albedo;
        };

        struct InstanceDesc
        {
            u32 submesh = 0;
            u32 material = 0;
            u32 transform = 0;
            u32 padding = 0;
        };
        ShaderStorageBuffer<DynamicSsboStruct<InstanceDesc>> instances;

        // map sub-draw id to instance id 
        ShaderStorageBuffer<DynamicSsboStruct<u32>> drawCalls;
    };
}
