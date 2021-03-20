#pragma once

#include <functional>
#include <map>

#include "glew.h"
#include "stb_image.h"

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

struct FrameBuffer
{
    GLuint fbo;
    GLuint colorTarget;
    GLuint depthTarget;
};

// TODO: Implement GfxContext class to manage the rendering state
class GfxContext
{
public:
    GfxContext() { }
    ~GfxContext() { }

    void setMaterial(Material* _material) 
    { 
        _material->m_shader->bind();

        for (auto& uniform : _material->m_uniforms)
            shader->setUniform(uniform);

        for (auto& texture : _material->m_textures)
            shader->setTexture("", texture.id);
    };

    GLuint m_vao;
    GLuint m_fbo;
    GLuint m_activeShader;
    glm::vec2 m_viewportSize;
    bool b_depthTest;
};

namespace Cyan
{
    Shader* createShader(const char* vertSrc, const char* fragSrc);


    // TODO: Do we need to cache uniforms and materials?
    Uniform* createUniform(const char* _name, Uniform::UniformType _type)
    {
        Uniform* uniform = new Uniform{_type, _name, 0};
        return uniform;
    }
    Material* createMaterial(Shader* _shader)
    {
        Material* material = new Material{_shader};
        return material;
    }
}

// TODO: Abstract FrameBuffer object 
class CyanRenderer {
public:

    /* Utility functions */
    class Toolkit
    {
    public:
        static GLuint createTextureFromFile(const char* file, TextureType textureType, TexFilterType filterType);
        static GLuint createHDRTextureFromFile(const char* file, TextureType textureType, TexFilterType filterType);

        static void loadScene(Scene& scene, const char* filename);
        static void loadTexture();
        static glm::mat4 normalizeMeshScale(MeshGroup* mesh);
        static VertexInfo loadSubMesh(aiMesh* assimpMesh, uint8_t* attribFlag);
    };

    /* Rendering assets */
    typedef struct
    {
        std::vector<MeshGroup*> meshes;
        std::vector<Texture*> textures;
        std::vector<Material*> materials;
    } Assets;

    Grid floorGrid;
    uint16_t bufferWidth, bufferHeight;
    uint16_t activeShaderIdx;

    u16 shaderCount;
    ShaderBase* mShaders[15];

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

    /*
    * NOTE(min): Three different kinds of envmap shader can be passed to this function as they use same type of ShaderVars, EnvmapShaderVars
    */
    GLuint createCubemapFromTexture(ShaderBase* shader, GLuint envmap, MeshGroup* cubeMesh, int windowWidth, int windowHeight, int viewportWidth, int viewportHeight, bool hdr=true);

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
    // TODO:
    void drawEnvmap() { }
    void drawGrid();

    void clearBackBuffer();
    void enableDepthTest();
    void setClearColor(glm::vec4 color);

    ShaderBase* getShader(ShaderType type);
    ShaderBase* getMaterialShader(MaterialType type);

    /* Create an empty MeshGroup */
    MeshGroup* createMeshGroup(const char* meshName);

    /* Create a MeshGroup from an asset file */
    MeshGroup* createMeshFromFile(const char* fileName);

    /* Create a unit cube mesh */
    MeshGroup* createCubeMesh(const char* meshName);

    Assets mRenderAssets;

private:
    RequestSwapCallback mSwapBufferCallback;

    void updateShaderVariables(Entity* entity);

    std::vector<ShaderBase*> shaders;
    Scene* m_Scene;
    Toolkit m_toolKit;
};

extern float quadVerts[24];