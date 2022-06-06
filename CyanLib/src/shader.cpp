#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>

#include "gtc/type_ptr.hpp"

#include "CyanAPI.h"
#include "GfxContext.h"
#include "Shader.h"
#include "Scene.h"
#include "Entity.h"

namespace Cyan
{    
    static void checkShaderCompilation(GLuint shader) 
    {
        int compile_result;
        char log[512];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_result);
        if (!compile_result) {
            glGetShaderInfoLog(shader, 512, nullptr, log);
            std::cout << log << std::endl;
        }
    }

    static void checkShaderLinkage(GLuint shader) 
    {
        int link_result;
        char log[512];
        glGetShaderiv(shader, GL_LINK_STATUS, &link_result);
        if (!link_result) {
            glGetShaderInfoLog(shader, 512, nullptr, log);
            std::cout << log << std::endl;
        }
    }

    static const char* readShaderFile(const char* filename) 
    {
        // Open file
        std::fstream shader_file(filename);
        std::string src_str;
        const char* shader_src = nullptr;
        if (shader_file.is_open()) {
            std::stringstream src_str_stream;
            std::string line;
            while (std::getline(shader_file, line)) {
                src_str_stream << line << "\n";
            }
            src_str = src_str_stream.str();
            // Copy the string over to a char array
            shader_src = new char[src_str.length() + 1];
            // + 1 here to include null character
            std::memcpy((void*)shader_src, src_str.c_str(), src_str.length() + 1);
        } else {
            std::cout << "ERROR: Cannot open shader source file!" << std::endl;
        }
        // close file
        shader_file.close();
        return shader_src;
    }

    // global shader definitions shared by multiple shaders
    const char* gCommonMathDefs = {
        R"(
            #define PI 3.1415926f
        )"
    };

    const char* gDrawDataDef = {
        R"(
            layout(std430, binding = 0) buffer GlobalDrawData
            {
                mat4  view;
                mat4  projection;
                mat4  sunLightView;
                mat4  sunShadowProjections[4];
                int   numDirLights;
                int   numPointLights;
                float m_ssao;
                float dummy;
            } gDrawData;
        )"
    };

    const char* gGlobalTransformDef = {
        R"(
            layout(std430, binding = 3) buffer InstanceTransformData
            {
                mat4 models[];
            } gInstanceTransforms;
        )"
    };

    Shader::Shader(const ShaderSource& shaderSource)
        : source(shaderSource)
    {
        build();
        init();
    }

    void Shader::build()
    {
        switch (source.type)
        {
        case ShaderSource::Type::kVsPs:
            buildVsPs();
            break;
        case ShaderSource::Type::kVsGsPs:
            buildVsGsPs();
            break;
        case ShaderSource::Type::kCs:
            buildCs();
            break;
        default:
            break;
        }
    }

    Shader::UniformMetaData::Type translate(GLenum glType)
    {
        using UniformType = Shader::UniformMetaData::Type;

        switch (glType) 
        {
        case GL_SAMPLER_2D:
            return UniformType::kSampler2D;
        case GL_SAMPLER_2D_ARRAY:
            return UniformType::kSampler2DArray;
        case GL_SAMPLER_3D:
            return UniformType::kSampler3D;
        case GL_SAMPLER_2D_SHADOW:
            return UniformType::kSamplerShadow;
        case GL_SAMPLER_CUBE:
            return UniformType::kSamplerCube;
        case GL_IMAGE_3D:
            return UniformType::kImage3D;
        case GL_UNSIGNED_INT_IMAGE_3D:
            return UniformType::kImageUI3D;
        case GL_UNSIGNED_INT:
            return UniformType::kUint;
        case GL_UNSIGNED_INT_ATOMIC_COUNTER:
            return UniformType::kAtomicUint;
        case GL_INT:
            return UniformType::kInt;
        case GL_FLOAT:
            return UniformType::kFloat;
        case GL_FLOAT_MAT4:
            return UniformType::kMat4;
        case GL_FLOAT_VEC3:
            return UniformType::kVec3;
        case GL_FLOAT_VEC4:
            return UniformType::kVec4;
        default:
            return UniformType::kCount;
        }
    }

    void Shader::init()
    {
        i32 numActiveUniforms, nameMaxLen;
        glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numActiveUniforms);
        glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &nameMaxLen);
        char* name = (char*)alloca(nameMaxLen + 1);
        for (u32 i = 0; i < numActiveUniforms; ++i)
        {
            GLenum type; i32 num;
            glGetActiveUniform(program, i, nameMaxLen, nullptr, &num, &type, name);
            UniformMetaData uniformMetaData = { };
            for (u32 ii = 1; ii < num; ++ii)
            {
                char* token = strtok(name, "[");
                char suffix[5] = "[%u]";
                char* format = strcat(token, suffix);
                sprintf(name, format, ii);
            }
            uniformMetaData.type = translate(type);
            uniformMetaData.name = std::string(name);
            uniformMetaData.location = glGetUniformLocation(program, name);
            uniformMetaDataMap.insert({ uniformMetaData.name.c_str(), uniformMetaData });
        }
    }

    void Shader::buildVsPs()
    {
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        program = glCreateProgram();
        // load shader source
        const char* vertShaderSrc = readShaderFile(source.vsFilePath);
        const char* fragShaderSrc = readShaderFile(source.psFilePath);
        glShaderSource(vs, 1, &vertShaderSrc, nullptr);
        glShaderSource(fs, 1, &fragShaderSrc, nullptr);
        // compile
        glCompileShader(vs);
        glCompileShader(fs);
        checkShaderCompilation(vs);
        checkShaderCompilation(fs);
        // link
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        checkShaderLinkage(program);
        glDeleteShader(vs);
        glDeleteShader(fs);
    }

    void Shader::buildVsGsPs()
    {
        const char* vsSrc = readShaderFile(source.vsFilePath);
        const char* gsSrc = readShaderFile(source.gsFilePath);
        const char* fsSrc = readShaderFile(source.psFilePath);
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        GLuint gs = glCreateShader(GL_GEOMETRY_SHADER);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        program = glCreateProgram();
        // load shader source
        glShaderSource(vs, 1, &vsSrc, nullptr);
        glShaderSource(gs, 1, &gsSrc, nullptr);
        glShaderSource(fs, 1, &fsSrc, nullptr);
        // compile 
        glCompileShader(vs);
        glCompileShader(gs);
        glCompileShader(fs);
        checkShaderCompilation(vs);
        checkShaderCompilation(gs);
        checkShaderCompilation(fs);
        // link
        glAttachShader(program, vs);
        glAttachShader(program, gs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        checkShaderLinkage(program);
        glDeleteShader(vs);
        glDeleteShader(gs);
        glDeleteShader(fs);
    }

    void Shader::buildCs()
    {
        const char* csSrc = readShaderFile(source.csFilePath);
        GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
        program = glCreateProgram();
        // load shader source
        glShaderSource(cs, 1, &csSrc, nullptr);
        // compile
        glCompileShader(cs);
        checkShaderCompilation(cs);
        // link
        glAttachShader(program, cs);
        glLinkProgram(program);
        checkShaderLinkage(program);
        glDeleteShader(cs);
    }

    i32 Shader::getUniformLocation(const char* name)
    {
        auto entry = uniformMetaDataMap.find(name);
        if (entry == uniformMetaDataMap.end())
        {
            return -1;
        }
        return entry->second.location;
    }

#define SET_UNIFORM(func, ...)               \
    i32 location = getUniformLocation(name); \
    if (location > 0)                        \
    {                                        \
        func(program, location, __VA_ARGS__);\
    }                                        \
    return *this;                            \

    void Shader::bind()
    {
        glUseProgram(program);
    }

    void Shader::unbind()
    {
        glUseProgram(0);
    }

    Shader& Shader::setUniform(const char* name, f32 data)
    {
        SET_UNIFORM(glProgramUniform1f, data)
    }

    Shader& Shader::setUniform(const char* name, u32 data)
    {
        SET_UNIFORM(glProgramUniform1ui, data)
    }

    Shader& Shader::setUniform(const char* name, i32 data)
    {
        SET_UNIFORM(glProgramUniform1i, data)
    }

    Shader& Shader::setUniform(const char* name, const glm::vec2& data)
    {
        SET_UNIFORM(glProgramUniform2f, data.x, data.y)
    }

    Shader& Shader::setUniform(const char* name, const glm::vec3& data)
    {
        SET_UNIFORM(glProgramUniform3f, data.x, data.y, data.z)
    }
    
    Shader& Shader::setUniform(const char* name, const glm::vec4& data)
    {
        SET_UNIFORM(glProgramUniform4f, data.x, data.y, data.z, data.w)
    }

    Shader& Shader::setUniform(const char* name, const glm::mat4& data)
    {
        SET_UNIFORM(glProgramUniformMatrix4fv, 1, false, &data[0][0])
    }

    Shader& Shader::setTexture(const char* samplerName, ITextureRenderable* texture)
    {
        const auto& entry = samplerBindingMap.find(samplerName);
        if (entry == samplerBindingMap.end())
        {
            samplerBindingMap.insert({ samplerName, texture });
        }
        else
        {
            entry->second = texture;
        }
        return *this;
    }

    Shader& Shader::setTextureBindings(GfxContext* ctx)
    {
        for (const auto& entry : samplerBindingMap)
        {
            const char* sampler = entry.first;
            ITextureRenderable* texture = entry.second;
            i32 binding = ctx->setTransientTexture(texture);
            setUniform(sampler, binding);
        }
        return *this;
    }

    ShaderManager* Singleton<ShaderManager>::singleton = nullptr;

    ShaderManager::ShaderSourceMap ShaderManager::m_shaderSourceMap = {
        { "PBRShader",                { ShaderSource::Type::kVsPs, "PBRShader", SHADER_SOURCE_PATH "pbs_v.glsl", SHADER_SOURCE_PATH "pbs_p.glsl" } },
        { "SSAOShader",               { ShaderSource::Type::kVsPs, "SSAOShader", SHADER_SOURCE_PATH "shader_ao.vs", SHADER_SOURCE_PATH "shader_ao.fs" } },
        { "ConstantColorShader",      { ShaderSource::Type::kVsPs, "ConstantColorShader", SHADER_SOURCE_PATH "shader_flat_color.vs", SHADER_SOURCE_PATH "shader_flat_color.fs" } },
        { "BloomSetupShader",         { ShaderSource::Type::kVsPs, "BloomSetupShader", SHADER_SOURCE_PATH "shader_bloom_preprocess.vs", SHADER_SOURCE_PATH "shader_bloom_preprocess.fs" } },
        { "BloomDownSampleShader",    { ShaderSource::Type::kVsPs, "BloomDownSampleShader", SHADER_SOURCE_PATH "shader_downsample.vs", SHADER_SOURCE_PATH "shader_downsample.fs" } },
        { "UpSampleShader",           { ShaderSource::Type::kVsPs, "UpSampleShader", SHADER_SOURCE_PATH "shader_upsample.vs", SHADER_SOURCE_PATH "shader_upsample.fs" } },
        { "GaussianBlurShader",       { ShaderSource::Type::kVsPs, "GaussianBlurShader", SHADER_SOURCE_PATH "shader_gaussian_blur.vs", SHADER_SOURCE_PATH "shader_gaussian_blur.fs" } },
        { "CompositeShader",          { ShaderSource::Type::kVsPs, "CompositeShader", SHADER_SOURCE_PATH "composite_v.glsl", SHADER_SOURCE_PATH "composite_p.glsl"} },
        { "VctxShader",               { ShaderSource::Type::kVsPs, "VctxShader", SHADER_SOURCE_PATH "vctx_v.glsl", SHADER_SOURCE_PATH "vctx_p.glsl" } },
        { "SceneDepthNormalShader",   { ShaderSource::Type::kVsPs, "SceneDepthNormalShader", SHADER_SOURCE_PATH "scene_depth_normal_v.glsl", SHADER_SOURCE_PATH "scene_depth_normal_p.glsl" } },
        { "SSAOShader",               { ShaderSource::Type::kVsPs, "SSAOShader", SHADER_SOURCE_PATH "shader_ao.vs", SHADER_SOURCE_PATH "shader_ao.fs" } },
        { "SceneDepthNormalShader",   { ShaderSource::Type::kVsPs, "SceneDepthNormalShader" SHADER_SOURCE_PATH "scene_depth_normal_v.glsl", SHADER_SOURCE_PATH "scene_depth_normal_p.glsl" } },
        { "DebugShadingShader",       { ShaderSource::Type::kVsPs, "DebugShadingShader", SHADER_SOURCE_PATH "debug_color_vs.glsl", SHADER_SOURCE_PATH "debug_color_fs.glsl" } },
        { "SDFSkyShader",             { ShaderSource::Type::kVsPs, "SDFSkyShader", SHADER_SOURCE_PATH "sky_sdf_v.glsl", SHADER_SOURCE_PATH "sky_sdf_p.glsl" } },
        { "SkyDomeShader",            { ShaderSource::Type::kVsPs, "SkyDomeShader", SHADER_SOURCE_PATH "skybox_v.glsl", SHADER_SOURCE_PATH "skybox_p.glsl" } },
        { "RenderToCubemapShader",    { ShaderSource::Type::kVsPs, "RenderToCubemapShader", SHADER_SOURCE_PATH "render_to_cubemap_v.glsl", SHADER_SOURCE_PATH "render_to_cubemap_p.glsl" } },
        { "ConvolveReflectionShader", { ShaderSource::Type::kVsPs, "ConvolveReflectionShader", SHADER_SOURCE_PATH "convolve_specular_v.glsl", SHADER_SOURCE_PATH "convolve_specular_p.glsl" } },
        { "ConvolveIrradianceShader", { ShaderSource::Type::kVsPs, "ConvolveIrradianceShader", SHADER_SOURCE_PATH "convolve_diffuse_v.glsl", SHADER_SOURCE_PATH "convolve_diffuse_p.glsl" } },
        { "ConvolveReflectionShader", { ShaderSource::Type::kVsPs, "ConvolveReflectionShader", SHADER_SOURCE_PATH "convolve_specular_v.glsl", SHADER_SOURCE_PATH "convolve_specular_p.glsl" } },
        { "PointLightShader",         { ShaderSource::Type::kVsPs, "PointLightShader", SHADER_SOURCE_PATH "shader_light.vs", SHADER_SOURCE_PATH "shader_light.fs" } },
        { "DirShadowShader",          { ShaderSource::Type::kVsPs, "DirShadowShader", SHADER_SOURCE_PATH "shader_dir_shadow.vs", SHADER_SOURCE_PATH "shader_dir_shadow.fs" } },
        { "IntegrateBRDFShader",      { ShaderSource::Type::kVsPs, "IntegrateBRDFShader", SHADER_SOURCE_PATH "shader_integrate_brdf.vs", SHADER_SOURCE_PATH  "shader_integrate_brdf.fs" } },
        { "RenderProbeShader",        { ShaderSource::Type::kVsPs, "RenderProbeShader", SHADER_SOURCE_PATH "shader_render_probe.vs", SHADER_SOURCE_PATH  "shader_render_probe.fs"} },
        { "IntegrateBRDFShader",      { ShaderSource::Type::kVsPs, "IntegrateBRDFShader", SHADER_SOURCE_PATH "shader_integrate_brdf.vs", SHADER_SOURCE_PATH  "shader_integrate_brdf.fs" } },
        { "RenderProbeShader",        { ShaderSource::Type::kVsPs, "RenderProbeShader", SHADER_SOURCE_PATH "shader_render_probe.vs", SHADER_SOURCE_PATH  "shader_render_probe.fs" } },
        { "LightMapShader",           { ShaderSource::Type::kVsPs, "LightMapShader", SHADER_SOURCE_PATH "shader_lightmap.vs", SHADER_SOURCE_PATH  "shader_lightmap.fs" } },
        { "VoxelizeResolveShader",    { ShaderSource::Type::kCs,   "VoxelizeResolveShader", nullptr, nullptr, nullptr, SHADER_SOURCE_PATH "voxelize_resolve_c.glsl" } },
        { "DebugShadingShader",       { ShaderSource::Type::kVsPs, "DebugShadingShader", SHADER_SOURCE_PATH "debug_color_vs.glsl", SHADER_SOURCE_PATH "debug_color_fs.glsl" } },
    };

    ShaderManager::ShaderMap ShaderManager::m_shaderMap;

    void ShaderManager::initialize()
    {
        for (const auto& entry : m_shaderSourceMap)
        {
            createShader(entry.second);
        }
    }

    Shader* ShaderManager::getShader(const char* shaderName)
    {
        auto entry = m_shaderMap.find(std::string(shaderName));
        if (entry == m_shaderMap.end())
        {
            return nullptr;
        }
        return entry->second.get();
    }

    Shader* ShaderManager::createShader(const ShaderSource& shaderSource)
    {
        Shader* shader = getShader(shaderSource.name);
        if (shader)
        {
            return shader;
        }

        // create a new shader
        m_shaderSourceMap.insert({ shaderSource.name, shaderSource });
        m_shaderMap.insert({ shaderSource.name, std::make_unique<Shader>(shaderSource) });

        return getShader(shaderSource.name);
    }
}