#include <iostream>
#include <fstream>
#include <sstream>

#include "gtc/type_ptr.hpp"

#include "Shader.h"
#include "Scene.h"
#include "Entity.h"

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

#define CYAN_GUARD_SET_UNIFORM(name, glFunc, ...) \
    if (getUniformLocation(name) >= 0) \
    { \
        GLint loc = m_uniformMap[name]; \
        glFunc(loc, __VA_ARGS__); \
    } \
    else \
    { \
    } \

// NOTE: If "name" key doesn't exist, [] operator will automatically add an entry instead of 
// crashing the application
void ShaderBase::setUniform1i(const char* name, GLint data) {
    CYAN_GUARD_SET_UNIFORM(name, glUniform1i, data);
}

void ShaderBase::setUniform1f(const char* name, GLfloat data) {
    CYAN_GUARD_SET_UNIFORM(name, glUniform1f, data);
}

void ShaderBase::setUniformVec3(const char* name, GLfloat* vecData) {
    CYAN_GUARD_SET_UNIFORM(name, glUniform3fv, 1, vecData);
}

void ShaderBase::setUniformMat4f(const char* name, GLfloat* matData) {
    CYAN_GUARD_SET_UNIFORM(name, glUniformMatrix4fv, 1, GL_FALSE, matData);
}

