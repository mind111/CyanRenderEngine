#include <iostream>
#include <fstream>
#include <sstream>

#include "gtc/type_ptr.hpp"

#include "Shader.h"
#include "Scene.h"

void checkShaderCompilation(GLuint shader) {
    int compile_result;
    char log[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_result);
    if (!compile_result) {
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cout << log << std::endl;
    }
}

void checkShaderLinkage(GLuint shader) {
    int link_result;
    char log[512];
    glGetShaderiv(shader, GL_LINK_STATUS, &link_result);
    if (!link_result) {
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cout << log << std::endl;
    }
}

const char* readShaderFile(const char* filename) {
    // Open file
    std::fstream shader_file(filename);
    std::string src_str;
    const char* vertex_shader_src = nullptr; 
    if (shader_file.is_open()) {
        std::stringstream src_str_stream;
        std::string line;
        while (std::getline(shader_file, line)) {
            src_str_stream << line << "\n";
        }
        src_str = src_str_stream.str();
        // Copy the string over to a char array
        vertex_shader_src = new char[src_str.length()];
        // + 1 here to include null character
        std::memcpy((void*)vertex_shader_src, src_str.c_str(), src_str.length() + 1);
    } else {
        std::cout << "ERROR: Cannot open shader source file!" << std::endl;
    }
    // close file
    shader_file.close();
    return vertex_shader_src;
}

