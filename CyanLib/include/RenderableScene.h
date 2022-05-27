#pragma once

#include "Common.h"
#include "Mesh.h"
#include "Light.h"
#include "LightProbe.h"
#include "ShaderStorageBuffer.h"

struct Scene;

namespace Cyan
{
    struct Skybox;
    struct SceneView;

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
            glm::mat4 sunLightView;
            glm::mat4 sunShadowProjections[4];
            i32 numDirLights;
            i32 numPointLights;
            f32 ssao;
            f32 dummy;
        };

        using ViewSsbo = ShaderStorageBuffer<f32, ViewData>;
        using DirectionalLightSsbo = ShaderStorageBuffer<DirLightGpuData>;
        using PointLightSsbo = ShaderStorageBuffer<PointLightGpuData>;
        using TransformSsbo = ShaderStorageBuffer<glm::mat4>;

        // view
        std::unique_ptr<ViewSsbo> viewSsboPtr = nullptr;

        // lighting
        Skybox* skybox = nullptr;
        IrradianceProbe* irradianceProbe = nullptr;
        ReflectionProbe* reflectionProbe = nullptr;
        std::unique_ptr<DirectionalLightSsbo> directionalLightSsboPtr = nullptr;
        std::unique_ptr<PointLightSsbo> pointLightSsboPtr = nullptr;

        // mesh instances
        std::vector<MeshInstance*> meshInstances;
        std::unique_ptr<TransformSsbo> transformSsboPtr;

        RenderableScene(Scene* scene, const SceneView& sceneView);
        ~RenderableScene()
        { }

        /**
        * Submit rendering data to global gpu buffers
        */
        void submitSceneData(GfxContext* ctx);
    };
}
