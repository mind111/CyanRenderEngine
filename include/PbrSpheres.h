#if 0
#pragma once

#include <vector>

#include "CyanApp.h"
#include "Scene.h"
#include "Mesh.h"
#include "Geometry.h"

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

    Camera m_genEnvmapCamera;

    /* Shaders */
    Shader* m_pbrShader;
    Shader* m_envmapShader;

    // Materials
    Cyan::MaterialInstance* m_envmapMatl;
    Cyan::MaterialInstance* m_sphereMatl;

    /* Buffers */
    RegularBuffer* m_pointLightsBuffer;
    RegularBuffer* m_dirLightsBuffer;

    /* Textures */
    Cyan::Texture* m_rawEnvmap;
    Cyan::Texture* m_envmap;

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
#endif