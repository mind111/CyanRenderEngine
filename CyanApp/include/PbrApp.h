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
#include "SkyBox.h"

#define DEBUG_PROBE_TRACING 0
#define DEBUG_SSAO          1

namespace Demo
{
    void mouseScrollWheelCallback(double xOffset, double yOffset);
};

class DemoApp : public CyanApp
{
public:
    DemoApp();
    ~DemoApp() { }
    virtual void initialize(int appWindowWidth, int appWindowHeight, glm::vec2 sceneViewportPos, glm::vec2 renderSize) override;

    static DemoApp* get();

    virtual void render() override;
    virtual void run() override;
    virtual void shutDown() override;
    void buildFrame();

    // precomputation
    void precompute();

    // tick
    void update();
    void updateScene(Scene* scene);
    void updateMaterialData(Cyan::StandardPbrMaterial* matl);
    void debugIrradianceCache();

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
    void drawStats();
    void drawEntityPanel();
    void drawDebugWindows();
    void drawLightingWidgets();
    void drawRenderSettings();
    void uiDrawEntityGraph(Entity* entity) ;
    RayCastInfo castMouseRay(const glm::vec2& currentViewportPos, const glm::vec2& currentViewportSize);

    // manual scene initialization
    void initDemoScene00();
    void initSponzaScene();

    // material
    Cyan::StandardPbrMaterial* createStandardPbrMatlInstance(Scene* scene, Cyan::PbrMaterialParam params, bool isStatic);
    // manual custom entity creation
    void createHelmetInstance(Scene* scene);

    friend void Demo::mouseScrollWheelCallback(double xOffset, double yOffset);

    bool                         bOrbit;
    bool                         bRayCast;
    RayCastInfo                  m_mouseRayHitInfo;
    bool                         bPicking;
    double                       m_mouseCursorX, m_mouseCursorY;

    Cyan::Texture*               activeDebugViewTexture;
    std::vector<Scene*>          m_scenes;

    u32                          m_currentScene;
    u32                          m_currentDebugView;
    std::vector<Line>            m_debugLines;
private:
    float m_sampleVertex[(64 + 1) * 4 * 2] = { };
    bool bRunning;

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

    // debug parameters
    Line m_debugRay;
    Cyan::GraphicsSystem* m_graphicsSystem;
};