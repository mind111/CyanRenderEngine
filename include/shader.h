#pragma once

#include "Windows.h"

#include <vector>
#include <map>

#include "glew.h"

#include "Material.h"
#include "Scene.h"

// TODO: Is there a way to further abstract this into a ShaderParams

enum class ShaderType
{
    None = 0,
    BlinnPhong,
    PBR
};

struct BlinnPhongShaderParams 
{
    DirectionLight dLight;
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
    Material* material;
};

struct PBRShaderParams
{

};

// TODO: Define this in the cpp file ...?
struct ShaderFileInfo
{
    const char* filePath;
    FILETIME lastWriteTime;
};

class Shader {
public:
    GLuint vertexShader;
    GLuint fragmentShader;
    GLuint shaderProgram;

    std::map<std::string, GLint> uniformLocMap;

    void init();
    void initUniformLoc(const std::vector<std::string>& uniformNames);
    void loadShaderSrc(const char* vertFileName, const char* fragFileName);
    void generateShaderProgram();
    void bindShader(); 
    void unbindShader();
    
    void setUniform1i(const char* uniformName, GLint data);
    void setUniform1f(const char* uniformName, GLfloat data);
    void setUniformVec3(const char* uniformName, GLfloat* vecData);
    void setUniformMat4f(const char* uniformName, GLfloat* matData);
    
    Shader() {}
    ~Shader() {}
};

class ShaderBase
{
public:
    ShaderBase(const char* vsSrc, const char* fsSrc);
    virtual ~ShaderBase() { }

    virtual ShaderType getShaderType() = 0;
    virtual void prePass(void* params) = 0;
    virtual void bindMaterialTextures(Material* matl) = 0;

    void setUniform1i(const char* name, GLint data);
    void setUniform1f(const char* name, GLfloat data);
    void setUniformVec3(const char* name, GLfloat* vecData);
    void setUniformMat4f(const char* name, GLfloat* matData);

    void reloadShaderProgram();
    
    ShaderFileInfo vsInfo;
    ShaderFileInfo fsInfo;

protected:
    void loadShaderSrc(GLuint vs, const char* vertFileName, GLuint fs, const char* fragFileName);
    void generateShaderProgram(GLuint vs, GLuint fs, GLuint program);
    GLint getUniformLocation(std::string& name);

    std::map<std::string, int> uniformMap;
    GLuint mProgramId;
};

class BlinnPhongShader : public ShaderBase
{
public:
    static const int kMaxDiffuseMapCount = 5;
    static const int kMaxSpecularMapCount = 5;

    BlinnPhongShader(const char* vertSrc, const char* fragSrc);
    ~BlinnPhongShader() { }

    virtual inline ShaderType getShaderType() { return ShaderType::BlinnPhong; }
    virtual void prePass(void* params) override;
    virtual void bindMaterialTextures(Material* matl) override;

    void initUniformLocations(std::vector<std::string>& names);
};