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

    // is it better to define
    /**
    * A Scene representation that only contains renderable data.
    */
    struct RenderableScene 
    {
        struct View 
        {
            glm::mat4 view;
            glm::mat4 projection;
            f32 ssao;
            glm::vec3 dummy;
        };

        struct InstanceDesc 
        {
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

        virtual void clone(const RenderableScene& src);
        virtual void onClone(const RenderableScene& src) { }

        /**
        * Submit rendering data to global gpu buffers
        */
        virtual void upload();
        // extension point for derived class to do whatever custom behavior they need to
        virtual void onUpload() { };

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

        // static PackedGeometry* packedGeometry;
        std::unique_ptr<ViewBuffer> viewBuffer = nullptr;
        std::unique_ptr<TransformBuffer> transformBuffer = nullptr;
        std::unique_ptr<InstanceBuffer> instanceBuffer = nullptr;
        std::unique_ptr<DrawCallBuffer> drawCallBuffer = nullptr;
    };

    struct RenderableSceneBindless : public RenderableScene
    {
        RenderableSceneBindless(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator);
        RenderableSceneBindless(const RenderableScene& src);
        ~RenderableSceneBindless() { }

        virtual void onClone(const RenderableScene& src) override;
        virtual void onUpload() override;

        std::unique_ptr<MaterialBuffer> materialBuffer = nullptr;
    };

    struct RenderableSceneTextureAtlas : public RenderableScene
    {
        RenderableSceneTextureAtlas(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator);
        RenderableSceneTextureAtlas(const RenderableScene& src);
        ~RenderableSceneTextureAtlas() { }

        virtual void onClone(const RenderableScene& src) override;
        virtual void onUpload() override;

        using SubtextureBuffer = ShaderStorageBuffer<DynamicSsboData<Texture2DAtlas::Subtexture>>;
        using ImageTransformBuffer = ShaderStorageBuffer<DynamicSsboData<Texture2DAtlas::ImageTransform>>;

        std::unique_ptr<SubtextureBuffer> subtextureBufferR8 = nullptr;
        std::unique_ptr<SubtextureBuffer> subtextureBufferRGB8 = nullptr;
        std::unique_ptr<SubtextureBuffer> subtextureBufferRGBA8 = nullptr;
        std::unique_ptr<ImageTransformBuffer> imageTransformBufferR8 = nullptr;
        std::unique_ptr<ImageTransformBuffer> imageTransformBufferRGB8 = nullptr;
        std::unique_ptr<ImageTransformBuffer> imageTransformBufferRGBA8 = nullptr;
    };
}
