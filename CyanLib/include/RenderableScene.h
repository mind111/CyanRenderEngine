#pragma once

#include "Common.h"
#include "Mesh.h"
#include "Light.h"
#include "LightProbe.h"
#include "ShaderStorageBuffer.h"
#include "DirectionalLight.h"
#include "LightRenderable.h"

namespace Cyan
{
    struct Scene;
    struct Skybox;
    struct SceneView;
    struct LinearAllocator;

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
    struct SceneRenderable
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

        using ViewSsbo = ShaderStorageBuffer<StaticSsboStruct<ViewData>>;
        using TransformSsbo = ShaderStorageBuffer<DynamicSsboStruct<glm::mat4>>;

        SceneRenderable(const Scene* scene, const SceneView& sceneView, LinearAllocator& allocator);
        ~SceneRenderable()
        { }

        /**
        * Submit rendering data to global gpu buffers
        */
        void submitSceneData(GfxContext* ctx);

        // scene reference
        const Scene* scene = nullptr;

        // view
        std::unique_ptr<ViewSsbo> viewSsboPtr = nullptr;

        // mesh instances
        std::vector<MeshInstance*> meshInstances;
        std::unique_ptr<TransformSsbo> transformSsboPtr;

        // lights
        Skybox* skybox = nullptr;
        std::vector<ILightRenderable*> lights;
        IrradianceProbe* irradianceProbe = nullptr;
        ReflectionProbe* reflectionProbe = nullptr;
    };
}
