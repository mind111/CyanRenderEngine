#pragma once

#include <vector>
#include <unordered_map>

#include "CyanApp.h"
#include "GraphicsSystem.h"
#include "Mesh.h"
#include "Geometry.h"
#include "imgui/imgui.h"
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

class DemoApp : public Cyan::DefaultApp
{
public:
    DemoApp(u32 appWindowWidth, u32 appWindowHeight);
    ~DemoApp() { }

    static DemoApp* get();

    virtual void customInitialize() override;
    virtual void customFinalize() override;
    virtual void customUpdate() override;
    virtual void customRender() override;

    // precomputation
    void precompute();
    void debugIrradianceCache();

    // camera control
    void dispatchCameraCommand(struct CameraCommand& command);
    void orbitCamera(Camera& camera, double deltaX, double deltaY);
    void zoomCamera(Camera& camera, double dx, double dy);
    void rotateCamera(double deltaX, double deltaY);
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

    // manual custom entity creation
    void createHelmetInstance(Scene* scene);

    friend void Demo::mouseScrollWheelCallback(double xOffset, double yOffset);

    bool                         bOrbit;
    bool                         bRayCast;
    RayCastInfo                  m_mouseRayHitInfo;
    bool                         bPicking;
    double                       m_mouseCursorX, m_mouseCursorY;

    Cyan::Texture*               activeDebugViewTexture;
    u32                          m_currentDebugView;
private:
    Entity* m_selectedEntity;
    SceneNode* m_selectedNode;
    u32 m_debugViewIndex;
    double m_lastFrameDurationInMs;
    bool bDisneyReparam;
    float m_directDiffuseSlider;
    float m_directSpecularSlider;
    float m_indirectDiffuseSlider;
    float m_indirectSpecularSlider;
    float m_directLightingSlider;
    float m_indirectLightingSlider;
};