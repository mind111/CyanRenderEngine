#pragma once

#include "Texture.h"
#include "Entity.h"
#include "RenderTarget.h"

struct Scene;

namespace Cyan
{
    struct LightProbe
    {
        LightProbe(TextureRenderable* srcCubemapTexture);
        LightProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& resolution);
        ~LightProbe() { }
        virtual void initialize();
        virtual void captureScene();
        virtual void debugRender();

        Scene*               scene;
        glm::vec3            position;
        glm::vec2            resolution;
        TextureRenderable*             sceneCapture;
        MeshInstance*        debugSphereMesh;
    };

    struct IrradianceProbe : public LightProbe
    {
        IrradianceProbe(TextureRenderable* srcCubemapTexture, const glm::uvec2& irradianceRes);
        IrradianceProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution, const glm::uvec2& irradianceResolution);
        ~IrradianceProbe() { }
        virtual void debugRender() override;

        void initialize();
        void convolve();
        void build();
        void buildFromCubemap();

        static Shader* s_convolveIrradianceShader;
        static RegularBuffer*      m_rayBuffers;
        static const u32 kNumZenithSlice       = 32u;
        static const u32 kNumAzimuthalSlice    = 32u;
        static const u32 kNumRaysPerHemiSphere = 128u;

        glm::vec2         m_irradianceTextureRes;
        TextureRenderable*          m_convolvedIrradianceTexture;
    };

    struct IrradianceVolume
    {
        glm::vec3 m_volumePos;
        glm::vec3 m_volumeDimension;
        glm::vec3 m_probeSpacing;
        glm::vec3 m_lowerLeftCorner;
        std::vector<IrradianceProbe*> m_probes;
    };

    struct ReflectionProbe : public LightProbe
    {
        ReflectionProbe(TextureRenderable* srcCubemapTexture);
        ReflectionProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution);
        ~ReflectionProbe() { }

        static TextureRenderable* buildBRDFLookupTexture();

        static TextureRenderable* getBRDFLookupTexture()
        {
            return s_BRDFLookupTexture;
        }

        void initialize();
        virtual void debugRender() override;
        void convolve();
        void build();
        void buildFromCubemap();

        static TextureRenderable*      s_BRDFLookupTexture;
        static const u32     kNumMips = 11; 
        static Shader*       s_convolveReflectionShader;

        TextureRenderable*             m_convolvedReflectionTexture;
    };

    namespace LightProbeCameras
    {
        extern const glm::vec3 cameraFacingDirections[6];
        extern const glm::vec3 worldUps[6];
    }
}