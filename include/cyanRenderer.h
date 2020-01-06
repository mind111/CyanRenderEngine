#pragma once
#include "glew.h"
#include "shader.h"
#include "scene.h"
#include "camera.h"

enum class ShadingMode {
    grid,
    cubemap,
    phong,
    blinnPhong,
    pbr,
    flat,
    bloom,
    bloomBlend,
    gaussianBlur
};

class CyanRenderer {
public:
    uint16_t bufferWidth, bufferHeight;
    uint16_t activeShaderIdx;

    // ---- States booleans ----
    bool enableMSAA;
    // ------------------

    // ---- Shader pools -----
    Shader quadShader;
    Shader shaderPool[15];
    // -----------------------

    // ---- Framebuffers -----
    GLuint quadVBO, quadVAO;
    GLuint defaultFBO, intermFBO, MSAAFBO, hdrFBO;
    GLuint pingPongFBO[2], pingPongColorBuffer[2];
    GLuint intermDepthBuffer, depthBuffer, MSAADepthBuffer, stencilBuffer, MSAAStencilBuffer;
    GLuint intermColorBuffer, colorBuffer, MSAAColorBuffer, depthStencilBuffer; // framebuffer attachment
    GLuint colorBuffer0, colorBuffer1;
    // -----------------------

    void initRenderer();
    void initShaders();
    void prepareBlinnPhongShader(Scene& scene, MeshInstance& instance);
    void prepareFlatShader(Scene& scene, MeshInstance& instance);

    void drawInstance(Scene& scene, MeshInstance& instance);
    void drawScene(Scene& scene);
    void drawSkybox(Scene& scene);
};

extern float quadVerts[24];