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

    virtual void customInitialize() override;
    virtual void customFinalize() override;
    virtual void customUpdate() override;
    virtual void customRender() override;

    void setupScene();

    // precomputation
    void precompute();

    // camera control
    void dispatchCameraCommand(struct CameraCommand& command);
    void orbitCamera(Camera& camera, double deltaX, double deltaY);
    void zoomCamera(Camera& camera, double dx, double dy);
    void rotateCamera(double deltaX, double deltaY);
    bool mouseOverUI();

    RayCastInfo castMouseRay(const glm::vec2& currentViewportPos, const glm::vec2& currentViewportSize);


    friend void Demo::mouseScrollWheelCallback(double xOffset, double yOffset);

    bool                         bOrbit;
    bool                         bRayCast;
    RayCastInfo                  m_mouseRayHitInfo;
    bool                         bPicking;
    double                       m_mouseCursorX, m_mouseCursorY;

    Cyan::TextureRenderable*               activeDebugViewTexture;
    u32                          m_currentDebugView;
private:
    Entity* m_selectedEntity;
    SceneComponent* m_selectedNode;
    u32 m_debugViewIndex;
    double m_lastFrameDurationInMs;
};