GLint ShaderBase::getUniformLocation(const std::string& name)
{
    if (m_uniformMap.find(name) != m_uniformMap.end())
        return m_uniformMap[name];
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

void ShaderBase::generateShaderProgram(const char* vertSrc, const char* fragSrc)
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    m_programId = glCreateProgram();
    loadShaderSrc(vs, vertSrc, fs, fragSrc);
    glCompileShader(vs);
    glCompileShader(fs);
    checkShaderCompilation(vs);
    checkShaderCompilation(fs);
    glAttachShader(m_programId, vs);
    glAttachShader(m_programId, fs);
    glLinkProgram(m_programId);
    checkShaderLinkage(m_programId);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

// TODO: This also need to re-init all the uniforms and update uniform names assuming
// that user can modify unfiorm names or add/remove uniforms on the fly
void ShaderBase::reloadShaderProgram()
{
    glDeleteProgram(m_programId);
    // Reload shader sources
    generateShaderProgram(vsInfo.filePath, fsInfo.filePath);
}

void ShaderBase::initUniformLocations(std::vector<std::string>& names)
{
    for (auto& name : names)
    {
        int loc = glGetUniformLocation(m_programId, name.c_str());
        m_uniformMap.insert(std::pair<std::string, int>(name, loc));
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
    generateShaderProgram(vertSrc, fragSrc);

    // TODO: Where to add uniform names
    std::vector<std::string> uniformNames = 
    {
        "model",
        "view",
        "projection",
        "kAmbient",
        "kDiffuse",
        "kSpecular",
        "activeNumDiffuse",
        "activeNumSpecular",
        "activeNumEmission",
        "diffuseMaps[0]",
        "specularMaps[0]",
        "emissionMaps[0]",
        "normalMap",
        "aoMap",
        "roughnessMap",
        "hasAoMap"
    };
    initUniformLocations(uniformNames);
}

// TODO: Bind default textures to unbound samplers ...?
void BlinnPhongShader::bindMaterialTextures(Material* material)
{
    BlinnPhongMaterial* bpMaterial = static_cast<BlinnPhongMaterial*>(material);

    int textureUnit = 0;

    setUniform1i("activeNumDiffuse", bpMaterial->m_diffuseMaps.size());
    setUniform1i("activeNumSpecular", bpMaterial->m_specularMaps.size());
    setUniform1i("activeNumEmission", bpMaterial->m_emissionMaps.size());

    // Diffuse
    {
        for (int t = 0; t < bpMaterial->m_diffuseMaps.size(); t++)
        {
            char samplerName[50];
            sprintf(samplerName, "diffuseMaps[%d]", t);
            setUniform1i(samplerName, textureUnit);
            glBindTextureUnit(textureUnit++, bpMaterial->m_diffuseMaps[t]->id);
        }
    }

    // Specular
    {
        // TODO: this is hard-coded for now! 
        textureUnit = kMaxNumDiffuseMap;
        for (int t = 0; t < bpMaterial->m_specularMaps.size(); t++)
        {
            char samplerName[50];
            sprintf(samplerName, "specularMaps[%d]", t);
            setUniform1i(samplerName, textureUnit);
            glBindTextureUnit(textureUnit++, bpMaterial->m_specularMaps[t]->id);
        }
    }

    // Emission
    {
        textureUnit = kMaxNumDiffuseMap + kMaxNumSpecularMap;
        for (int t = 0; t < bpMaterial->m_emissionMaps.size(); t++)
        {
            char samplerName[50];
            sprintf(samplerName, "emissionMaps[%d]", t);
            setUniform1i(samplerName, textureUnit);
            glBindTextureUnit(textureUnit++, bpMaterial->m_emissionMaps[t]->id);
        }
    }

    // Other textures
    {
        /* Normal map */
        textureUnit = kMaxNumDiffuseMap + kMaxNumSpecularMap + kMaxNumEmissionMap;
        setUniform1i("normalMap", textureUnit);
        if (bpMaterial->m_normalMap)
        {
            glBindTextureUnit(textureUnit++, bpMaterial->m_normalMap->id);
        }

        /* Ao map */
        setUniform1i("aoMap", textureUnit);
        if (bpMaterial->m_aoMap)
        {
            glBindTextureUnit(textureUnit++, bpMaterial->m_aoMap->id);
        }

        /* Roughness map */
        setUniform1i("roughnessMap", textureUnit);
        if (bpMaterial->m_roughnessMap)
        {
            glBindTextureUnit(textureUnit++, bpMaterial->m_roughnessMap->id);
        }
    }
}

void BlinnPhongShader::prePass()
{
    glUseProgram(m_programId);

    // Lighting
    {
        // TODO:
        setUniform1f("kAmbient", m_vars.kAmbient);
        setUniform1f("kDiffuse", m_vars.kDiffuse);
        setUniform1f("kSpecular", m_vars.kSpecular);
    }

    // xforms
    {
        setUniformMat4f("model", glm::value_ptr(m_vars.model));
        setUniformMat4f("view", glm::value_ptr(m_vars.view));
        setUniformMat4f("projection", glm::value_ptr(m_vars.projection));
    }

    // Textures
    {
        bindMaterialTextures(m_vars.material);
    }

    // Misc
    {
        BlinnPhongMaterial* blinnPhongMatl = static_cast<BlinnPhongMaterial*>(m_vars.material);
        setUniform1f("hasAoMap", 0.0f);
        if (blinnPhongMatl->m_aoMap)
        {
            setUniform1f("hasAoMap", 1.0f);
        }
    }
}

void BlinnPhongShader::setShaderVariables(void* vars)
{
    memcpy(&m_vars, vars, sizeof(BlinnPhongShaderVars));
}

void BlinnPhongShader::updateShaderVarsForEntity(Entity* e)
{
    glm::mat4 model(1.f);
    model = glm::translate(model, e->xform.translation);
    glm::vec3 rotAxis(0.f, 1.f, 0.f);
    float theta = RADIANS(90.f);
    glm::quat quat(cos(theta * 0.5f), sin(theta * 0.5f) * rotAxis);
    glm::mat4 rot = glm::toMat4(e->xform.qRot);
    model *= rot;
    model = glm::scale(model, e->xform.scale);
    model *= e->mesh->normalizeXform;
    m_vars.model = model;
    m_vars.material = e->matl;
}


PbrShader::PbrShader(const char* vertSrc, const char* fragSrc)
    : ShaderBase(vertSrc, fragSrc)
{
    generateShaderProgram(vertSrc, fragSrc);

    // TODO: Where to add uniform names
    std::vector<std::string> uniformNames = 
    {
        "model",
        "view",
        "projection",
        "kAmbient",
        "kDiffuse",
        "kSpecular",
        "activeNumDiffuse",
        "activeNumSpecular",
        "activeNumEmission",
        "diffuseMaps[0]",
        "specularMaps[0]",
        "emissionMaps[0]",
        "normalMap",
        "aoMap",
        "roughnessMap",
        "hasAoMap",
        "hasNormalMap",
        "numPointLights",
        "numDirLights"
    };
    initUniformLocations(uniformNames);
    glCreateBuffers(1, &m_pLightsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_pLightsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PointLight) * kMaxPointLights, 0, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_pLightsBuffer);

    glCreateBuffers(1, &m_dLightsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_dLightsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(DirectionalLight) * kMaxDirLights, 0, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_dLightsBuffer);
} 

void PbrShader::prePass()
{
    glUseProgram(m_programId);

    // Lighting
    {
        // TODO: Do we have to update this every frame ...?
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_pLightsBuffer);
        void* baseAddr = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
        memcpy(baseAddr, m_vars.pLights, sizeof(PointLight) * m_vars.numPointLights);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_dLightsBuffer);
        baseAddr = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
        memcpy(baseAddr, m_vars.dLights, sizeof(DirectionalLight) * m_vars.numDirLights);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        setUniform1i("numPointLights", m_vars.numPointLights);
        setUniform1i("numDirLights", m_vars.numDirLights);

        setUniform1f("kAmbient", m_vars.kAmbient);
        setUniform1f("kDiffuse", m_vars.kDiffuse);
        setUniform1f("kSpecular", m_vars.kSpecular);
    }

    // xforms
    {
        setUniformMat4f("model", glm::value_ptr(m_vars.model));
        setUniformMat4f("view", glm::value_ptr(m_vars.view));
        setUniformMat4f("projection", glm::value_ptr(m_vars.projection));
    }

    // Textures
    {
        bindMaterialTextures(m_vars.material);
    }

    // Misc
    {
        PbrMaterial* pbrMaterial = static_cast<PbrMaterial*>(m_vars.material);
        setUniform1f("hasAoMap", 0.0f);
        if (pbrMaterial->m_aoMap)
        {
            setUniform1f("hasAoMap", 1.0f);
        }
        setUniform1f("hasNormalMap", 0.0f);
        if (pbrMaterial->m_normalMap)
        {
            setUniform1f("hasNormalMap", 1.0f);
        }
    }
}

