#include <fstream>
#include <unordered_map>
#include <stack>

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "json.hpp"

#include "Asset.h"
#include "CyanAPI.h"
#include "GfxContext.h"
#include "Camera.h"

// TODO: I did lots of new but never delete any memory allocation 

namespace glm {
    //---- Utilities for loading the scene data from json
    void from_json(const nlohmann::json& j, glm::vec3& v) 
    { 
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
    }

    void from_json(const nlohmann::json& j, glm::vec4& v) 
    { 
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
        v.w = j.at(3).get<float>();
    }
}

void from_json(const nlohmann::json& j, Transform& t) 
{
    t.m_translate = j.at("translation").get<glm::vec3>();
    glm::vec4 rotation = j.at("rotation").get<glm::vec4>();
    t.m_qRot = glm::quat(cos(RADIANS(rotation.x * 0.5f)), sin(RADIANS(rotation.x * 0.5f)) * glm::vec3(rotation.y, rotation.z, rotation.w));
    t.m_scale = j.at("scale").get<glm::vec3>();
}

void from_json(const nlohmann::json& j, Camera& c) 
{
    c.position = j.at("position").get<glm::vec3>();
    c.lookAt = j.at("lookAt").get<glm::vec3>();
    c.worldUp = j.at("worldUp").get<glm::vec3>();
    j.at("fov").get_to(c.fov);
    j.at("z_far").get_to(c.f);
    j.at("z_near").get_to(c.n);
}

namespace Cyan
{
    struct HandleAllocator
    {
        u32 m_nextHandle;
        u32 m_numAllocated;

        u32 alloc()
        {
            u32 handle = m_nextHandle;
            m_nextHandle += 1;
            m_numAllocated += 1;
            return handle;
        }
    };

    static const u32 kMaxNumUniforms = 256;
    static const u32 kMaxNumShaders = 64;
    static const u32 kMaxNumMaterialTypes = 64;
    static const u32 kMaxNumSceneNodes = 256;

    // TODO: Where do these live in memory...?
    static std::vector<Texture*> s_textures;
    static std::vector<Mesh*> s_meshes;
    static std::vector<Uniform*> s_uniforms;
    static std::vector<LightProbe> s_probes;
    static SceneNode s_sceneNodes[kMaxNumSceneNodes];
    static GfxContext* s_gfxc = nullptr;
    static void* s_memory = nullptr;
    static LinearAllocator* s_allocator = nullptr;

    static std::unordered_map<std::string, u32> s_uniformRegistry;
    static std::unordered_map<std::string, u32> s_shaderRegistry;

    static HandleAllocator s_uniformHandleAllocator = { };
    static HandleAllocator s_shaderHandleAllocator = { };
    static Uniform* m_uniforms[kMaxNumUniforms];

    // each material type share same handle as its according shader 
    static Shader* m_shaders[kMaxNumShaders];
    static Material* m_materialTypes[kMaxNumMaterialTypes];

    static UniformBuffer* m_uniformBuffer = nullptr;

    static u32 m_numUniforms = 0u;
    static u32 m_numSceneNodes = 0u;

    void init()
    {
        CYAN_ASSERT(!s_gfxc, "Graphics context was already initialized");
        // Create global graphics context
        s_gfxc = new GfxContext;
        s_gfxc->init();
        s_memory = (void*)(new char[1024 * 1024 * 1024]); // 1GB memory pool
        s_allocator = LinearAllocator::create(s_memory, 1024 * 1024 * 1024);
        m_uniformBuffer = createUniformBuffer();
    }

    void* alloc(u32 sizeInBytes)
    {
        return s_allocator->alloc(sizeInBytes);
    }

    u32 allocUniformHandle()
    {
        return s_uniformHandleAllocator.alloc();
    }

    u32 allocShaderHandle()
    {
        return s_shaderHandleAllocator.alloc();
    }

    SceneNode* allocSceneNode()
    {
        CYAN_ASSERT(m_numSceneNodes < kMaxNumSceneNodes, "Too many scene nodes created!!")
        s_sceneNodes[m_numSceneNodes].m_parent = nullptr;
        s_sceneNodes[m_numSceneNodes].m_entity = nullptr;
        return &s_sceneNodes[m_numSceneNodes++]; 
    }

    void shutDown()
    {
        delete s_gfxc;
    }

    GfxContext* getCurrentGfxCtx()
    {
        CYAN_ASSERT(s_gfxc, "Graphics context was not initialized!");
        return s_gfxc;
    }

    RegularBuffer* createRegularBuffer(u32 totalSizeInBytes)
    {
        RegularBuffer* buffer = new RegularBuffer;
        buffer->m_totalSize = totalSizeInBytes;
        buffer->m_sizeToUpload = 0u;
        buffer->m_data = nullptr;
        glCreateBuffers(1, &buffer->m_ssbo);
        glNamedBufferData(buffer->m_ssbo, buffer->m_totalSize, nullptr, GL_DYNAMIC_COPY);
        return buffer;
    }

    VertexBuffer* createVertexBuffer(void* _data, u32 _sizeInBytes, u32 _strideInBytes, u32 _numVerts)
    {
        VertexBuffer* vb = new VertexBuffer;
        u32 offset = 0;
        vb->m_data = _data;
        vb->m_strideInBytes = _strideInBytes;
        vb->m_numVerts = _numVerts;
        glCreateBuffers(1, &vb->m_vbo);
        glNamedBufferData(vb->m_vbo, _sizeInBytes, vb->m_data, GL_STATIC_DRAW);
        return vb;
    }

    VertexArray* createVertexArray(VertexBuffer* vb)
    {
        VertexArray* vertexArray = new VertexArray;
        glCreateVertexArrays(1, &vertexArray->m_vao);
        vertexArray->m_vertexBuffer = vb;
        vertexArray->m_ibo = static_cast<u32>(-1);
        vertexArray->m_numIndices = static_cast<u32>(-1);
        return vertexArray;
    }

    void addMesh(Mesh* mesh) {
        s_meshes.push_back(mesh);
    }

