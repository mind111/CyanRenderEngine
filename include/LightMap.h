#pragma once

#include <vector>
#include <atomic>

#include "glm.hpp"
#include "scene.h"
#include "SceneNode.h"
#include "Shader.h"
namespace Cyan
{
    // alignment
    struct LightMapTexelData
    {
        glm::vec4 worldPosition;
        glm::vec4 normal;
        glm::vec4 texCoord;
    };

    // todo: optimize Triangle::intersectRay() to improve baking speed
    // todo: figure out ways to combat seams
    // todo: save baked map to an image
    // todo: streamline lightmap baking code path
    struct LightMap
    {
        struct SceneNode* m_owner;
        struct RenderTarget* m_renderTarget;
        struct Texture* m_texAltas;
        Scene*          m_scene;
        RegularBuffer*  m_bakingGpuDataBuffer;
        f32*            m_pixels;
        std::vector<LightMapTexelData> m_bakingData;

        void setPixel(u32 px, u32 py, glm::vec3& color);
    };

    class LightMapManager
    {
    public:
        LightMapManager();
        ~LightMapManager();

        void renderMeshInstanceToLightMap(SceneNode* node, bool saveImage);
        void createLightMapForMeshInstance(Scene* scene, SceneNode* node);
        void bakeLightMap(Scene* scene, SceneNode* node, bool saveImage=false);
        void bakeSingleThread(LightMap* lightMap, u32 overlappedTexelCount);
        void bakeMultiThread(LightMap* lightMap, u32 overlappedTexelCount);
        static LightMapManager* getSingletonPtr();
        static std::atomic<u32> progressCounter;

        Shader* m_lightMapShader;
        MaterialInstance* m_lightMapMatl;
        GLuint texelCounterBuffer;
    private:
        static LightMapManager* m_singleton;
    };
}