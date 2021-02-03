#pragma once

#include "Windows.h"

#include <vector>
#include <map>

#include "glew.h"

#include "Light.h"
#include "Material.h"
#include "Scene.h"

// TODO: Is there a way to further abstract this into a ShaderParams

enum class ShaderType
{
    None = 0,
    BlinnPhong,
    Pbr,
    GenEnvmap,
    GenIrradiance,
    QuadShader
};

struct BlinnPhongShaderVars 
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
    float kAmbient, kDiffuse, kSpecular;
    Material* material;
};

struct PbrShaderVars
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
    float kAmbient, kDiffuse, kSpecular;
    Material* material;
    DirectionalLight* dLights;
    PointLight* pLights;
    u8 numDirLights;
    u8 numPointLights;
};

struct ShaderFileInfo
{
    const char* filePath;
    FILETIME lastWriteTime;
};

class ShaderBase
{
public:
    ShaderBase(const char* vsSrc, const char* fsSrc);
    virtual ~ShaderBase() { }

    virtual ShaderType getShaderType() = 0;
    virtual void prePass() = 0;
    virtual void bindMaterialTextures(Material* matl) = 0;
    virtual void setShaderVariables(void* vars) = 0;
    virtual void updateShaderVarsForEntity(Entity* e) = 0;
    virtual void initUniformLocations(std::vector<std::string>& names);

    void setUniform1i(const char* name, GLint data);
    void setUniform1f(const char* name, GLfloat data);
    void setUniformVec3(const char* name, GLfloat* vecData);
    void setUniformMat4f(const char* name, GLfloat* matData);

    void reloadShaderProgram();
    
    ShaderFileInfo vsInfo;
    ShaderFileInfo fsInfo;

protected:
    void loadShaderSrc(GLuint vs, const char* vertFileName, GLuint fs, const char* fragFileName);
    void generateShaderProgram(const char* vertSrc, const char* fragSrc);
    GLint getUniformLocation(const std::string& name);

    std::map<std::string, int> m_uniformMap;

    GLuint m_programId;
};

class BlinnPhongShader : public ShaderBase
{
public:
    static const int kMaxNumDiffuseMap = 6;
    static const int kMaxNumSpecularMap = 6;
    static const int kMaxNumEmissionMap = 2;

    BlinnPhongShader(const char* vertSrc, const char* fragSrc);
    ~BlinnPhongShader() { }

    virtual inline ShaderType getShaderType() { return ShaderType::BlinnPhong; }
    virtual void prePass() override;
    virtual void bindMaterialTextures(Material* matl) override;
    virtual void setShaderVariables(void* vars) override;
    virtual void updateShaderVarsForEntity(Entity* e) override;

    BlinnPhongShaderVars m_vars;
private:

};

class PbrShader : public ShaderBase
{
public:
    static const int kMaxNumDiffuseMap = 6;
    static const int kMaxNumSpecularMap = 6;
    static const int kMaxNumEmissionMap = 2;
    static const int kMaxPointLights = 10;
    static const int kMaxDirLights = 10;

    virtual inline ShaderType getShaderType() { return ShaderType::Pbr; }
    virtual void prePass() override;
    virtual void bindMaterialTextures(Material* matl) override;
    virtual void setShaderVariables(void* vars) override;
    virtual void updateShaderVarsForEntity(Entity* e) override;

    PbrShader(const char* vertSrc, const char* fragSrc);
    ~PbrShader() { }

    GLuint m_pLightsBuffer;
    GLuint m_dLightsBuffer;

    PbrShaderVars m_vars;
};

struct EnvmapShaderVars
{
    glm::mat4 view;
    glm::mat4 projection;
    GLuint envmap;
};

class GenEnvmapShader : public ShaderBase
{
public:
    virtual inline ShaderType getShaderType() { return ShaderType::GenEnvmap; }
    virtual void prePass() override;
    virtual void bindMaterialTextures(Material* matl) override;
    virtual void setShaderVariables(void* vars) override;
    virtual void updateShaderVarsForEntity(Entity* e) override;

    GenEnvmapShader(const char* vertSrc, const char* fragSrc);
    ~GenEnvmapShader() { }

    EnvmapShaderVars m_vars;
};


class EnvmapShader : public ShaderBase
{
public:
    virtual inline ShaderType getShaderType() { return ShaderType::GenEnvmap; }
    virtual void prePass() override;
    virtual void bindMaterialTextures(Material* matl) override { }
    virtual void setShaderVariables(void* vars) override;
    virtual void updateShaderVarsForEntity(Entity* e) override { };

    EnvmapShader(const char* vertSrc, const char* fragSrc);
    ~EnvmapShader() { };

    EnvmapShaderVars m_vars;
};

class GenIrradianceShader : public ShaderBase
{
public:
    virtual inline ShaderType getShaderType() { return ShaderType::GenIrradiance; }
    virtual void prePass() override;
    virtual void bindMaterialTextures(Material* matl) override { }
    virtual void setShaderVariables(void* vars) override;
    virtual void updateShaderVarsForEntity(Entity* e) override { };

    GenIrradianceShader(const char* vertSrc, const char* fragSrc);
    ~GenIrradianceShader() { };

    // Buffer used to store the vertex of generated samples (Debugging purpose)
    GLuint m_sampleVertexBuffer;
    EnvmapShaderVars m_vars; 
};

struct QuadShaderVars
{
    GLuint texture;
};

class QuadShader : public ShaderBase
{
public:
    virtual inline ShaderType getShaderType() { return ShaderType::QuadShader; }
    virtual void prePass() override;
    virtual void bindMaterialTextures(Material* matl) override { }
    virtual void setShaderVariables(void* vars) override;
    virtual void updateShaderVarsForEntity(Entity* e) override { }

    QuadShader(const char* vertSrc, const char* fragSrc);
    ~QuadShader() { }

    QuadShaderVars m_vars;
};