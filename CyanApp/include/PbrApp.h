#pragma once

#include <vector>
#include <unordered_map>

#include "CyanApp.h"
#include "GraphicsSystem.h"
#include "Mesh.h"
#include "Geometry.h"
#include "imgui/imgui.h"
#include "GfxContext.h"
#include "RayTracer.h"
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
    bool mouseOverUI();

    Cyan::RayCastInfo castMouseRay(const glm::vec2& currentViewportPos, const glm::vec2& currentViewportSize);

    friend void Demo::mouseScrollWheelCallback(double xOffset, double yOffset);

    bool bOrbit;
    bool bRayCast;
    Cyan::RayCastInfo m_mouseRayHitInfo;
    bool bPicking;
    double m_mouseCursorX, m_mouseCursorY;

    Cyan::Texture2DRenderable* activeDebugViewTexture;
    u32                          m_currentDebugView;
private:
    Cyan::Entity* m_selectedEntity;
    Cyan::SceneComponent* m_selectedNode;
    u32 m_debugViewIndex;
    double m_lastFrameDurationInMs;
};