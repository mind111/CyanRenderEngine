#pragma once

#include <functional>

#include "glew.h"

#include "Common.h"
#include "Shader.h"
#include "Scene.h"
#include "camera.h"
#include "Entity.h"

class aiMesh;

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

struct RenderConfig
{
    bool hdr;
    bool msaa;
    bool bloom;
    bool motionBlur;
    bool toneMapping;
};

// Asset loading utils
class ToolKit
{
public:
    static void loadScene(Scene& scene, const char* filename);
    static void loadTexture();
    static glm::mat4 normalizeMeshScale(MeshGroup* mesh);
    static VertexInfo loadSubMesh(aiMesh* assimpMesh, uint8_t* attribFlag);
};

struct FrameBuffer
{
    GLuint fbo;
    GLuint colorTarget;
    GLuint depthTarget;
};

// TODO: Abstract FrameBuffer object 
class CyanRenderer {
public:

    // Rendering assets
    typedef struct
    {
        std::vector<MeshGroup*> meshes;
        std::vector<Texture*> textures;
        std::vector<Material*> materials;
    } Assets;

    Grid floorGrid;
    uint16_t bufferWidth, bufferHeight;
    uint16_t activeShaderIdx;

    // ---- Shader Cache -----
    uint16_t shaderCount;
    ShaderBase* mShaders[15];
    // -----------------------

    // ---- Framebuffers -----
    FrameBuffer frameBuffer;

    GLuint quadVBO, quadVAO;
    GLuint defaultFBO, intermFBO, MSAAFBO, hdrFBO;
    GLuint pingPongFBO[2], pingPongColorBuffer[2];
    GLuint intermDepthBuffer, depthBuffer, MSAADepthBuffer, stencilBuffer, MSAAStencilBuffer;
    GLuint intermColorBuffer, colorBuffer, MSAAColorBuffer, depthStencilBuffer; // framebuffer attachment
    GLuint colorBuffer0, colorBuffer1;
    // -----------------------

    CyanRenderer();

    typedef std::function<void()> RequestSwapCallback;
    static CyanRenderer* get();
    void bindSwapBufferCallback(RequestSwapCallback callback);
    void initRenderer();

    void registerShader(ShaderBase* shader);

    FrameBuffer createFrameBuffer(int bufferWidth, int bufferHeight, int internalFormat);

    void prePass(Scene& scene, Entity& entity);
    void render(Scene& scene, RenderConfig& renderConfig);
    void requestSwapBuffers();

    glm::mat4 generateViewMatrix(const Camera& camera);

    void prepareGridShader(Scene& scene);

    void setupMSAA();
    void bloomPostProcess();
    void blitBackbuffer(); // final render pass

    MeshGroup* findMesh(const std::string& desc);
    Texture* findTexture(const std::string& name);
    Material* findMaterial(const std::string& name);

    void drawMesh(MeshGroup& mesh);
    void drawEntity(Scene& scene, Entity* entity);
    void drawGrid();
    void drawSkybox(Scene& scene);

    /* 
        * Wrapper over basic OpenGL operations
    */
    void clearBackBuffer();
    void enableDepthTest();
    void setClearColor(glm::vec4 color);

    ShaderBase* getShader(ShaderType type);
    ShaderBase* getMaterialShader(MaterialType type);

    MeshGroup* createMesh(const char* meshName);
    MeshGroup* createMeshFromFile(const char* fileName);

    Assets mRenderAssets;

private:
    friend class ToolKit;
    RequestSwapCallback mSwapBufferCallback;

    void updateShaderVariables(Entity* entity);

    static Texture createTexture(GLenum target, GLint intermalFormat, GLenum pixelFormat, GLenum dataType, void* image, int width, int height);
    static Texture createTexture(GLenum target, GLint intermalFormat, GLenum pixelFormat, GLenum dataType, int width, int height);

    static void bindTexture(GLenum target, Texture& texture);

    std::vector<ShaderBase*> shaders;
    Scene* m_Scene;
};

extern float quadVerts[24];