#pragma once

#include "Common.h"
#include "Mesh.h"
#include "Light.h"
#include "LightProbe.h"

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
        } view;
        GLuint viewSsbo = -1;

        // lighting
        Skybox* skybox = nullptr;
        IrradianceProbe* irradianceProbe = nullptr;
        ReflectionProbe* reflectionProbe = nullptr;
        std::vector<DirLightGpuData>   directionalLights;
        std::vector<PointLightGpuData> pointLights;
        GLuint directionalLightSbo = -1;
        GLuint pointLightSbo = -1;

        // mesh instances
        std::vector<MeshInstance*> meshInstances;
        std::vector<glm::mat4> worldTransformMatrices;
        GLuint worldTransformMatrixSbo = -1;

        RenderableScene(Scene* scene, const SceneView& sceneView);
        ~RenderableScene()
        {
            GLuint buffers[4] = { viewSsbo, worldTransformMatrixSbo, directionalLightSbo, pointLightSbo };
            glDeleteBuffers(4, buffers);
        }

        /**
        * Submit rendering data to global gpu buffers
        */
        void submitSceneData(GfxContext* ctx);
    };
}