void PbrShader::bindMaterialTextures(Material* matl)
{
    PbrMaterial* pbrMaterial = static_cast<PbrMaterial*>(matl);

    int textureUnit = 0;

    setUniform1i("activeNumDiffuse", pbrMaterial->m_diffuseMaps.size());
    setUniform1i("activeNumSpecular", pbrMaterial->m_specularMaps.size());
    setUniform1i("activeNumEmission", pbrMaterial->m_emissionMaps.size());

    // Diffuse
    {
        for (int t = 0; t < pbrMaterial->m_diffuseMaps.size(); t++)
        {
            char samplerName[50];
            sprintf(samplerName, "diffuseMaps[%d]", t);
            setUniform1i(samplerName, textureUnit);
            glBindTextureUnit(textureUnit++, pbrMaterial->m_diffuseMaps[t]->id);
        }
    }

    // Specular
    {
        // TODO: this is hard-coded for now! 
        textureUnit = kMaxNumDiffuseMap;
        for (int t = 0; t < pbrMaterial->m_specularMaps.size(); t++)
        {
            char samplerName[50];
            sprintf(samplerName, "specularMaps[%d]", t);
            setUniform1i(samplerName, textureUnit);
            glBindTextureUnit(textureUnit++, pbrMaterial->m_specularMaps[t]->id);
        }
    }

    // Emission
    {
        textureUnit = kMaxNumDiffuseMap + kMaxNumSpecularMap;
        for (int t = 0; t < pbrMaterial->m_emissionMaps.size(); t++)
        {
            char samplerName[50];
            sprintf(samplerName, "emissionMaps[%d]", t);
            setUniform1i(samplerName, textureUnit);
            glBindTextureUnit(textureUnit++, pbrMaterial->m_emissionMaps[t]->id);
        }
    }

    // Other textures
    {
        /* Normal map */
        textureUnit = kMaxNumDiffuseMap + kMaxNumSpecularMap + kMaxNumEmissionMap;
        setUniform1i("normalMap", textureUnit);
        if (pbrMaterial->m_normalMap)
        {
            glBindTextureUnit(textureUnit++, pbrMaterial->m_normalMap->id);
        }

        /* Ao map */
        setUniform1i("aoMap", textureUnit);
        if (pbrMaterial->m_aoMap)
        {
            glBindTextureUnit(textureUnit++, pbrMaterial->m_aoMap->id);
        }

        /* Roughness map */
        setUniform1i("roughnessMap", textureUnit);
        if (pbrMaterial->m_roughnessMap)
        {
            glBindTextureUnit(textureUnit++, pbrMaterial->m_roughnessMap->id);
        }
    }
}

