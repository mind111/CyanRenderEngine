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

struct Grid {
    uint32_t gridSize; 
    uint32_t numOfVerts;
    GLuint vbo;
    GLuint vao;
    float* verts;
    void initRenderParams();
    void generateVerts();
    void printGridVerts(); // Debugging purposes
};

class CyanRenderer {
public:
    Grid floorGrid;
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
    void prepareGridShader(Scene& scene);
    void prepareBlinnPhongShader(Scene& scene, MeshInstance& instance);
    void prepareFlatShader(Scene& scene, MeshInstance& instance);

    void setupMSAA();
    void bloomPostProcess();
    void blitBackbuffer(); // final render pass

    void drawInstance(Scene& scene, MeshInstance& instance);
    void drawScene(Scene& scene);
    void drawGrid();
    void drawSkybox(Scene& scene);
};

extern float quadVerts[24];