// NOTE (Min): Ran into a situation where if I don't close the file handle properly, later openning the 
// the file using ifstream() will fail. Remember to close the file handles!!!
ShaderBase::ShaderBase(const char* vsSrc, const char* fsSrc)
{
    vsInfo.filePath = vsSrc;
    HANDLE hVs = CreateFileA(vsInfo.filePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
    GetFileTime(hVs, 0, 0, &vsInfo.lastWriteTime);
    CloseHandle(hVs);
    fsInfo.filePath = fsSrc;
    HANDLE hFs = CreateFileA(fsInfo.filePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
    GetFileTime(hVs, 0, 0, &fsInfo.lastWriteTime);
    CloseHandle(hFs);
}

void ShaderBase::setUniform1i(const char* name, GLint data) {
    GLint loc = uniformMap[name];
    glUniform1i(loc, data);
}

void ShaderBase::setUniform1f(const char* name, GLfloat data) {
    GLint floatLocation = uniformMap[name];
    glUniform1f(floatLocation, data);
}

void ShaderBase::setUniformVec3(const char* name, GLfloat* vecData) {
    GLint vecLocation = uniformMap[name];
    glUniform3fv(vecLocation, 1, vecData);
}

void ShaderBase::setUniformMat4f(const char* name, GLfloat* matData) {
    GLint loc = uniformMap[name];
    glUniformMatrix4fv(loc, 1, GL_FALSE, matData);
}

GLint ShaderBase::getUniformLocation(std::string& name)
{
    if (uniformMap.find(name) != uniformMap.end())
        return uniformMap[name];
    return -1;
}

void ShaderBase::loadShaderSrc(GLuint vs, const char* vertSrc, 
                               GLuint fs, const char* fragSrc) 
{
    const char* vertShaderSrc = readShaderFile(vertSrc);
    const char* fragShaderSrc = readShaderFile(fragSrc);
    glShaderSource(vs, 1, &vertShaderSrc, nullptr);
    glShaderSource(fs, 1, &fragShaderSrc, nullptr);
}

void ShaderBase::generateShaderProgram(GLuint vs, GLuint fs, GLuint program)
{
    glCompileShader(vs);
    glCompileShader(fs);
    checkShaderCompilation(vs);
    checkShaderCompilation(fs);
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    checkShaderLinkage(program);
}

// TODO: This also need to re-init all the uniforms and update uniform names assuming
// that user can modify unfiorm names or add/remove uniforms on the fly
void ShaderBase::reloadShaderProgram()
{
    glDeleteProgram(mProgramId);
    // Reload shader sources
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    mProgramId = glCreateProgram();
    loadShaderSrc(vs, vsInfo.filePath, fs, fsInfo.filePath);
    generateShaderProgram(vs, fs, mProgramId);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

/*
        {
            "name": "chinese_dragon",
            "path": "asset/dragon/dragon.obj",
            "diffuseTexture": [""],
            "specularTexture": [""], 
            "normalMapName": "",
            "aoMapName": "",
            "roughnessMapName": ""
        },
*/

void BlinnPhongShader::initUniformLocations(std::vector<std::string>& names)
{
    for (auto& name : names)
    {
        int loc = glGetUniformLocation(mProgramId, name.c_str());
        uniformMap.insert(std::pair<std::string, int>(name, loc));
    }
}

// TODO: Parser to parse a shader file and extract all the uniform variable names ...?
// maybe something like 
/*
   std::vector<std::string> getUniformNames(const char* src)
   {
       // parse

       // return 
   }
*/
// TODO: Mirror the uniform variables in the shader on the cpp side
BlinnPhongShader::BlinnPhongShader(const char* vertSrc, const char* fragSrc)
    : ShaderBase(vertSrc, fragSrc)
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    mProgramId = glCreateProgram();
    loadShaderSrc(vs, vertSrc, fs, fragSrc);
    generateShaderProgram(vs, fs, mProgramId);
    glDeleteShader(vs);
    glDeleteShader(fs);
    std::vector<std::string> uniformNames = 
    {
        "model",
        "view",
        "projection"
    };
    initUniformLocations(uniformNames);
}

void BlinnPhongShader::bindMaterialTextures(Material* material)
{
    BlinnPhongMaterial* bpMaterial = dynamic_cast<BlinnPhongMaterial*>(material);

    int textureUnit = 0;

    // Diffuse
    {
        for (auto itr = bpMaterial->diffuseBegin(); itr != bpMaterial->diffuseEnd(); itr++)
        {
            // Bind sampler2D to texture unit
            char samplerName[50];
            sprintf(samplerName, "diffuseSampler[%d]", textureUnit);
            setUniform1f(samplerName, textureUnit);
            if (*itr)
            {
                // Bind texture object to texture unit
                glBindTextureUnit(textureUnit++, (*itr)->id);
            }
        }
    }

    // Specular
    {
        for (auto itr = bpMaterial->specularBegin(); itr != bpMaterial->specularEnd(); itr++)
        {
            // Bind sampler2D to texture unit
            char samplerName[50];
            sprintf(samplerName, "specularSamplers[%d]", textureUnit);
            setUniform1f(samplerName, textureUnit);
            if (*itr)
            {
                // Bind texture object to texture unit
                glBindTextureUnit(textureUnit++, (*itr)->id);
            }
        }
    }

    // Other textures
    {
        setUniform1f("normalSampler", textureUnit);
        if (bpMaterial->getNormalMap())
        {
            // Bind texture object to texture unit
            glBindTextureUnit(textureUnit++, bpMaterial->getNormalMap()->id);
        }
    }
}

void BlinnPhongShader::prePass(void* params)
{
    BlinnPhongShaderParams* blinnPhongParams = static_cast<BlinnPhongShaderParams*>(params);

    glUseProgram(mProgramId);

    // Lighting
    {
        setUniformVec3("dLight.baseLight.color", glm::value_ptr(blinnPhongParams->dLight.color));
        setUniformVec3("dLight.direction", glm::value_ptr(blinnPhongParams->dLight.direction));
    }

    // xforms
    {
        setUniformMat4f("model", glm::value_ptr(blinnPhongParams->model));
        setUniformMat4f("view", glm::value_ptr(blinnPhongParams->view));
        setUniformMat4f("projection", glm::value_ptr(blinnPhongParams->projection));
    }

    // Textures
    {
        bindMaterialTextures(blinnPhongParams->material);
    }
}

// legacy code

#if 0
void Shader::init() {
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    shaderProgram = glCreateProgram();
}

void Shader::initUniformLoc(const std::vector<std::string>& uniformNames) {
    for (auto name : uniformNames) {
        GLint location = glGetUniformLocation(shaderProgram, name.c_str());
        uniformLocMap.insert(std::pair<std::string, GLint>(name, location));
    }
}

void Shader::loadShaderSrc(const char* vertFileName, 
                           const char* fragFileName) {
    const char* vertShaderSrc = readShaderFile(vertFileName);
    const char* fragShaderSrc = readShaderFile(fragFileName);
    glShaderSource(vertexShader, 1, &vertShaderSrc, nullptr);
    glShaderSource(fragmentShader, 1, &fragShaderSrc, nullptr);
}

void Shader::generateShaderProgram() {
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);
    checkShaderCompilation(vertexShader);
    checkShaderCompilation(fragmentShader);
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkShaderLinkage(shaderProgram);
}

void Shader::setUniform1i(const char* uniformName, GLint data) {
    GLint intLocation = uniformLocMap[uniformName];
    glUniform1i(intLocation, data);
}

void Shader::setUniform1f(const char* uniformName, GLfloat data) {
    GLint floatLocation = uniformLocMap[uniformName];
    glUniform1f(floatLocation, data);
}

void Shader::setUniformVec3(const char* uniformName, GLfloat* vecData) {
    GLint vecLocation = uniformLocMap[uniformName];
    glUniform3fv(vecLocation, 1, vecData);
}

void Shader::setUniformMat4f(const char* uniformName, GLfloat* matData) {
    GLint matLocation = uniformLocMap[uniformName];
    glUniformMatrix4fv(matLocation, 1, GL_FALSE, matData);
}

void Shader::bindShader() {
    glUseProgram(shaderProgram);
}
#endif