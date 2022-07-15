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

        RenderableScene(const Scene* scene, const SceneView& sceneView, LinearAllocator& allocator);
        ~RenderableScene()
        { }

        /**
        * Submit rendering data to global gpu buffers
        */
        void submitSceneData(GfxContext* ctx);

        // scene reference
        const Scene* scene = nullptr;

        // view
        std::unique_ptr<ViewSsbo> viewSsboPtr = nullptr;

        // camera
        ICamera* camera = nullptr;

        // mesh instances
        std::vector<MeshInstance*> meshInstances;
        std::unique_ptr<TransformSsbo> transformSsboPtr = nullptr;

        // lights
        Skybox* skybox = nullptr;
        std::vector<std::shared_ptr<ILightRenderable>> lights;
        IrradianceProbe* irradianceProbe = nullptr;
        ReflectionProbe* reflectionProbe = nullptr;
    };
}
