#pragma once

#include <vector>
#include <unordered_map>

#include "CyanApp.h"
#include "GraphicsSystem.h"
#include "Mesh.h"
#include "Geometry.h"
#include "imgui/imgui.h"
#include "CyanUI.h"
#include "GfxContext.h"
#include "CyanPathTracer.h"
#include "LightMap.h"

#define DEBUG_PROBE_TRACING 0
#define DEBUG_SSAO          1

namespace Pbr
{
    void mouseScrollWheelCallback(double xOffset, double yOffset);
};

class PbrApp : public CyanApp
{
public:
    PbrApp();
    ~PbrApp() { }
    virtual void init(int appWindowWidth, int appWindowHeight, glm::vec2 sceneViewportPos, glm::vec2 renderSize) override;

    static PbrApp* get();

    virtual void beginFrame() override;
    virtual void render() override;
    virtual void endFrame() override;
    virtual void run() override;
    virtual void shutDown() override;
    void buildFrame();
    void pathTraceScene(Scene* scene);

    // pre-computation
    void doPrecomputeWork();
    void bakeLightProbes(Cyan::ReflectionProbe* probe, u32 numProbes);

    // tick
    void update();
    void updateScene(Scene* scene);
    void updateMaterialData(Cyan::StandardPbrMaterial* matl);
    void debugRenderOctree();

    // camera control
    void dispatchCameraCommand(struct CameraCommand& command);
    void orbitCamera(Camera& camera, double deltaX, double deltaY);
    void zoomCamera(Camera& camera, double dx, double dy);
    void rotateCamera(double deltaX, double deltaY);
    void switchCamera();
    bool mouseOverUI();

    // main scene viewport
    void drawSceneViewport(); 
    // ui
    void drawEntityPanel();
    void drawDebugWindows();
    void drawLightingWidgets();
    void drawStats();
    void drawRenderSettings();
    void uiDrawEntityGraph(Entity* entity) ;
    RayCastInfo castMouseRay(const glm::vec2& currentViewportPos, const glm::vec2& currentViewportSize);

    // init
    void initScenes();
    void initUniforms();
    void initShaders();

    // manual scene initialization
    void initDemoScene00();
    void initSponzaScene();
    void initSkyBoxes();

    // material
    Cyan::StandardPbrMaterial* createStandardPbrMatlInstance(Scene* scene, Cyan::PbrMaterialParam params, bool isStatic);
    // manual custom entity creation
    void createHelmetInstance(Scene* scene);

    friend void Pbr::mouseScrollWheelCallback(double xOffset, double yOffset);

    bool                         bOrbit;
    bool                         bRayCast;
    RayCastInfo                  m_mouseRayHitInfo;
    bool                         bPicking;
    double m_mouseCursorX,       m_mouseCursorY;
    Cyan::IrradianceProbe*       m_irradianceProbe;
    Cyan::ReflectionProbe*       m_reflectionProbe;
    GLuint                       m_debugRayAtomicCounter;
    int                          m_debugProbeIndex;
    glm::vec3                    m_debugRayTracingNormal;

    Cyan::Texture*               activeDebugViewTexture;
    std::vector<Scene*>          m_scenes;
    RegularBuffer*               m_debugRayOctBuffer;
    RegularBuffer*               m_debugRayWorldBuffer;
    RegularBuffer*               m_debugRayBoundryBuffer;

    u32                          m_currentScene;
    Cyan::RenderTarget*          m_lightMapRenderTarget;
    Shader*                      m_lightMapShader;
    Cyan::LightMap               m_lightMap;      
    u32                          m_currentDebugView;
private:
    float m_sampleVertex[(64 + 1) * 4 * 2] = { };
    bool bRunning;
    u32 m_currentProbeIndex;

    Shader*                      m_rayTracingShader;
    Cyan::MaterialInstance*      m_rayTracingMatl;
    Shader*                      m_skyBoxShader;
    Cyan::MaterialInstance*      m_skyBoxMatl;
    Shader*                      m_skyShader;
    Cyan::MaterialInstance*      m_skyMatl;

    /* Buffers */
    RegularBuffer* m_pointLightsBuffer;
    RegularBuffer* m_dirLightsBuffer;

    // ui
    UI m_ui;
    Entity* m_selectedEntity;
    SceneNode* m_selectedNode;
    ImFont* m_font;
    u32 m_debugViewIndex;
    double m_lastFrameDurationInMs;
    bool bDisneyReparam;
    float m_directDiffuseSlider;
    float m_directSpecularSlider;
    float m_indirectDiffuseSlider;
    float m_indirectSpecularSlider;
    float m_directLightingSlider;
    float m_indirectLightingSlider;

    glm::vec3 m_debugRo;
    glm::vec3 m_debugRd;

    // debug parameters
    Line m_debugRay;
    Cyan::GraphicsSystem* m_graphicsSystem;
};