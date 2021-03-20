#pragma once

#include <vector>

#include "CyanApp.h"
#include "Scene.h"

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
    
    PbrShaderVars m_shaderVars;
    PbrShader* m_shader;
    bool bRunning;
    u32 entityOnFocusIdx;
    u32 currentScene;
    std::vector<Scene*> m_scenes;

    Camera m_genEnvmapCamera;

    /* Shader */
    Shader* m_genEnvmapShader;
    Shader* m_genIrradianceShader;
    Shader* m_envmapShader;
    Shader* m_lineShader;

    /*
    GenEnvmapShader* m_genEnvmapShader;
    GenIrradianceShader* m_genIrradianceShader;
    EnvmapShader* m_envmapShader;
    LineShader* m_lineShader;
    */

    GLuint m_envmapVbo, m_envmapVao;
    GLuint m_rawEnvmap, m_envmap;
    GLuint m_diffuseIrradianceMap;
    GLuint m_linesVbo, m_linesVao;
    MeshGroup* m_cubeMesh;
};