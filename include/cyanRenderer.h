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
    pbr
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
    Shader shaderPool[5];
    // -----------------------

    // ---- Framebuffers -----
    GLuint quadVBO, quadVAO;
    GLuint defaultFBO;
    GLuint depthBuffer, stencilBuffer;
    GLuint colorBuffer, depthStencilBuffer; // framebuffer attachment
    GLuint multiSampleFBO;
    // -----------------------

    void initRenderer();
    void initShaders();
    void drawInstance(Scene& scene, MeshInstance& instance);
    void drawScene(Scene& scene);
    void drawSkybox(Scene& scene);
};

extern float quadVerts[24];