    // TODO: switch to use tinygltf for loading gltf files
    Mesh* createMesh(const char* _name, const char* _file)
    {
        Mesh* mesh = new Mesh;
        mesh->m_name = _name;

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            _file,
            aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals
        );
        float** vertexDataPtrs = reinterpret_cast<float**>(_alloca(sizeof(float*) * scene->mNumMeshes));
        for (u32 sm = 0u; sm < scene->mNumMeshes; sm++) 
        {
            Mesh::SubMesh* subMesh = new Mesh::SubMesh;

            u8 attribFlag = 0x0;
            u32 strideInBytes = 0;
            attribFlag |= scene->mMeshes[sm]->HasPositions() ? VertexAttribFlag::kPosition : 0x0;
            attribFlag |= scene->mMeshes[sm]->HasTextureCoords(0) ? VertexAttribFlag::kTexcoord : 0x0;
            attribFlag |= scene->mMeshes[sm]->HasNormals() ? VertexAttribFlag::kNormal : 0x0;
            attribFlag |= scene->mMeshes[sm]->HasTangentsAndBitangents() ? VertexAttribFlag::kTangents : 0x0;

            strideInBytes += (attribFlag & VertexAttribFlag::kPosition) > 0 ? 3 * sizeof(f32) : 0;
            strideInBytes += (attribFlag & VertexAttribFlag::kNormal)   > 0 ? 3 * sizeof(f32) : 0;
            strideInBytes += (attribFlag & VertexAttribFlag::kTangents) > 0 ? 4 * sizeof(f32) : 0;
            strideInBytes += (attribFlag & VertexAttribFlag::kTexcoord) > 0 ? 2 * sizeof(f32) : 0;

            u32 numVerts = (u32)scene->mMeshes[sm]->mNumVertices;
            vertexDataPtrs[sm] = new float[strideInBytes * numVerts];
            float* data = vertexDataPtrs[sm];

            subMesh->m_numVerts = numVerts;

            for (u32 v = 0u; v < numVerts; v++)
            {
                u32 offset = 0;
                float* vertexAddress = (float*)((u8*)data + strideInBytes * v);
                if (attribFlag & VertexAttribFlag::kPosition)
                {
                    vertexAddress[offset++] = scene->mMeshes[sm]->mVertices[v].x;
                    vertexAddress[offset++] = scene->mMeshes[sm]->mVertices[v].y;
                    vertexAddress[offset++] = scene->mMeshes[sm]->mVertices[v].z;
                }
                if (attribFlag & VertexAttribFlag::kNormal)
                {
                    vertexAddress[offset++] = scene->mMeshes[sm]->mNormals[v].x;
                    vertexAddress[offset++] = scene->mMeshes[sm]->mNormals[v].y;
                    vertexAddress[offset++] = scene->mMeshes[sm]->mNormals[v].z;
                }
                if (attribFlag & VertexAttribFlag::kTangents)
                {
                    vertexAddress[offset++] = scene->mMeshes[sm]->mTangents[v].x;
                    vertexAddress[offset++] = scene->mMeshes[sm]->mTangents[v].y;
                    vertexAddress[offset++] = scene->mMeshes[sm]->mTangents[v].z;
                    vertexAddress[offset++] = 1.f;
                }
                if (attribFlag & VertexAttribFlag::kTexcoord)
                {
                    vertexAddress[offset++] = scene->mMeshes[sm]->mTextureCoords[0][v].x;
                    vertexAddress[offset++] = scene->mMeshes[sm]->mTextureCoords[0][v].y;
                }
            }

            subMesh->m_vertexArray = createVertexArray(createVertexBuffer(data, strideInBytes * numVerts, strideInBytes, numVerts));
            mesh->m_subMeshes.push_back(subMesh);

            u32 offset = 0;
            // FIXME: last parameter in VertexAttrib initialization is wrong
            if (attribFlag & VertexAttribFlag::kPosition)
            {
                subMesh->m_vertexArray->m_vertexBuffer->m_vertexAttribs.push_back({
                    VertexAttrib::DataType::Float, 3, strideInBytes, offset, (float*)subMesh->m_vertexArray->m_vertexBuffer->m_data + offset});
                offset += sizeof(f32) * 3;
            }
            if (attribFlag & VertexAttribFlag::kNormal)
            {
                subMesh->m_vertexArray->m_vertexBuffer->m_vertexAttribs.push_back({
                    VertexAttrib::DataType::Float, 3, strideInBytes, offset, (float*)subMesh->m_vertexArray->m_vertexBuffer->m_data + offset});
                offset += sizeof(f32) * 3;
            }
            if (attribFlag & VertexAttribFlag::kTangents)
            {
                subMesh->m_vertexArray->m_vertexBuffer->m_vertexAttribs.push_back({
                    VertexAttrib::DataType::Float, 4, strideInBytes, offset, (float*)subMesh->m_vertexArray->m_vertexBuffer->m_data + offset});
                offset += sizeof(f32) * 4;
            }
            if (attribFlag & VertexAttribFlag::kTexcoord)
            {
                subMesh->m_vertexArray->m_vertexBuffer->m_vertexAttribs.push_back({
                    VertexAttrib::DataType::Float, 2, strideInBytes, offset, (float*)subMesh->m_vertexArray->m_vertexBuffer->m_data + offset});
                offset += sizeof(f32) * 2;
            }
            subMesh->m_vertexArray->init();
        }
        // Store the xform for normalizing object space mesh coordinates
        mesh->m_normalization = Toolkit::computeMeshNormalization(mesh);
        addMesh(mesh);
        for (u32 sm = 0u; sm < scene->mNumMeshes; ++sm) {
            delete[] vertexDataPtrs[sm];
        }
        return mesh;
    }

    Mesh* getMesh(const char* _name)
    {
        if (_name)
        {
            for (auto mesh : s_meshes)
            {
                if (strcmp(mesh->m_name.c_str(), _name) == 0)
                {
                    return mesh; 
                }
            }
        }
        return 0;
    }

    Scene* createScene(const char* name, const char* _file)
    {
        Scene* scene = new Scene;
        scene->m_name = std::string(name);

        scene->m_lastProbe = nullptr;
        scene->m_currentProbe = nullptr;
        scene->m_root = nullptr;
        // create root entity
        Entity* rootEntity = SceneManager::createEntity(scene, "SceneRoot", nullptr, Transform(), true);
        SceneManager::createSceneNode(scene, nullptr, rootEntity);

        nlohmann::json sceneJson;
        std::ifstream sceneFile(_file);
        sceneFile >> sceneJson;
        auto cameras = sceneJson["cameras"];
        auto meshInfoList = sceneJson["meshes"];
        auto textureInfoList = sceneJson["textures"];
        auto entities = sceneJson["entities"];

        // create buffer size that can contain max number of lights
        scene->m_dirLightsBuffer = createRegularBuffer(Scene::kMaxNumDirLights * sizeof(DirectionalLight));
        scene->m_pointLightsBuffer = createRegularBuffer(Scene::kMaxNumPointLights * sizeof(PointLight));

        // TODO: each scene should only have one camera
        for (auto camera : cameras) 
        {
            scene->mainCamera = camera.get<Camera>();
            scene->mainCamera.view = glm::mat4(1.f);
        }

        for (auto textureInfo : textureInfoList) 
        {
            std::string filename = textureInfo.at("path").get<std::string>();
            std::string name     = textureInfo.at("name").get<std::string>();
            std::string dynamicRange = textureInfo.at("dynamic_range").get<std::string>();
            u32 numMips = textureInfo.at("numMips").get<u32>();
            
            TextureSpec spec = { };
            spec.m_type = Texture::Type::TEX_2D;
            spec.m_numMips = numMips;
            spec.m_min = Texture::Filter::LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::NONE;
            spec.m_t = Texture::Wrap::NONE;
            spec.m_r = Texture::Wrap::NONE;
            if (dynamicRange == "ldr")
            {
                createTexture(name.c_str(), filename.c_str(), spec);
            }
            else if (dynamicRange == "hdr")
            {
                createTextureHDR(name.c_str(), filename.c_str(), spec);
            }
        }

        for (auto meshInfo : meshInfoList) 
        {
            std::string path, name;
            meshInfo.at("path").get_to(path);
            meshInfo.at("name").get_to(name);
            // special case for gltf-2.0 for now
            if (path.find(".gltf") != std::string::npos) {
                AssetManager assetManager;
                assetManager.loadGltf(scene, path.c_str());
            } else {
                createMesh(name.c_str(), path.c_str());
            }
        }

        for (auto entityInfo : entities) 
        {
            auto findMesh = [](const char* _name) {
                for (auto mesh : s_meshes)
                {
                    if (mesh->m_name == _name)
                        return mesh;
                }
            };

            std::string meshName;
            std::string entityName;
            entityInfo.at("mesh").get_to(meshName);
            entityInfo.at("name").get_to(entityName);
            auto xformInfo = entityInfo.at("xform");
            Transform xform = entityInfo.at("xform").get<Transform>();
            Entity* entity = SceneManager::createEntity(scene, entityName.c_str(), meshName.c_str(), xform, true);
            SceneManager::createSceneNode(scene, nullptr, entity);
        }
        return scene;
    }

    Shader* createShader(const char* name, const char* vertSrc, const char* fragSrc)
    {
        using ShaderEntry = std::pair<std::string, u32>;

        // found a existing shader
        auto itr = s_shaderRegistry.find(std::string(name));
        if (itr != s_shaderRegistry.end())
        {
            return m_shaders[itr->second];
        }

        // TODO: Memory management
        u32 handle = ALLOC_HANDLE(Shader)
        CYAN_ASSERT(handle < kMaxNumShaders,  "Too many shader created!!!")
        m_shaders[handle] = new Shader();
        Shader* shader = m_shaders[handle];
        shader->m_name = std::string(name);
        s_shaderRegistry.insert(ShaderEntry(shader->m_name, handle));
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        shader->m_programId = glCreateProgram();
        // Load shader source
        const char* vertShaderSrc = ShaderUtil::readShaderFile(vertSrc);
        const char* fragShaderSrc = ShaderUtil::readShaderFile(fragSrc);
        glShaderSource(vs, 1, &vertShaderSrc, nullptr);
        glShaderSource(fs, 1, &fragShaderSrc, nullptr);
        // Compile shader
        glCompileShader(vs);
        glCompileShader(fs);
        ShaderUtil::checkShaderCompilation(vs);
        ShaderUtil::checkShaderCompilation(fs);
        // Link shader
        glAttachShader(shader->m_programId, vs);
        glAttachShader(shader->m_programId, fs);
        glLinkProgram(shader->m_programId);
        ShaderUtil::checkShaderLinkage(shader->m_programId);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return shader;
    }

    RenderTarget* createRenderTarget(u32 _width, u32 _height)
    {
        RenderTarget* rt = new RenderTarget();
        rt->m_width = _width;
        rt->m_height = _height;
        rt->m_numColorBuffers = 0;
        glCreateFramebuffers(1, &rt->m_frameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, rt->m_frameBuffer);
        {
            glCreateRenderbuffers(1, &rt->m_renderBuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, rt->m_renderBuffer);
            {
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, rt->m_width, rt->m_height);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt->m_renderBuffer);
            }
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        rt->validate();
        return rt;
    }

    static GLenum convertTexFilter(Texture::Filter filter)
    {
        switch(filter)
        {
            case Texture::Filter::LINEAR:
                return GL_LINEAR;
            case Texture::Filter::MIPMAP_LINEAR:
                return GL_LINEAR_MIPMAP_LINEAR;
            default:
                break;
        }
    }

    static GLenum convertTexWrap(Texture::Wrap wrap)
    {
        switch(wrap)
        {
            case Texture::Wrap::CLAMP_TO_EDGE:
                return GL_CLAMP_TO_EDGE;
            default:
                break;
        }
    }

    struct TextureSpecGL
    {
        GLenum m_typeGL;
        GLenum m_dataFormatGL;
        GLenum m_internalFormatGL;
        GLenum m_minGL;
        GLenum m_magGL;
        u8     m_wrapFlag;
        GLenum m_sGL;
        GLenum m_tGL;
        GLenum m_rGL;
    };

    static TextureSpecGL translate(TextureSpec& spec)
    {
        struct DataFormatGL
        {
            GLenum m_dataFormatGL;
            GLenum m_internalFormatGL;
        };

        auto convertTexType = [](Texture::Type type) {
            switch(type)
            {
                case Texture::Type::TEX_2D:
                    return GL_TEXTURE_2D;
                case Texture::Type::TEX_CUBEMAP:
                    return GL_TEXTURE_CUBE_MAP;
                default:
                    CYAN_ASSERT(0, "Undefined texture type.")
                    return GL_INVALID_ENUM;
            }
        };

        auto convertDataFormat = [](Texture::ColorFormat format) {
            switch (format)
            {
                case Texture::ColorFormat::R8G8B8: 
                    return DataFormatGL{ GL_RGB8, GL_RGB };
                case Texture::ColorFormat::R8G8B8A8: 
                    return DataFormatGL{ GL_RGBA8, GL_RGBA };
                case Texture::ColorFormat::R16G16B16: 
                    return DataFormatGL{ GL_RGB16F, GL_RGB };
                case Texture::ColorFormat::R16G16B16A16:
                    return DataFormatGL{ GL_RGBA16F, GL_RGBA };
                default:
                    CYAN_ASSERT(0, "Undefined texture color format.")
                    return DataFormatGL{ GL_INVALID_ENUM, GL_INVALID_ENUM };
            }
        };
         
       auto convertTexFilter = [](Texture::Filter filter) {
            switch(filter)
            {
                case Texture::Filter::LINEAR:
                    return GL_LINEAR;
                case Texture::Filter::MIPMAP_LINEAR:
                    return GL_LINEAR_MIPMAP_LINEAR;
                default:
                    CYAN_ASSERT(0, "Undefined texture filter parameter.")
                    return GL_INVALID_ENUM;
            }
        };

        auto convertTexWrap = [](Texture::Wrap wrap) {
            switch(wrap)
            {
                case Texture::Wrap::CLAMP_TO_EDGE:
                    return GL_CLAMP_TO_EDGE;
                case Texture::Wrap::NONE:
                    return 0;
                default:
                    CYAN_ASSERT(0, "Undefined texture wrap parameter.")
                    return GL_INVALID_ENUM;
            }
        };

        TextureSpecGL specGL = { };
        // type
        specGL.m_typeGL = convertTexType(spec.m_type);
        // format
        DataFormatGL format = convertDataFormat(spec.m_format);
        specGL.m_dataFormatGL = format.m_dataFormatGL;
        specGL.m_internalFormatGL = format.m_internalFormatGL;
        // filters
        specGL.m_minGL = convertTexFilter(spec.m_min);
        specGL.m_magGL = convertTexFilter(spec.m_mag);
        // linear mipmap filtering
        if (spec.m_numMips > 1u)
        {
            specGL.m_minGL = convertTexFilter(Texture::Filter::MIPMAP_LINEAR);
        }
        // wraps
        auto wrapS = convertTexWrap(spec.m_s);
        auto wrapT = convertTexWrap(spec.m_t);
        auto wrapR = convertTexWrap(spec.m_r);
        if (wrapS > 0)
        {
            specGL.m_sGL = wrapS;
            specGL.m_wrapFlag |= (1 << 0);
        }
        if (wrapT > 0)
        {
            specGL.m_tGL = wrapT;
            specGL.m_wrapFlag |= (1 << 1);
        }
        if (wrapR > 0)
        {
            specGL.m_rGL = wrapR;
            specGL.m_wrapFlag |= (1 << 2);
        }
        return specGL;
    }

    // set texture parameters such as min/mag filters, wrap_s, wrap_t, and wrap_r
    static void setTextureParameters(Texture* texture, TextureSpecGL& specGL)
    {
        glBindTexture(specGL.m_typeGL, texture->m_id);
        glTexParameteri(specGL.m_typeGL, GL_TEXTURE_MIN_FILTER, specGL.m_minGL);
        glTexParameteri(specGL.m_typeGL, GL_TEXTURE_MAG_FILTER, specGL.m_magGL);
        if (specGL.m_wrapFlag & (1 << 0 ))
        {
            glTexParameteri(specGL.m_typeGL, GL_TEXTURE_WRAP_S, specGL.m_sGL);
        }
        if (specGL.m_wrapFlag & (1 << 1))
        {
            glTexParameteri(specGL.m_typeGL, GL_TEXTURE_WRAP_T, specGL.m_tGL);
        }
        if (specGL.m_wrapFlag & (1 << 2))
        {
            glTexParameteri(specGL.m_typeGL, GL_TEXTURE_WRAP_R, specGL.m_rGL);
        }
        glBindTexture(specGL.m_typeGL, 0);
    }
    
    Texture* createTexture(const char* _name, TextureSpec spec)
    {
        Texture* texture = new Texture();

        texture->m_name = _name;
        texture->m_width = spec.m_width;
        texture->m_height = spec.m_height;
        texture->m_type = spec.m_type;
        texture->m_format = spec.m_format;
        texture->m_minFilter = spec.m_min;
        texture->m_magFilter = spec.m_mag;
        texture->m_wrapS = spec.m_s; 
        texture->m_wrapT = spec.m_t;
        texture->m_wrapR = spec.m_r;
        texture->m_data = spec.m_data;

        TextureSpecGL specGL = translate(spec);

        glCreateTextures(specGL.m_typeGL, 1, &texture->m_id);
        glBindTexture(specGL.m_typeGL, texture->m_id);
        glTexImage2D(specGL.m_typeGL, 0, specGL.m_dataFormatGL, texture->m_width, texture->m_height, 0, specGL.m_internalFormatGL, GL_UNSIGNED_BYTE, texture->m_data);
        glBindTexture(GL_TEXTURE_2D, 0);
        setTextureParameters(texture, specGL);
        if (spec.m_numMips > 1u)
        {
            glGenerateTextureMipmap(texture->m_id);
        }

        return texture;
    }

    Texture* createTextureHDR(const char* _name, TextureSpec spec)
    {
        Texture* texture = new Texture();

        texture->m_name = _name;
        texture->m_width = spec.m_width;
        texture->m_height = spec.m_height;
        texture->m_type = spec.m_type;
        texture->m_format = spec.m_format;
        texture->m_minFilter = spec.m_min;
        texture->m_magFilter = spec.m_mag;
        texture->m_wrapS = spec.m_s; 
        texture->m_wrapT = spec.m_t;
        texture->m_wrapR = spec.m_r;
        texture->m_data = spec.m_data;

        TextureSpecGL specGL = translate(spec);

        glCreateTextures(specGL.m_typeGL, 1, &texture->m_id);
        glBindTexture(specGL.m_typeGL, texture->m_id);
        switch(spec.m_type)
        {
            case Texture::Type::TEX_2D:
                glTexImage2D(GL_TEXTURE_2D, 0, specGL.m_dataFormatGL, texture->m_width, texture->m_height, 0, specGL.m_internalFormatGL, GL_FLOAT, texture->m_data);
                break;
            case Texture::Type::TEX_CUBEMAP:
                for (int f = 0; f < 6; f++)
                {
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, specGL.m_dataFormatGL, texture->m_width, texture->m_height, 0, specGL.m_internalFormatGL, GL_FLOAT, nullptr);
                }
                break;
            default:
                break;
        }
        glBindTexture(specGL.m_typeGL, 0);
        setTextureParameters(texture, specGL);
        if (spec.m_numMips > 1u)
        {
            glGenerateTextureMipmap(texture->m_id);
        }

        return texture;
    }

    // when loading a texture from file, TextureSpec.m_format will be modified as 
    // the format is detmermined after loading the image, so @spec needs to be taken
    // as a reference
    Texture* createTexture(const char* _name, const char* _file, TextureSpec& spec)
    {
        int w, h, numChannels;
        Texture* texture = new Texture();
        stbi_set_flip_vertically_on_load(1);
        unsigned char* pixels = stbi_load(_file, &w, &h, &numChannels, 0);

        spec.m_format = numChannels == 3 ? Texture::ColorFormat::R8G8B8 : Texture::ColorFormat::R8G8B8A8;
        texture->m_name = _name;
        texture->m_width = w;
        texture->m_height = h;
        texture->m_type = spec.m_type;
        texture->m_format = spec.m_format;
        texture->m_minFilter = spec.m_min;
        texture->m_magFilter = spec.m_mag;
        texture->m_wrapS = spec.m_s; 
        texture->m_wrapT = spec.m_t;
        texture->m_wrapR = spec.m_r;
        texture->m_data = pixels;

        TextureSpecGL specGL = translate(spec);

        glCreateTextures(specGL.m_typeGL, 1, &texture->m_id);
        glTextureStorage2D(texture->m_id, spec.m_numMips, specGL.m_dataFormatGL, texture->m_width, texture->m_height);
        glTextureSubImage2D(texture->m_id, 0, 0, 0, texture->m_width, texture->m_height, specGL.m_internalFormatGL, GL_UNSIGNED_BYTE, texture->m_data);
        if (spec.m_numMips > 1u)
        {
            glGenerateTextureMipmap(texture->m_id);
        }
        s_textures.push_back(texture);
        stbi_image_free(pixels);
        return texture;
    }

    // when loading a texture from file, TextureSpec.m_format will be modified as 
    // the format is detmermined after loading the image, so @spec needs to be taken
    // as a reference
    Texture* createTextureHDR(const char* _name, const char* _file, TextureSpec& spec)
    {
        int w, h, numChannels;
        Texture* texture = new Texture();
        stbi_set_flip_vertically_on_load(1);
        float* pixels = stbi_loadf(_file, &w, &h, &numChannels, 0);

        spec.m_format = numChannels == 3 ? Texture::ColorFormat::R16G16B16 : Texture::ColorFormat::R16G16B16A16;
        texture->m_name = _name;
        texture->m_width = w;
        texture->m_height = h;
        texture->m_type = spec.m_type;
        texture->m_format = spec.m_format;
        texture->m_minFilter = spec.m_min;
        texture->m_magFilter = spec.m_mag;
        texture->m_wrapS = spec.m_s; 
        texture->m_wrapT = spec.m_t;
        texture->m_wrapR = spec.m_r;
        texture->m_data = pixels;

        TextureSpecGL specGL = translate(spec);

        glCreateTextures(specGL.m_typeGL, 1, &texture->m_id);
        glTextureStorage2D(texture->m_id, spec.m_numMips, specGL.m_dataFormatGL, texture->m_width, texture->m_height);
        glTextureSubImage2D(texture->m_id, 0, 0, 0, texture->m_width, texture->m_height, specGL.m_internalFormatGL, GL_FLOAT, texture->m_data);
        setTextureParameters(texture, specGL);
        if (spec.m_numMips > 1u)
        {
            glGenerateTextureMipmap(texture->m_id);
        }
        s_textures.push_back(texture);
        stbi_image_free(pixels);
        return texture;
    }

    void addTexture(Texture* texture) {
        s_textures.push_back(texture);
    }

    u32 getNumTextures() {
        return static_cast<u32>(s_textures.size());
    }

    Texture* getTexture(const char* _name)
    {
        for (auto texture :  s_textures)
        {
            if (strcmp(texture->m_name.c_str(), _name) == 0)
            {
                return texture;
            }
        }
        CYAN_ASSERT(0, "should not reach!") // Unreachable
        return 0;
    }

    Uniform* createUniform(const char* _name, Uniform::Type _type)
    {
        std::string key(_name); 
        auto itr = s_uniformRegistry.find(key);
        if (itr != s_uniformRegistry.end())
        {
            u32 handle = itr->second; 
            return m_uniforms[handle]; 
        }
        u32 handle = ALLOC_HANDLE(Uniform)
        m_uniforms[handle] = (Uniform*)s_allocator->alloc(sizeof(Uniform)); 
        Uniform* uniform = m_uniforms[handle];
        uniform->m_type = _type;
        strcpy(uniform->m_name, _name);
        u32 size = uniform->getSize();
        uniform->m_valuePtr = s_allocator->alloc(size);
        memset((u8*)uniform->m_valuePtr, 0x0, size);
        s_uniformRegistry.insert(std::pair<std::string,u32>(key, handle));
        m_numUniforms += 1;
        CYAN_ASSERT(m_numUniforms <= kMaxNumUniforms, "Too many uniforms created");
        return uniform;
    }

    UniformHandle getUniformHandle(const char* name)
    {
        std::string key(name);
        auto itr = s_uniformRegistry.find(key);
        if (itr != s_uniformRegistry.end())
        {
            return itr->second;
        }
        else
        {
            printf("[ERROR] Unknown uniform with name %s", name);
        }
        return (UniformHandle)-1;
    }

    Uniform* getUniform(UniformHandle handle)
    {
        CYAN_ASSERT(handle < kMaxNumUniforms, "Uniform handle %u out of bound", handle)
        return m_uniforms[handle];
    }

    UniformBuffer* createUniformBuffer(u32 sizeInBytes)
    {
        UniformBuffer* buffer = (UniformBuffer*)s_allocator->alloc(sizeof(UniformBuffer));
        buffer->m_size = sizeInBytes;
        buffer->m_pos = 0;
        buffer->m_data = s_allocator->alloc(sizeInBytes);
        return buffer;
    }

    Uniform* createShaderUniform(Shader* _shader, const char* _name, Uniform::Type _type)
    {
        Uniform* uniform = createUniform(_name, _type);
        // _shader->bindUniform(uniform);
        return uniform;
    }

    Uniform* createMaterialUniform(Material* _material, const char* _name, Uniform::Type _type)
    {
        Uniform* uniform = createUniform(_name, _type);
        _material->bindUniform(uniform);
        return uniform;
    }

    Uniform::Type glToCyanType(GLenum glType)
    {
        switch (glType)
        {
            case GL_SAMPLER_2D:
                return Uniform::Type::u_sampler2D;
            case GL_SAMPLER_CUBE:
                return Uniform::Type::u_samplerCube;
            case GL_UNSIGNED_INT:
                return Uniform::Type::u_uint;
            case GL_INT:
                return Uniform::Type::u_int;
            case GL_FLOAT:
                return Uniform::Type::u_float;
            case GL_FLOAT_MAT4:
                return Uniform::Type::u_mat4;
            case GL_FLOAT_VEC3:
                return Uniform::Type::u_vec3;
            case GL_FLOAT_VEC4:
                return Uniform::Type::u_vec4;
            default:
                printf("[ERROR]: Unhandled GL type when converting GL types to Cyan types \n");
                return Uniform::Type::u_undefined;
        }
    }

    LightProbe* getProbe(u32 index)
    {
        return &s_probes[index];
    }

    u32 getNumProbes()
    {
        return (u32)s_probes.size();
    }

    Material* createMaterial(Shader* _shader)
    {
        auto itr = s_shaderRegistry.find(_shader->m_name);
        CYAN_ASSERT(itr != s_shaderRegistry.end(), "Trying to create a Material type from an unregistered shader")
        if (m_materialTypes[itr->second])
        {
            return m_materialTypes[itr->second];
        }
        u32 handle = itr->second; 
        m_materialTypes[handle] = new Material;
        Material* material = m_materialTypes[handle];
        material->m_shader = _shader;
        material->m_bufferSize = 0u; // at least need to contain UniformBuffer::End
        material->m_numSamplers = 0u;
        material->m_numBuffers = 0u;
        material->m_dataFieldsFlag = 0u;
        material->m_lit = false;

        GLuint programId = material->m_shader->m_programId;
        GLsizei numActiveUniforms;
        glGetProgramiv(programId, GL_ACTIVE_UNIFORMS, &numActiveUniforms);
        GLint nameMaxLen;
        glGetProgramiv(programId, GL_ACTIVE_UNIFORM_MAX_LENGTH, &nameMaxLen);
        char* name = (char*)_malloca(nameMaxLen + 1);
        GLenum type;
        GLint num;
        for (u32 i = 0; i < numActiveUniforms; ++i)
        {
            glGetActiveUniform(programId, i, nameMaxLen, nullptr, &num, &type, name);
            if (nameMaxLen > 2 && name[0] == 's' && name[1] == '_')
            {
                // skipping predefined uniforms such as model, view, projection
                continue;
            }
            // TODO: this is obviously not optimal
            // TODO: how to determine if a material is lit or not
            if ((strcmp(name, "numPointLights") == 0) || 
                (strcmp(name, "numDirLights") == 0))
            {
                material->m_dataFieldsFlag |= (1 << Material::DataFields::Lit);
            }
            if (strcmp(name, "irradianceDiffuse") == 0)
            {
                material->m_dataFieldsFlag |= (1 << Material::DataFields::Probe);
            }
            for (u32 ii = 0; ii < num; ++ii)
            {
                // change the name for each array entry
                if (ii > 0)
                {
                    char* token = strtok(name, "[");
                    char suffix[5] = "[%u]";
                    char* format = strcat(token, suffix);
                    sprintf(name, format, ii);
                }
                Uniform::Type cyanType = glToCyanType(type);
                switch (cyanType)
                {
                    case Uniform::Type::u_sampler2D:
                    case Uniform::Type::u_samplerCube:
                        material->bindSampler(createUniform(name, cyanType));
                        break;
                    default:
                        createMaterialUniform(material, name, cyanType);
                        break;
                }
            }
        }
        // buffers ubo/ssbo
        i32 numActiveBlocks = 0, maxBlockNameLength = 0;
        glGetProgramInterfaceiv(programId, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &numActiveBlocks);
        glGetProgramInterfaceiv(programId, GL_SHADER_STORAGE_BLOCK, GL_MAX_NAME_LENGTH, &maxBlockNameLength);
        char* blockName = (char*)_alloca(maxBlockNameLength + 1);
        for (u32 i = 0; i < numActiveBlocks; ++i)
        {
            i32 length = 0;
            glGetProgramResourceName(programId, GL_SHADER_STORAGE_BLOCK, i, maxBlockNameLength + 1, &length, blockName);
            material->m_bufferBlocks[i] = std::string(blockName);
        }
        material->m_numBuffers = numActiveBlocks;
        material->finalize();
        return material;
    }

    #define CYAN_CHECK_UNIFORM(uniform)                                                          \
        std::string key(uniform->m_name);                                                        \
        auto itr = s_uniformRegistry.find(key);                                                 \
        CYAN_ASSERT(itr != s_uniformRegistry.end(), "Cannot find uniform %s", uniform->m_name); \

    // Notes(Min): for variables that are allocated from stack, we need to call ctx->setUniform
    //             before the variable goes out of scope
    void setUniform(Uniform* _uniform, void* _valuePtr)
    {
        CYAN_CHECK_UNIFORM(_uniform)

        u32 size = _uniform->getSize();
        u32 handle = itr->second; 
        // // write uniform handle
        // m_uniformBuffer->write((void*)&handle, sizeof(u32));
        // // write data
        // m_uniformBuffer->write(_valuePtr, size);

        memcpy(_uniform->m_valuePtr, _valuePtr, size);
    }

    void setUniform(Uniform* _uniform, u32 _value)
    {
        CYAN_CHECK_UNIFORM(_uniform)

        u32 handle = itr->second; 
        // int type in glsl is 32bits
        CYAN_ASSERT(_uniform->m_type == Uniform::Type::u_int, "mismatced uniform type, expecting unsigned int")
        // // write uniform handle
        // m_uniformBuffer->write(handle);
        // // write data 
        // m_uniformBuffer->write((void*)&_value, sizeof(u32));

        u32* ptr = static_cast<u32*>(_uniform->m_valuePtr); 
        *(u32*)(_uniform->m_valuePtr) = _value;
    }

    void setUniform(Uniform* _uniform, float _value)
    {
        CYAN_CHECK_UNIFORM(_uniform)

        u32 handle = itr->second; 
        // // write uniform handle
        // m_uniformBuffer->write(handle);
        // // write data 
        // m_uniformBuffer->write((void*)&_value, sizeof(f32));

        CYAN_ASSERT(_uniform->m_type == Uniform::Type::u_float, "mismatched uniform type, expecting float")
        *(f32*)(_uniform->m_valuePtr) = _value;
    }

    void setBuffer(RegularBuffer* _buffer, void* _data, u32 sizeToUpload)
    {
        _buffer->m_data = _data;
        _buffer->m_sizeToUpload = sizeToUpload;
    }


    namespace Toolkit
    {
        Mesh* createCubeMesh(const char* _name)
        {
            Mesh* cubeMesh = new Mesh;
            Mesh::SubMesh* subMesh = new Mesh::SubMesh;
            cubeMesh->m_name = _name;
            subMesh->m_numVerts = sizeof(cubeVertices) / sizeof(f32) / 3;
            subMesh->m_vertexArray = createVertexArray(createVertexBuffer(cubeVertices, sizeof(cubeVertices), 3 * sizeof(f32), subMesh->m_numVerts));
            subMesh->m_vertexArray->m_vertexBuffer->m_vertexAttribs.push_back({VertexAttrib::DataType::Float, 3, 3 * sizeof(f32), 0, cubeVertices});
            subMesh->m_vertexArray->init();

            cubeMesh->m_subMeshes.push_back(subMesh);
            s_meshes.push_back(cubeMesh);
            return cubeMesh;
        }

        glm::mat4 computeMeshNormalization(Mesh* _mesh)
        {
            auto findMin = [](float* vertexData, u32 numVerts, i32 vertexSize, i32 offset) {
                float min = vertexData[offset];
                for (u32 v = 1; v < numVerts; v++)
                    min = Min(min, vertexData[v * vertexSize + offset]); 
                return min;
            };

            auto findMax = [](float* vertexData, u32 numVerts, i32 vertexSize, i32 offset) {
                float max = vertexData[offset];
                for (u32 v = 1; v < numVerts; v++)
                    max = Max(max, vertexData[v * vertexSize + offset]); 
                return max;
            };

            float meshXMin = FLT_MAX; 
            float meshXMax = FLT_MIN;
            float meshYMin = FLT_MAX; 
            float meshYMax = FLT_MIN;
            float meshZMin = FLT_MAX; 
            float meshZMax = FLT_MIN;

            for (auto sm : _mesh->m_subMeshes)
            {
                u32 vertexSize = 0; 
                for (auto attrib : sm->m_vertexArray->m_vertexBuffer->m_vertexAttribs)
                    vertexSize += attrib.m_size;

                float xMin = findMin((float*)sm->m_vertexArray->m_vertexBuffer->m_data, sm->m_numVerts, vertexSize, 0);
                float yMin = findMin((float*)sm->m_vertexArray->m_vertexBuffer->m_data, sm->m_numVerts, vertexSize, 1);
                float zMin = findMin((float*)sm->m_vertexArray->m_vertexBuffer->m_data, sm->m_numVerts, vertexSize, 2);
                float xMax = findMax((float*)sm->m_vertexArray->m_vertexBuffer->m_data, sm->m_numVerts, vertexSize, 0);
                float yMax = findMax((float*)sm->m_vertexArray->m_vertexBuffer->m_data, sm->m_numVerts, vertexSize, 1);
                float zMax = findMax((float*)sm->m_vertexArray->m_vertexBuffer->m_data, sm->m_numVerts, vertexSize, 2);
                meshXMin = Min(meshXMin, xMin);
                meshYMin = Min(meshYMin, yMin);
                meshZMin = Min(meshZMin, zMin);
                meshXMax = Max(meshXMax, xMax);
                meshYMax = Max(meshYMax, yMax);
                meshZMax = Max(meshZMax, zMax);
            }
            f32 scale = Max(Cyan::fabs(meshXMin), Cyan::fabs(meshXMax));
            scale = Max(scale, Max(Cyan::fabs(meshYMin), Cyan::fabs(meshYMax)));
            scale = Max(scale, Max(Cyan::fabs(meshZMin), Cyan::fabs(meshZMax)));
            return glm::scale(glm::mat4(1.f), glm::vec3(1.f / scale));
        }

        glm::mat4 computeMeshesNormalization(std::vector<Mesh*> meshes) {
            for (auto& mesh : meshes) {
                continue;
            }
            return glm::mat4(1.f);
        }

        // Load equirectangular map into a cubemap
        Texture* loadEquirectangularMap(const char* _name, const char* _file, bool _hdr)
        {
            const u32 kViewportWidth = 1024;
            const u32 kViewportHeight = 1024;
            Camera camera = { };
            camera.position = glm::vec3(0.f);
            camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 

            // Create textures
            Texture* equirectMap, *envmap;
            TextureSpec spec = { };
            spec.m_type = Texture::Type::TEX_2D;
            spec.m_numMips = 1u;
            spec.m_min = Texture::Filter::LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::NONE;
            spec.m_t = Texture::Wrap::NONE;
            spec.m_r = Texture::Wrap::NONE;
            spec.m_data = nullptr;
            if (_hdr)
            {
                equirectMap = createTextureHDR(_name, _file, spec);
                spec.m_type = Texture::Type::TEX_CUBEMAP;
                spec.m_width = kViewportWidth;
                spec.m_height = kViewportHeight;
                spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
                spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
                spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
                envmap = createTextureHDR(_name, spec);
            }
            else
            {
                equirectMap = createTexture(_name, _file, spec);
                spec.m_type = Texture::Type::TEX_CUBEMAP;
                spec.m_width = kViewportWidth;
                spec.m_height = kViewportHeight;
                spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
                spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
                spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
                envmap = createTexture(_name, spec);
            }

            // Create render targets
            RenderTarget* rt = createRenderTarget(kViewportWidth, kViewportHeight);
            rt->attachColorBuffer(envmap);
            // Create shaders and uniforms
            Shader* shader = Cyan::createShader("GenCubemapShader", "../../shader/shader_gen_cubemap.vs", "../../shader/shader_gen_cubemap.fs");
            Uniform* u_projection = Cyan::createUniform("projection", Uniform::Type::u_mat4);
            Uniform* u_view = Cyan::createUniform("view", Uniform::Type::u_mat4);
            Uniform* u_envmapSampler = Cyan::createUniform("rawEnvmapSampler", Uniform::Type::u_sampler2D);
            glm::vec3 cameraTargets[] = {
                {1.f, 0.f, 0.f},   // Right
                {-1.f, 0.f, 0.f},  // Left
                {0.f, 1.f, 0.f},   // Up
                {0.f, -1.f, 0.f},  // Down
                {0.f, 0.f, 1.f},   // Front
                {0.f, 0.f, -1.f},  // Back
            }; 
            // TODO: (Min): Need to figure out why we need to flip the y-axis 
            // I thought they should just be vec3(0.f, 1.f, 0.f)
            // Referrence: https://www.khronos.org/opengl/wiki/Cubemap_Texture
            glm::vec3 worldUps[] = {
                {0.f, -1.f, 0.f},   // Right
                {0.f, -1.f, 0.f},   // Left
                {0.f, 0.f, 1.f},    // Up
                {0.f, 0.f, -1.f},   // Down
                {0.f, -1.f, 0.f},   // Forward
                {0.f, -1.f, 0.f},   // Back
            };
            Mesh* cubeMesh = Cyan::getMesh("cubemapMesh");
            if (!cubeMesh)
            {
                cubeMesh = Cyan::Toolkit::createCubeMesh("cubemapMesh");
            }
            // Cache viewport config
            glm::vec4 origViewport = s_gfxc->m_viewport;
            for (u32 f = 0; f < 6u; f++)
            {
                // Update view matrix
                camera.lookAt = cameraTargets[f];
                camera.worldUp = worldUps[f];
                CameraManager::updateCamera(camera);
                // Update uniform
                setUniform(u_projection, &camera.projection);
                setUniform(u_view, &camera.view);
                s_gfxc->setDepthControl(DepthControl::kDisable);
                s_gfxc->setRenderTarget(rt, f);
                // Since we are rendering to a framebuffer, we need to configure the viewport 
                // to prevent the texture being stretched to fit the framebuffer's dimension
                s_gfxc->setViewport(0, 0, kViewportWidth, kViewportHeight);
                s_gfxc->setShader(shader);
                s_gfxc->setUniform(u_projection);
                s_gfxc->setUniform(u_view);
                s_gfxc->setSampler(u_envmapSampler, 0);
                s_gfxc->setTexture(equirectMap, 0);
                s_gfxc->setPrimitiveType(PrimitiveType::TriangleList);
                s_gfxc->setVertexArray(cubeMesh->m_subMeshes[0]->m_vertexArray);

                s_gfxc->drawIndexAuto(cubeMesh->m_subMeshes[0]->m_numVerts, 0);
                s_gfxc->reset();
            }
            // Recover the viewport dimensions
            s_gfxc->setViewport(origViewport.x, origViewport.y, origViewport.z, origViewport.w);
            s_gfxc->setDepthControl(DepthControl::kEnable);
            return envmap;
        }

        Texture* prefilterEnvMapDiffuse(const char* _name, Texture* envMap)
        {
            const u32 kViewportWidth = 128u;
            const u32 kViewportHeight = 128u;
            Camera camera = { };
            camera.position = glm::vec3(0.f);
            camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
            // Create textures
            Texture* diffuseIrradianceMap;
            TextureSpec spec;
            spec.m_format = envMap->m_format;
            spec.m_type = Texture::Type::TEX_CUBEMAP;
            spec.m_numMips = 1u;
            spec.m_width = kViewportWidth;
            spec.m_height = kViewportHeight;
            spec.m_min = Texture::Filter::LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_data = nullptr;
            if (spec.m_format == Texture::ColorFormat::R16G16B16 || spec.m_format == Texture::ColorFormat::R16G16B16A16)
            {
                diffuseIrradianceMap = createTextureHDR(envMap->m_name.c_str(), spec);
            }
            else
            {
                diffuseIrradianceMap = createTexture(envMap->m_name.c_str(), spec);
            }
            // Create render targets
            RenderTarget* rt = createRenderTarget(kViewportWidth, kViewportHeight);
            rt->attachColorBuffer(diffuseIrradianceMap);
            // Create shaders and uniforms
            Shader* shader = Cyan::createShader("DiffuseIrradianceShader", "../../shader/shader_diff_irradiance.vs", "../../shader/shader_diff_irradiance.fs");
            Uniform* u_projection = Cyan::createUniform("projection", Uniform::Type::u_mat4);
            Uniform* u_view = Cyan::createUniform("view", Uniform::Type::u_mat4);
            Uniform* u_envmapSampler = Cyan::createUniform("envmapSampler", Uniform::Type::u_samplerCube);
            glm::vec3 cameraTargets[] = {
                {1.f, 0.f, 0.f},   // Right
                {-1.f, 0.f, 0.f},  // Left
                {0.f, 1.f, 0.f},   // Up
                {0.f, -1.f, 0.f},  // Down
                {0.f, 0.f, 1.f},   // Front
                {0.f, 0.f, -1.f},  // Back
            }; 
            // TODO: (Min): Need to figure out why we need to flip the y-axis 
            // I thought they should just be vec3(0.f, 1.f, 0.f)
            // Referrence: https://www.khronos.org/opengl/wiki/Cubemap_Texture
            glm::vec3 worldUps[] = {
                {0.f, -1.f, 0.f},   // Right
                {0.f, -1.f, 0.f},   // Left
                {0.f, 0.f, 1.f},    // Up
                {0.f, 0.f, -1.f},   // Down
                {0.f, -1.f, 0.f},   // Forward
                {0.f, -1.f, 0.f},   // Back
            };

            Mesh* cubeMesh = Cyan::getMesh("cubemapMesh");
            if (!cubeMesh)
            {
                cubeMesh = Cyan::Toolkit::createCubeMesh("cubemapMesh");
            }
            // Cache viewport config
            glm::vec4 origViewport = s_gfxc->m_viewport;
            for (u32 f = 0; f < 6u; f++)
            {
                // Update view matrix
                camera.lookAt = cameraTargets[f];
                camera.worldUp = worldUps[f];
                CameraManager::updateCamera(camera);
                // Update uniform
                setUniform(u_projection, &camera.projection);
                setUniform(u_view, &camera.view);
                s_gfxc->setDepthControl(DepthControl::kDisable);
                s_gfxc->setRenderTarget(rt, f);
                s_gfxc->setViewport(0, 0, kViewportWidth, kViewportHeight);
                s_gfxc->setShader(shader);
                s_gfxc->setUniform(u_projection);
                s_gfxc->setUniform(u_view);
                s_gfxc->setSampler(u_envmapSampler, 0);
                s_gfxc->setTexture(envMap, 0);
                s_gfxc->setPrimitiveType(PrimitiveType::TriangleList);
                s_gfxc->setVertexArray(cubeMesh->m_subMeshes[0]->m_vertexArray);

                s_gfxc->drawIndexAuto(cubeMesh->m_subMeshes[0]->m_numVerts, 0);
            }
            s_gfxc->reset();
            // Recover the viewport dimensions
            s_gfxc->setViewport(origViewport.x, origViewport.y, origViewport.z, origViewport.w);
            s_gfxc->setDepthControl(DepthControl::kEnable);
            return diffuseIrradianceMap;
        }

        Texture* prefilterEnvmapSpecular(Texture* envMap)
        {
            CYAN_ASSERT(envMap->m_type == Texture::Type::TEX_CUBEMAP, "Cannot prefilter a non-cubemap texture")
            Texture* prefilteredEnvMap;
            // HDR
            TextureSpec spec;
            spec.m_type = Texture::Type::TEX_CUBEMAP;
            spec.m_format = envMap->m_format;
            spec.m_numMips = 11u;
            spec.m_width = envMap->m_width;
            spec.m_height = envMap->m_height;
            spec.m_data = nullptr;
            // set the min filter to mipmap_linear as we need to sample from its mipmap
            spec.m_min = Texture::Filter::MIPMAP_LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_data = nullptr;
            if (spec.m_format == Texture::ColorFormat::R16G16B16 || spec.m_format == Texture::ColorFormat::R16G16B16A16)
            {
                prefilteredEnvMap = createTextureHDR(envMap->m_name.c_str(), spec);
            }
            else
            {
                prefilteredEnvMap = createTexture(envMap->m_name.c_str(), spec);
            }
            // glGenerateTextureMipmap(prefilteredEnvMap->m_id);
            Shader* shader = createShader("PrefilterSpecularShader", "../../shader/shader_prefilter_specular.vs", "../../shader/shader_prefilter_specular.fs");
            Uniform* u_roughness = Cyan::createUniform("roughness", Uniform::Type::u_float);
            Uniform* u_projection = Cyan::createUniform("projection", Uniform::Type::u_mat4);
            Uniform* u_view = Cyan::createUniform("view", Uniform::Type::u_mat4);
            Uniform* u_envmapSampler = Cyan::createUniform("envmapSampler", Uniform::Type::u_samplerCube);
            // camera
            Camera camera = { };
            camera.position = glm::vec3(0.f);
            camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
            glm::vec3 cameraTargets[] = {
                {1.f, 0.f, 0.f},   // Right
                {-1.f, 0.f, 0.f},  // Left
                {0.f, 1.f, 0.f},   // Up
                {0.f, -1.f, 0.f},  // Down
                {0.f, 0.f, 1.f},   // Front
                {0.f, 0.f, -1.f},  // Back
            }; 
            // TODO: (Min): Need to figure out why we need to flip the y-axis 
            // I thought they should just be vec3(0.f, 1.f, 0.f)
            // Referrence: https://www.khronos.org/opengl/wiki/Cubemap_Texture
            glm::vec3 worldUps[] = {
                {0.f, -1.f, 0.f},   // Right
                {0.f, -1.f, 0.f},   // Left
                {0.f, 0.f, 1.f},    // Up
                {0.f, 0.f, -1.f},   // Down
                {0.f, -1.f, 0.f},   // Forward
                {0.f, -1.f, 0.f},   // Back
            };
            Mesh* cubeMesh = Cyan::getMesh("cubemapMesh");
            if (!cubeMesh)
            {
                cubeMesh = Cyan::Toolkit::createCubeMesh("cubemapMesh");
            }
            // Cache viewport config
            glm::vec4 origViewport = s_gfxc->m_viewport;
            const u32 kNumMips = 10u;
            u32 mipWidth = prefilteredEnvMap->m_width; 
            u32 mipHeight = prefilteredEnvMap->m_height;
            RenderTarget* rts[kNumMips];;
            for (u32 mip = 0; mip < kNumMips; ++mip)
            {
                rts[mip] = createRenderTarget(mipWidth, mipHeight);
                rts[mip]->attachColorBuffer(prefilteredEnvMap, mip);
                s_gfxc->setViewport(0u, 0u, mipWidth, mipHeight);
                for (u32 f = 0; f < 6u; f++)
                {
                    camera.lookAt = cameraTargets[f];
                    camera.worldUp = worldUps[f];
                    CameraManager::updateCamera(camera);
                    // Update uniforms
                    setUniform(u_projection, &camera.projection);
                    setUniform(u_view, &camera.view);
                    setUniform(u_roughness, mip * (1.f / kNumMips));
                    s_gfxc->setDepthControl(DepthControl::kDisable);
                    s_gfxc->setRenderTarget(rts[mip], f);
                    s_gfxc->setShader(shader);
                    // uniforms
                    s_gfxc->setUniform(u_projection);
                    s_gfxc->setUniform(u_view);
                    s_gfxc->setUniform(u_roughness);
                    // textures
                    s_gfxc->setSampler(u_envmapSampler, 0);
                    s_gfxc->setTexture(envMap, 0);
                    s_gfxc->setPrimitiveType(PrimitiveType::TriangleList);
                    s_gfxc->setVertexArray(cubeMesh->m_subMeshes[0]->m_vertexArray);

                    s_gfxc->drawIndexAuto(cubeMesh->m_subMeshes[0]->m_numVerts, 0);
                }
                mipWidth /= 2u;
                mipHeight /= 2u;
                s_gfxc->reset();
            }
            // Recover the viewport dimensions
            s_gfxc->setViewport(origViewport.x, origViewport.y, origViewport.z, origViewport.w);
            s_gfxc->setDepthControl(DepthControl::kEnable);
            return prefilteredEnvMap;
        }

        // integrate brdf for specular IBL
        Texture* generateBrdfLUT()
        {
            const u32 kTexWidth = 512u;
            const u32 kTexHeight = 512u;
            TextureSpec spec = { };
            spec.m_type = Texture::Type::TEX_2D;
            spec.m_format = Texture::ColorFormat::R16G16B16A16;
            spec.m_numMips = 1u;
            spec.m_width = kTexWidth;
            spec.m_height = kTexHeight;
            spec.m_min = Texture::Filter::LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_data = nullptr;
            Texture* outputTexture = createTextureHDR("integrateBrdf", spec); 
            Shader* shader = createShader("IntegrateBRDFShader", "../../shader/shader_integrate_brdf.vs", "../../shader/shader_integrate_brdf.fs");
            RenderTarget* rt = createRenderTarget(kTexWidth, kTexWidth);
            rt->attachColorBuffer(outputTexture);
            f32 verts[] = {
                -1.f,  1.f, 0.f, 0.f,  1.f,
                -1.f, -1.f, 0.f, 0.f,  0.f,
                 1.f, -1.f, 0.f, 1.f,  0.f,
                -1.f,  1.f, 0.f, 0.f,  1.f,
                 1.f, -1.f, 0.f, 1.f,  0.f,
                 1.f,  1.f, 0.f, 1.f,  1.f
            };
            GLuint vbo, vao;
            glCreateBuffers(1, &vbo);
            glCreateVertexArrays(1, &vao);
            glBindVertexArray(vao);
            glNamedBufferData(vbo, sizeof(verts), verts, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glEnableVertexArrayAttrib(vao, 0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), 0);
            glEnableVertexArrayAttrib(vao, 1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (const void*)(3 * sizeof(f32)));
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            auto gfxc = getCurrentGfxCtx();
            glm::vec4 origViewport = gfxc->m_viewport;
            gfxc->setViewport(0.f, 0.f, kTexWidth, kTexHeight);
            gfxc->setShader(shader);
            gfxc->setRenderTarget(rt, 0);
            glBindVertexArray(vao);
            gfxc->setDepthControl(Cyan::DepthControl::kDisable);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            gfxc->setDepthControl(Cyan::DepthControl::kEnable);
            gfxc->setViewport(origViewport.x, origViewport.y, origViewport.z, origViewport.w);
            gfxc->reset();

            return outputTexture;
        }

        // TODO: This call uses a lot of memory, investigate why
        LightProbe createLightProbe(const char* name, const char* file, bool hdr)
        {
            LightProbe probe = { };
            probe.m_baseCubeMap = Toolkit::loadEquirectangularMap(name, file, hdr);
            probe.m_diffuse = Toolkit::prefilterEnvMapDiffuse(name, probe.m_baseCubeMap);
            probe.m_specular = Toolkit::prefilterEnvmapSpecular(probe.m_baseCubeMap);
            probe.m_brdfIntegral = Toolkit::generateBrdfLUT();
            s_probes.push_back(probe);
            return probe;
        }

        // create a flat color albedo map via fragment shader
        Texture* createFlatColorTexture(const char* name, u32 width, u32 height, glm::vec4 color)
        {
            TextureSpec spec = { };
            spec.m_type = Texture::Type::TEX_2D;
            spec.m_format = Texture::ColorFormat::R8G8B8A8;
            spec.m_width = width;
            spec.m_numMips = 1u;
            spec.m_height = height;
            spec.m_min = Texture::Filter::LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::NONE;
            spec.m_t = Texture::Wrap::NONE;
            spec.m_r = Texture::Wrap::NONE;
            spec.m_data = nullptr;
            f32 verts[] = {
                -1.f,  1.f, 0.f, 0.f,  1.f,
                -1.f, -1.f, 0.f, 0.f,  0.f,
                 1.f, -1.f, 0.f, 1.f,  0.f,
                -1.f,  1.f, 0.f, 0.f,  1.f,
                 1.f, -1.f, 0.f, 1.f,  0.f,
                 1.f,  1.f, 0.f, 1.f,  1.f
            };
            GLuint vbo, vao;
            glCreateBuffers(1, &vbo);
            glCreateVertexArrays(1, &vao);
            glBindVertexArray(vao);
            glNamedBufferData(vbo, sizeof(verts), verts, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glEnableVertexArrayAttrib(vao, 0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), 0);
            glEnableVertexArrayAttrib(vao, 1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (const void*)(3 * sizeof(f32)));
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            Shader* shader = createShader("FlatColorShader", "../../shader/shader_flat_color.vs", "../../shader/shader_flat_color.fs");
            Texture* texture = createTexture(name, spec);
            RenderTarget* rt = createRenderTarget(width, height);
            Uniform* u_color = createUniform("color", Uniform::Type::u_vec4);
            setUniform(u_color, &color.r);
            rt->attachColorBuffer(texture);

            auto gfxc = getCurrentGfxCtx();
            glm::vec4 origViewport = gfxc->m_viewport;
            gfxc->setViewport(0.f, 0.f, texture->m_width, texture->m_height);
            gfxc->setShader(shader);
            gfxc->setRenderTarget(rt, 0);
            gfxc->setUniform(u_color);
            glBindVertexArray(vao);
            gfxc->setDepthControl(Cyan::DepthControl::kDisable);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            gfxc->setDepthControl(Cyan::DepthControl::kEnable);
            gfxc->setViewport(origViewport.x, origViewport.y, origViewport.z, origViewport.w);
            gfxc->reset();

            s_textures.push_back(texture);
            return texture;
        }
    } // Toolkit

    namespace AssetGen
    {
#if 0
        struct Vertex
        {
            glm::vec3 m_position;
            glm::vec3 m_uv;
            glm::vec3 m_normal;
            glm::vec3 m_tangent;
        };

        // 1 x 1 x 1 cube mesh with uv
        glm::vec3 uvCubeVertices[] = {
            //  z 
            { glm::vec3(-1.f, -1.f, 1.f), glm::vec2(0.f, 0.f), glm::vec3(0.f, 0.f, 1.f), }
            { glm::vec3( 1.f, -1.f, 1.f), glm::vec2(0.f, 1.f), glm::vec3(0.f, 0.f, -1.f), },
            glm::vec3( 1.f,  1.f, 1.f),
            glm::vec3( 1.f,  1.f, 1.f),
            glm::vec3(-1.f,  1.f, 1.f),
            glm::vec3(-1.f, -1.f, 1.f),
            // -z
            glm::vec3(-1.f, -1.f, -1.f),
            glm::vec3( 1.f, -1.f, -1.f),
            glm::vec3( 1.f,  1.f, -1.f),
            glm::vec3( 1.f,  1.f, -1.f),
            glm::vec3(-1.f,  1.f, -1.f),
            glm::vec3(-1.f, -1.f, -1.f),
            //  x
            // -x
            //  y
            // -y
        };
#endif

        Mesh* createTerrain(float extendX, float extendY)
        {
            // TODO: determine texture tiling automatically 

            // texture tiling every 10 meters  
            const f32 textureTileThreshold = 4.f; 

            struct Vertex
            {
                glm::vec3 m_position;
                glm::vec3 m_normal;
                glm::vec3 m_tangent;
                glm::vec2 m_uv;
            };

            std::vector<Vertex> vertices;
            // each tile is a 1m x 1m squre
            float tileXInMeters = 1.f;
            float tileYInMeters = 1.f;
            glm::vec2 offsets[6] = {
                {0.f, 0.f},
                {1.f, 0.f},
                {1.f, 1.f},
                {1.f, 1.f},
                {0.f, 1.f},
                {0.f, 0.f}
            };
            // coordinates are initially in terrain space, where x goes right, y goes forward, z goes up,
            glm::vec2 deltaUv(1.f / extendX, 1.f / extendY);
            for (float x = 0.f; x < extendX; ++x)
            {
                for (float y = 0; y < extendY; ++y)
                {
                    Vertex tileVerts[6];
                    for (u32 v = 0u; v < 6u; ++v)
                    {
                        // position
                        tileVerts[v].m_position.x = (x + offsets[v].x) * tileXInMeters;
                        tileVerts[v].m_position.y = 0.f;
                        tileVerts[v].m_position.z = extendY - (y + offsets[v].y) * tileYInMeters;
                        // normal
                        tileVerts[v].m_normal = glm::vec3(0.f, 1.f, 0.f);
                        // tangent
                        tileVerts[v].m_tangent = glm::vec3(0.f, 0.f, 1.f);
                        vertices.push_back(tileVerts[v]);
                        // uv
                        tileVerts[v].m_uv = glm::vec2(x, y) + offsets[v];
                        tileVerts[v].m_uv /= textureTileThreshold;
                    }
                }
            }
            // center the mesh to (0, 0, 0) in object space
            for (auto& vertex : vertices)
            {
                glm::vec3 translate(-extendX * .5f, 0.f, -extendY * .5f);
                vertex.m_position += translate;
            }
            // create mesh
            Mesh* mesh = new Mesh;
            mesh->m_name = std::string("terrain_mesh");
            Mesh::SubMesh* subMesh = new Cyan::Mesh::SubMesh;
            subMesh->m_numVerts = (u32)vertices.size();
            u32 stride = sizeof(Vertex);
            u32 offset = 0;
            VertexBuffer* vb = Cyan::createVertexBuffer(vertices.data(), subMesh->m_numVerts * sizeof(Vertex), stride, subMesh->m_numVerts);
            vb->m_vertexAttribs.push_back({ VertexAttrib::DataType::Float, 3, stride, offset, vb->m_data});
            offset += sizeof(glm::vec3);
            vb->m_vertexAttribs.push_back({ VertexAttrib::DataType::Float, 3, stride, offset, (void*)((u8*)vb->m_data + offset)});
            offset += sizeof(glm::vec3);
            vb->m_vertexAttribs.push_back({ VertexAttrib::DataType::Float, 3, stride, offset, (void*)((u8*)vb->m_data + offset)});
            offset += sizeof(glm::vec3);
            vb->m_vertexAttribs.push_back({ VertexAttrib::DataType::Float, 2, stride, offset, (void*)((u8*)vb->m_data + offset)});
            offset += sizeof(glm::vec2);
            subMesh->m_vertexArray = Cyan::createVertexArray(vb);
            subMesh->m_vertexArray->init();
            mesh->m_subMeshes.push_back(subMesh);
            mesh->m_normalization = glm::mat4(1.f);
            addMesh(mesh);
            return mesh;
        }
    }; // namespace AssetGen
} // Cyan