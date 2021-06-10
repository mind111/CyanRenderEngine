#pragma once

#include <vector>

#include "CyanApp.h"
#include "Scene.h"
#include "Mesh.h"
#include "Geometry.h"

struct IBLAssets
{
    Cyan::Texture* m_diffuse;
    Cyan::Texture* m_specular;
    Cyan::Texture* m_brdfIntegral;
};

class PbrSpheresSample : public CyanApp
{
public:
    PbrSpheresSample();
    ~PbrSpheresSample() { }
    virtual void init(int appWindowWidth, int appWindowHeight) override;

    static PbrSpheresSample* get();

    virtual void beginFrame() override;
    virtual void render() override;
    virtual void endFrame() override;

    virtual void run() override;
    virtual void shutDown() override;

    void update();

    void orbitCamera(double deltaX, double deltaY);
    void rotateCamera(double deltaX, double deltaY);

    bool bOrbit;

private:
    float m_sampleVertex[(64 + 1) * 4 * 2] = { };
    
    bool bRunning;
    u32 currentScene;
    Scene* m_scene;

    /* Shaders */
    Shader* m_pbrShader;
    Shader* m_envmapShader;

    // Materials
    Cyan::MaterialInstance* m_envmapMatl;
    Cyan::MaterialInstance* m_sphereMatls[36];

    /* Buffers */
    RegularBuffer* m_pointLightsBuffer;
    RegularBuffer* m_dirLightsBuffer;

    /* Textures */
    Cyan::Texture* m_rawEnvmap;
    Cyan::Texture* m_envmap;
    Cyan::Texture* m_sphereAlbedo;
    IBLAssets m_iblAssets;

    /* Uniforms */
    Uniform* u_numPointLights;
    Uniform* u_numDirLights;
    Uniform* u_cameraView;
    Uniform* u_cameraProjection;
    Uniform* u_kDiffuse;
    Uniform* u_kSpecular;
    Uniform* u_hasNormalMap;
    Uniform* u_hasAoMap;
};