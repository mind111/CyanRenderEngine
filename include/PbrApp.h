#pragma once

#include <vector>

#include "CyanApp.h"
#include "Scene.h"
#include "Mesh.h"

// TODO: Can also let CyanApp define a basic beginFrame(), endFrame() where it calls customBeginFrame(), and customEndFrame(),
// and child class override customBegin() and customEnd() to do each application specific stuffs
class PbrApp : public CyanApp
{
public:
    PbrApp();
    ~PbrApp() { }
    virtual void init(int appWindowWidth, int appWindowHeight) override;

    static PbrApp* get();

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
    u32 entityOnFocusIdx;
    u32 currentScene;
    std::vector<Scene*> m_scenes;

    Camera m_genEnvmapCamera;

    // Materials
    Cyan::Material* m_helmetMatl;
    Cyan::Material* m_envmapMatl;

    /* Shaders */
    Shader* m_pbrShader;
    Shader* m_envmapShader;

    /* Buffers */
    RegularBuffer* m_pointLightsBuffer;
    RegularBuffer* m_dirLightsBuffer;

    /* Textures */
    Cyan::Texture* m_rawEnvmap;

    /* Uniforms */
    Uniform* u_numPointLights;
    Uniform* u_numDirLights;
    Uniform* u_cameraView;
    Uniform* u_cameraProjection;
    Uniform* u_kDiffuse;
    Uniform* u_kSpecular;
    Uniform* u_hasNormalMap;
    Uniform* u_hasAoMap;

    Cyan::Texture* m_envmap;
    Cyan::Texture* m_diffuseIrradianceMap;

    GLuint m_envmapVbo, m_envmapVao;
    GLuint m_linesVbo, m_linesVao;

    // debugging purpose
    Uniform* u_envmapSampler;
    Uniform* u_projection_dbg;
    Uniform* u_view_dbg;
    Shader* shader_dbg;
    Cyan::RenderTarget* rt_dbg;
    Cyan::Texture* equirectMap;
    Cyan::Texture* envmap_dbg;
};