void PbrShader::setShaderVariables(void* vars)
{
    memcpy(&m_vars, vars, sizeof(PbrShaderVars));
}

void PbrShader::updateShaderVarsForEntity(Entity* e)
{
    glm::mat4 model(1.f);
    model = glm::translate(model, e->xform.translation);
    glm::vec3 rotAxis(0.f, 1.f, 0.f);
    float theta = RADIANS(90.f);
    glm::quat quat(cos(theta * 0.5f), sin(theta * 0.5f) * rotAxis);
    glm::mat4 rot = glm::toMat4(e->xform.qRot);
    model *= rot;
    model = glm::scale(model, e->xform.scale);
    model *= e->mesh->normalizeXform;
    m_vars.model = model;
    m_vars.material = e->matl;
}

GenEnvmapShader::GenEnvmapShader(const char* vertSrc, const char* fragSrc)
    : ShaderBase(vertSrc, fragSrc)
{
    generateShaderProgram(vertSrc, fragSrc);

    // TODO: Where to add uniform names
    std::vector<std::string> uniformNames = 
    {
        "view",
        "projection",
        "rawEnvmapSampler",
    };
    initUniformLocations(uniformNames);
}

void GenEnvmapShader::prePass()
{
    glUseProgram(m_programId);

    {
        setUniformMat4f("view", glm::value_ptr(m_vars.view));
        setUniformMat4f("projection", glm::value_ptr(m_vars.projection));
    }

    {
        setUniform1i("rawEnvmapSampler", 0);
        glBindTextureUnit(0, m_vars.envmap);
    }
}

void GenEnvmapShader::bindMaterialTextures(Material* matl)
{

}

void GenEnvmapShader::setShaderVariables(void* vars)
{
    memcpy(&m_vars, vars, sizeof(EnvmapShaderVars));
}

void GenEnvmapShader::updateShaderVarsForEntity(Entity* e)
{

}

GenIrradianceShader::GenIrradianceShader(const char* vertSrc, const char* fragSrc)
    : ShaderBase(vertSrc, fragSrc)
{
    generateShaderProgram(vertSrc, fragSrc);

    // TODO: Where to add uniform names
    std::vector<std::string> uniformNames = 
    {
        "view",
        "projection",
        "envmapSampler"
    };
    initUniformLocations(uniformNames);

    glCreateBuffers(1, &m_sampleVertexBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_sampleVertexBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 3 * sizeof(float) * 16, 0, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_sampleVertexBuffer);
}

void GenIrradianceShader::prePass()
{
    glUseProgram(m_programId);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_sampleVertexBuffer);
    {
        setUniformMat4f("view", glm::value_ptr(m_vars.view));
        setUniformMat4f("projection", glm::value_ptr(m_vars.projection));
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_vars.envmap);
}

void GenIrradianceShader::setShaderVariables(void* vars)
{
    std::memcpy(&m_vars, vars, sizeof(EnvmapShaderVars));
}

EnvmapShader::EnvmapShader(const char* vertSrc, const char* fragSrc)
    : ShaderBase(vertSrc, fragSrc)
{
    generateShaderProgram(vertSrc, fragSrc);

    // TODO: Where to add uniform names
    std::vector<std::string> uniformNames = 
    {
        "view",
        "projection",
        "envmapSampler"
    };
    initUniformLocations(uniformNames);
}

void EnvmapShader::prePass()
{
    glUseProgram(m_programId);
    {
        setUniformMat4f("view", glm::value_ptr(m_vars.view));
        setUniformMat4f("projection", glm::value_ptr(m_vars.projection));
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_vars.envmap);
}

void EnvmapShader::setShaderVariables(void* vars)
{
    std::memcpy(&m_vars, vars, sizeof(EnvmapShaderVars));
}

QuadShader::QuadShader(const char* vertSrc, const char* fragSrc)
    : ShaderBase(vertSrc, fragSrc)
{
    generateShaderProgram(vertSrc, fragSrc);

    std::vector<std::string> uniformNames = 
    {
        "quadSampler"
    };
    initUniformLocations(uniformNames);
}

void QuadShader::prePass()
{
    glUseProgram(m_programId);

    {
        setUniform1i("quadSampler", 0);
        glBindTextureUnit(0, m_vars.texture);
    }
}

void QuadShader::setShaderVariables(void* vars)
{
    memcpy(&m_vars, vars, sizeof(QuadShaderVars));
}