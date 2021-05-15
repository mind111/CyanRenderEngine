#include <fstream>
#include <unordered_map>
#include <stack>

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "json.hpp"

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
    t.translation = j.at("translation").get<glm::vec3>();
    glm::vec4 rotation = j.at("rotation").get<glm::vec4>();
    t.qRot = glm::quat(cos(RADIANS(rotation.x * 0.5f)), sin(RADIANS(rotation.x * 0.5f)) * glm::vec3(rotation.y, rotation.z, rotation.w));
    t.scale = j.at("scale").get<glm::vec3>();
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

    // TODO: Where do these live in memory...?
    static std::vector<Texture*> s_textures;
    static std::vector<Mesh*> s_meshes;
    static std::vector<Uniform*> s_uniforms;
    static GfxContext* s_gfxc = nullptr;
    static void* s_memory = nullptr;
    static LinearAllocator* s_allocator = nullptr;
    static std::unordered_map<std::string, u32> s_uniformHashTable;
    static HandleAllocator s_handleAllocator = { };
    static Uniform* m_uniforms[kMaxNumUniforms];

    static u32 m_numUniforms = 0;

    void init()
    {
        CYAN_ASSERT(!s_gfxc, "Graphics context was already initialized");
        // Create global graphics context
        s_gfxc = new GfxContext;
        s_gfxc->init();
        s_memory = (void*)(new char[1024 * 1024 * 1024]); // 1GB memory pool
        s_allocator = LinearAllocator::create(s_memory, 1024 * 1024 * 1024);
    }

    u32 allocHandle()
    {
        return s_handleAllocator.alloc();
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

    RegularBuffer* createRegularBuffer(const char* _blockName, Shader* _shader, u32 _binding, u32 _sizeInBytes)
    {
        RegularBuffer* buffer = new RegularBuffer;
        buffer->m_name = _blockName;
        buffer->m_binding = _binding;
        buffer->m_data = nullptr;

        GLuint blockIndex = glGetProgramResourceIndex(_shader->m_programId, GL_SHADER_STORAGE_BLOCK, buffer->m_name);
        glShaderStorageBlockBinding(_shader->m_programId, blockIndex, _binding);
        glCreateBuffers(1, &buffer->m_ssbo);
        _shader->bind();
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer->m_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, _sizeInBytes, 0, GL_DYNAMIC_COPY);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, _binding, buffer->m_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        _shader->unbind();
        _shader->m_buffers.push_back(buffer);
        return buffer;
    }

    VertexBuffer* createVertexBuffer(void* _data, u32 _sizeInBytes, u32 _strideInBytes, u32 _numVerts)
    {
        CYAN_ASSERT(_sizeInBytes == _strideInBytes * _numVerts, "wrong size in bytes")
        VertexBuffer* vb = new VertexBuffer;
        u32 offset = 0;
        vb->m_data = _data;
        vb->m_strideInBytes = _strideInBytes;
        vb->m_numVerts = _numVerts;
        glCreateBuffers(1, &vb->m_vbo);
        glNamedBufferData(vb->m_vbo, _sizeInBytes, vb->m_data, GL_STATIC_DRAW);
        return vb;
    }

    VertexArray* createVertexArray()
    {
        VertexArray* vertexArray = new VertexArray;
        glCreateVertexArrays(1, &vertexArray->m_vao);
        return vertexArray;
    }

    Mesh* createMesh(const char* _name, const char* _file)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            _file,
            aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals
        );

        Mesh* mesh = new Mesh;
        mesh->m_name = _name;

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
            strideInBytes += (attribFlag & VertexAttribFlag::kTexcoord) > 0 ? 2 * sizeof(f32) : 0;
            strideInBytes += (attribFlag & VertexAttribFlag::kNormal)   > 0 ? 3 * sizeof(f32) : 0;
            strideInBytes += (attribFlag & VertexAttribFlag::kTangents) > 0 ? 6 * sizeof(f32) : 0;

            u32 numVerts = (u32)scene->mMeshes[sm]->mNumVertices;
            float* data = new float[strideInBytes * numVerts];
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
                if (attribFlag & VertexAttribFlag::kTexcoord)
                {
                    vertexAddress[offset++] = scene->mMeshes[sm]->mTextureCoords[0][v].x;
                    vertexAddress[offset++] = scene->mMeshes[sm]->mTextureCoords[0][v].y;
                }
                if (attribFlag & VertexAttribFlag::kTangents)
                {
                    vertexAddress[offset++] = scene->mMeshes[sm]->mTangents[v].x;
                    vertexAddress[offset++] = scene->mMeshes[sm]->mTangents[v].y;
                    vertexAddress[offset++] = scene->mMeshes[sm]->mTangents[v].z;
                    vertexAddress[offset++] = scene->mMeshes[sm]->mBitangents[v].x;
                    vertexAddress[offset++] = scene->mMeshes[sm]->mBitangents[v].y;
                    vertexAddress[offset++] = scene->mMeshes[sm]->mBitangents[v].z;
                }
            }

            subMesh->m_vertexArray = createVertexArray();
            subMesh->m_vertexArray->m_vertexBuffer = createVertexBuffer(data, strideInBytes * numVerts, strideInBytes, numVerts);
            mesh->m_subMeshes.push_back(subMesh);

            u32 offset = 0;
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
            if (attribFlag & VertexAttribFlag::kTexcoord)
            {
                subMesh->m_vertexArray->m_vertexBuffer->m_vertexAttribs.push_back({
                    VertexAttrib::DataType::Float, 2, strideInBytes, offset, (float*)subMesh->m_vertexArray->m_vertexBuffer->m_data + offset});
                offset += sizeof(f32) * 2;
            }
            if (attribFlag & VertexAttribFlag::kTangents)
            {
                subMesh->m_vertexArray->m_vertexBuffer->m_vertexAttribs.push_back({
                    VertexAttrib::DataType::Float, 3, strideInBytes, offset, (float*)subMesh->m_vertexArray->m_vertexBuffer->m_data + offset});
                offset += sizeof(f32) * 3;
                subMesh->m_vertexArray->m_vertexBuffer->m_vertexAttribs.push_back({
                    VertexAttrib::DataType::Float, 3, strideInBytes, offset, (float*)subMesh->m_vertexArray->m_vertexBuffer->m_data + offset});
            }
            subMesh->m_vertexArray->init();
        }
        // Store the xform for normalizing object space mesh coordinates
        mesh->m_normalization = Toolkit::computeMeshNormalization(mesh);
        s_meshes.push_back(mesh);
        return mesh;
    }

    Mesh* getMesh(const char* _name)
    {
        for (auto mesh : s_meshes)
        {
            if (strcmp(mesh->m_name.c_str(), _name) == 0)
            {
                return mesh; 
            }
        }
        return 0;
    }

    Scene* createScene(const char* _file)
    {
        Scene* scene = new Scene;

        nlohmann::json sceneJson;
        std::ifstream sceneFile(_file);
        sceneFile >> sceneJson;
        auto cameras = sceneJson["cameras"];
        auto meshInfoList = sceneJson["meshes"];
        auto textureInfoList = sceneJson["textures"];
        auto entities = sceneJson["entities"];

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
            
            if (dynamicRange == "ldr")
                createTexture(name.c_str(), filename.c_str());
            else if (dynamicRange == "hdr")
                createTextureHDR(name.c_str(), filename.c_str());
        }

        for (auto meshInfo : meshInfoList) 
        {
            std::string path, name;
            meshInfo.at("path").get_to(path);
            meshInfo.at("name").get_to(name);
            createMesh(name.c_str(), path.c_str());
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
            entityInfo.at("mesh").get_to(meshName);
            auto xformInfo = entityInfo.at("xform");
            Transform xform = entityInfo.at("xform").get<Transform>();
            Entity* newEntity = SceneManager::createEntity(*scene);
            newEntity->m_mesh = getMesh(meshName.c_str()); 
            newEntity->m_xform = entityInfo.at("xform").get<Transform>();
            newEntity->m_position = glm::vec3(0.f);
        }

        return scene;
    }

    Shader* createShader(const char* vertSrc, const char* fragSrc)
    {
        // TODO: Memory management
        Shader* shader = new Shader();
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
    
    Texture* createTexture(const char* _name, u32 _width, u32 _height, Texture::ColorFormat _format, Texture::Type _type, Texture::Filter _filter)
    {
        Texture* texture = new Texture();

        texture->m_name = _name;
        texture->m_width = _width;
        texture->m_height = _height;
        texture->m_type = _type;
        texture->m_format = _format;
        texture->m_filter = _filter;
        texture->m_data = nullptr;

        GLenum type_gl, internalFormat_gl, dataFormat_gl, filter_gl;

        switch(texture->m_type)
        {
            case Texture::Type::TEX_2D:
                type_gl = GL_TEXTURE_2D;
                break;
            case Texture::Type::TEX_CUBEMAP:
                type_gl = GL_TEXTURE_CUBE_MAP;
                break;
            default:
                break;
        }

        switch(texture->m_filter)
        {
            case Texture::Filter::LINEAR:
                filter_gl = GL_LINEAR;
                break;
            default:
                break;
        }

        switch (texture->m_format)
        {
            case Texture::ColorFormat::R8G8B8: 
                dataFormat_gl = GL_RGB8;
                internalFormat_gl = GL_RGB; 
                break;
            case Texture::ColorFormat::R8G8B8A8: 
                dataFormat_gl = GL_RGBA8;
                internalFormat_gl = GL_RGBA; 
                break;
            default:
                break;
        }

        glCreateTextures(type_gl, 1, &texture->m_id);

        glTextureStorage2D(texture->m_id, 1, dataFormat_gl, texture->m_width, texture->m_height);
        glTextureSubImage2D(texture->m_id, 0, 0, 0, texture->m_width, texture->m_height, internalFormat_gl, GL_UNSIGNED_BYTE, texture->m_data);

        glBindTexture(type_gl, texture->m_id);
        glTexParameteri(type_gl, GL_TEXTURE_MAG_FILTER, filter_gl);
        glTexParameteri(type_gl, GL_TEXTURE_MIN_FILTER, filter_gl);
        glBindTexture(type_gl, 0);
        
        return texture;
    }

    Texture* createTextureHDR(const char* _name, u32 _width, u32 _height, Texture::ColorFormat _format, Texture::Type _type, Texture::Filter _filter)
    {
        Texture* texture = new Texture();

        texture->m_name = _name;
        texture->m_width = _width;
        texture->m_height = _height;
        texture->m_type = _type;
        texture->m_format = _format;
        texture->m_filter = _filter;
        texture->m_data = nullptr;

        GLenum type_gl, internalFormat_gl, dataFormat_gl, filter_gl;

        switch(texture->m_type)
        {
            case Texture::Type::TEX_2D:
                type_gl = GL_TEXTURE_2D;
                break;
            case Texture::Type::TEX_CUBEMAP:
                type_gl = GL_TEXTURE_CUBE_MAP;
                break;
            default:
                break;
        }

        switch(texture->m_filter)
        {
            case Texture::Filter::LINEAR:
                filter_gl = GL_LINEAR;
                break;
            default:
                break;
        }

        switch (texture->m_format)
        {
            case Texture::ColorFormat::R16G16B16: 
                dataFormat_gl = GL_RGB16;
                internalFormat_gl = GL_RGB; 
                break;
            case Texture::ColorFormat::R16G16B16A16: 
                dataFormat_gl = GL_RGBA16;
                internalFormat_gl = GL_RGBA; 
                break;
            default:
                break;
        }

        glCreateTextures(type_gl, 1, &texture->m_id);

        glBindTexture(type_gl, texture->m_id);

        if (texture->m_type == Texture::Type::TEX_2D)
        {
            glTextureStorage2D(texture->m_id, 1, dataFormat_gl, texture->m_width, texture->m_height);
            glTextureSubImage2D(texture->m_id, 0, 0, 0, texture->m_width, texture->m_height, internalFormat_gl, GL_FLOAT, texture->m_data);

            glTexParameteri(type_gl, GL_TEXTURE_MAG_FILTER, filter_gl);
            glTexParameteri(type_gl, GL_TEXTURE_MIN_FILTER, filter_gl);
        }
        else if (texture->m_type == Texture::Type::TEX_CUBEMAP)
        {
            for (int f = 0; f < 6; f++)
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, dataFormat_gl, texture->m_width, texture->m_height, 0, internalFormat_gl, GL_FLOAT, nullptr);

            glTexParameteri(type_gl, GL_TEXTURE_MIN_FILTER, filter_gl);
            glTexParameteri(type_gl, GL_TEXTURE_MAG_FILTER, filter_gl);
            glTexParameteri(type_gl, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(type_gl, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(type_gl, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        }

        glBindTexture(type_gl, 0);
        
        return texture;
    }

    Texture* createTexture(const char* _name, const char* _file, Texture::Type _type, Texture::Filter _filter)
    {
        int w, h, numChannels;
        Texture* texture = new Texture();
        stbi_set_flip_vertically_on_load(1);
        unsigned char* pixels = stbi_load(_file, &w, &h, &numChannels, 0);

        texture->m_name = _name;
        texture->m_width = w;
        texture->m_height = h;
        texture->m_type = _type;
        texture->m_format = numChannels == 3 ? Texture::ColorFormat::R8G8B8 : Texture::ColorFormat::R8G8B8A8;
        texture->m_filter = _filter;
        texture->m_data = pixels;

        GLenum type_gl, internalFormat_gl, dataFormat_gl, filter_gl;

        switch(texture->m_type)
        {
            case Texture::Type::TEX_2D:
                type_gl = GL_TEXTURE_2D;
                break;
            case Texture::Type::TEX_CUBEMAP:
                type_gl = GL_TEXTURE_CUBE_MAP;
                break;
            default:
                break;
        }

        switch(texture->m_filter)
        {
            case Texture::Filter::LINEAR:
                filter_gl = GL_LINEAR;
                break;
            default:
                break;
        }

        switch (texture->m_format)
        {
            case Texture::ColorFormat::R8G8B8: 
                dataFormat_gl = GL_RGB8;
                internalFormat_gl = GL_RGB; 
                break;
            case Texture::ColorFormat::R8G8B8A8: 
                dataFormat_gl = GL_RGBA8;
                internalFormat_gl = GL_RGBA; 
                break;
            default:
                break;
        }

        glCreateTextures(type_gl, 1, &texture->m_id);

        glTextureStorage2D(texture->m_id, 1, dataFormat_gl, texture->m_width, texture->m_height);
        glTextureSubImage2D(texture->m_id, 0, 0, 0, texture->m_width, texture->m_height, internalFormat_gl, GL_UNSIGNED_BYTE, texture->m_data);

        glBindTexture(type_gl, texture->m_id);
        glTexParameteri(type_gl, GL_TEXTURE_MAG_FILTER, filter_gl);
        glTexParameteri(type_gl, GL_TEXTURE_MIN_FILTER, filter_gl);
        glBindTexture(type_gl, 0);

        s_textures.push_back(texture);

        return texture;
    }

    Texture* createTextureHDR(const char* _name, const char* _file, Texture::Type _type, Texture::Filter _filter)
    {
        int w, h, numChannels;
        Texture* texture = new Texture();
        float* pixels = stbi_loadf(_file, &w, &h, &numChannels, 0);

        texture->m_name = _name;
        texture->m_width = w;
        texture->m_height = h;
        texture->m_type = _type;
        texture->m_format = numChannels == 3 ? Texture::ColorFormat::R16G16B16 : Texture::ColorFormat::R16G16B16A16;
        texture->m_filter = _filter;
        texture->m_data = pixels;

        GLenum type_gl, internalFormat_gl, dataFormat_gl, filter_gl;

        switch(texture->m_type)
        {
            case Texture::Type::TEX_2D:
                type_gl = GL_TEXTURE_2D;
                break;
            case Texture::Type::TEX_CUBEMAP:
                type_gl = GL_TEXTURE_CUBE_MAP;
                break;
            default:
                break;
        }

        switch(texture->m_filter)
        {
            case Texture::Filter::LINEAR:
                filter_gl = GL_LINEAR;
                break;
            default:
                break;
        }

        switch (texture->m_format)
        {
            case Texture::ColorFormat::R16G16B16: 
                dataFormat_gl = GL_RGB16;
                internalFormat_gl = GL_RGB; 
                break;
            case Texture::ColorFormat::R16G16B16A16: 
                dataFormat_gl = GL_RGBA16;
                internalFormat_gl = GL_RGBA; 
                break;
            default:
                break;
        }

        glCreateTextures(type_gl, 1, &texture->m_id);

        glTextureStorage2D(texture->m_id, 1, dataFormat_gl, texture->m_width, texture->m_height);
        glTextureSubImage2D(texture->m_id, 0, 0, 0, texture->m_width, texture->m_height, internalFormat_gl, GL_FLOAT, texture->m_data);

        glBindTexture(type_gl, texture->m_id);
        glTexParameteri(type_gl, GL_TEXTURE_MAG_FILTER, filter_gl);
        glTexParameteri(type_gl, GL_TEXTURE_MIN_FILTER, filter_gl);
        glBindTexture(type_gl, 0);

        s_textures.push_back(texture);

        return texture;
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
        auto itr = s_uniformHashTable.find(key);
        if (itr != s_uniformHashTable.end())
        {
            u32 handle = itr->second; 
            return m_uniforms[handle]; 
        }
        u32 handle = allocHandle();
        m_uniforms[handle] = (Uniform*)s_allocator->alloc(sizeof(Uniform)); 
        Uniform* uniform = m_uniforms[handle];
        uniform->m_type = _type;
        strcpy(uniform->m_name, _name);
        u32 size = uniform->getSize();
        uniform->m_valuePtr = s_allocator->alloc(size);
        memset((u8*)uniform->m_valuePtr, 0x0, size);
        s_uniformHashTable.insert(std::pair<std::string,u32>(key, handle));
        m_numUniforms += 1;
        CYAN_ASSERT(m_numUniforms <= kMaxNumUniforms, "Too many uniforms created");
        return uniform;
    }

    Uniform* createShaderUniform(Shader* _shader, const char* _name, Uniform::Type _type)
    {
        Uniform* uniform = createUniform(_name, _type);
        _shader->bindUniform(uniform);
        return uniform;
    }

    Uniform* createMaterialUniform(Material* _material, const char* _name, Uniform::Type _type)
    {
        Uniform* uniform = createUniform(_name, _type);
        _material->bindUniform(uniform);
        return uniform;
    }

    Material* createMaterial(Shader* _shader)
    {
        Material* material = new Material;
        material->m_shader = _shader;
        return material;
    }

    // Notes(Min): for variables that are allocated from stack, we need to call ctx->setUniform
    //             before the variable goes out of scope
    void setUniform(Uniform* _uniform, void* _valuePtr)
    {
        u32 size = _uniform->getSize();
        memcpy(_uniform->m_valuePtr, _valuePtr, size);
    }

    void setUniform(Uniform* _uniform, u32 _value)
    {
        // int type in glsl is 32bits
        CYAN_ASSERT(_uniform->m_type == Uniform::Type::u_int, "mismatced uniform type, expecting unsigned int")
        u32* ptr = static_cast<u32*>(_uniform->m_valuePtr); 
        *(u32*)(_uniform->m_valuePtr) = _value;
    }

    void setUniform(Uniform* _uniform, float _value)
    {
        CYAN_ASSERT(_uniform->m_type == Uniform::Type::u_float, "mismatched uniform type, expecting float")
        *(f32*)(_uniform->m_valuePtr) = _value;
    }

    void setBuffer(RegularBuffer* _buffer, void* _data, u32 _sizeInBytes)
    {
        _buffer->m_data = _data;
        _buffer->m_sizeInBytes = _sizeInBytes;
    }

    namespace Toolkit
    {
        Mesh* createCubeMesh(const char* _name)
        {
            Mesh* cubeMesh = new Mesh;
            Mesh::SubMesh* subMesh = new Mesh::SubMesh;
            cubeMesh->m_name = _name;
            subMesh->m_numVerts = sizeof(cubeVertices) / sizeof(f32) / 3;
            subMesh->m_vertexArray = createVertexArray();
            subMesh->m_vertexArray->m_vertexBuffer = createVertexBuffer(cubeVertices, sizeof(cubeVertices), 3 * sizeof(f32), subMesh->m_numVerts);
            subMesh->m_vertexArray->m_vertexBuffer->m_vertexAttribs.push_back({VertexAttrib::DataType::Float, 3, 3 * sizeof(f32), 0, cubeVertices});
            subMesh->m_vertexArray->init();

            cubeMesh->m_subMeshes.push_back(subMesh);
            s_meshes.push_back(cubeMesh);
            return cubeMesh;
        }

        glm::mat4 computeMeshNormalization(Mesh* _mesh)
        {
            auto findMin = [](float* vertexData, u32 numVerts, int vertexSize, int offset) {
                float min = vertexData[offset];
                for (u32 v = 1; v < numVerts; v++)
                    min = Min(min, vertexData[v * vertexSize + offset]); 
                return min;
            };

            auto findMax = [](float* vertexData, u32 numVerts, int vertexSize, int offset) {
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
            if (_hdr)
            {
                equirectMap = createTextureHDR(_name, _file);
                envmap = createTextureHDR(_name, kViewportWidth, kViewportHeight, Texture::ColorFormat::R16G16B16, Texture::Type::TEX_CUBEMAP);
            }
            else
            {
                equirectMap = createTexture(_name, _file);
                envmap = createTexture(_name, kViewportWidth, kViewportHeight, Texture::ColorFormat::R8G8B8, Texture::Type::TEX_CUBEMAP);
            }
            // Create render targets
            RenderTarget* rt = createRenderTarget(kViewportWidth, kViewportHeight);
            rt->attachColorBuffer(envmap);
            // Create shaders and uniforms
            Shader* shader = Cyan::createShader("../../shader/shader_gen_cubemap.vs", "../../shader/shader_gen_cubemap.fs");
            Uniform* u_projection = Cyan::createUniform("projection", Uniform::Type::u_mat4);
            Uniform* u_view = Cyan::createUniform("view", Uniform::Type::u_mat4);
            Uniform* u_envmapSampler = Cyan::createUniform("rawEnvmapSampler", Uniform::Type::u_sampler2D);
            shader->bindUniform(u_projection);
            shader->bindUniform(u_view);
            shader->bindUniform(u_envmapSampler);
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
                s_gfxc->setRenderTarget(rt, (1 << f));
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

                s_gfxc->drawIndex(cubeMesh->m_subMeshes[0]->m_numVerts, 0);
                s_gfxc->reset();
            }
            // Recover the viewport dimensions
            s_gfxc->setViewport(origViewport.x, origViewport.y, origViewport.z, origViewport.w);
            s_gfxc->setDepthControl(DepthControl::kEnable);
            return envmap;
        }

        Texture* generateDiffsueIrradianceMap(const char* _name, Texture* _envMap, bool _hdr)
        {
            const u32 kViewportWidth = 1024;
            const u32 kViewportHeight = 1024;

            Camera camera = { };
            camera.position = glm::vec3(0.f);
            camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 

            // Create textures
            Texture* diffuseIrradianceMap;
            if (_hdr)
            {
                diffuseIrradianceMap = createTextureHDR(_name, kViewportWidth, kViewportHeight, Texture::ColorFormat::R16G16B16, Texture::Type::TEX_CUBEMAP);
            }
            else
            {
                diffuseIrradianceMap = createTexture(_name, kViewportWidth, kViewportHeight, Texture::ColorFormat::R8G8B8, Texture::Type::TEX_CUBEMAP);
            }

            // Create render targets
            RenderTarget* rt = createRenderTarget(kViewportWidth, kViewportHeight);
            rt->attachColorBuffer(diffuseIrradianceMap);

            // Create shaders and uniforms
            Shader* shader = Cyan::createShader("../../shader/shader_diff_irradiance.vs", "../../shader/shader_diff_irradiance.fs");
            Uniform* u_projection = Cyan::createUniform("projection", Uniform::Type::u_mat4);
            Uniform* u_view = Cyan::createUniform("view", Uniform::Type::u_mat4);
            Uniform* u_envmapSampler = Cyan::createUniform("envmapSampler", Uniform::Type::u_sampler2D);
            shader->bindUniform(u_projection);
            shader->bindUniform(u_view);
            shader->bindUniform(u_envmapSampler);

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
                s_gfxc->setRenderTarget(rt, (1 << f));
                s_gfxc->setViewport(0, 0, kViewportWidth, kViewportHeight);
                s_gfxc->setShader(shader);
                s_gfxc->setUniform(u_projection);
                s_gfxc->setUniform(u_view);
                s_gfxc->setSampler(u_envmapSampler, 0);
                s_gfxc->setTexture(_envMap, 0);
                s_gfxc->setPrimitiveType(PrimitiveType::TriangleList);
                s_gfxc->setVertexArray(cubeMesh->m_subMeshes[0]->m_vertexArray);

                s_gfxc->drawIndex(cubeMesh->m_subMeshes[0]->m_numVerts, 0);
                s_gfxc->reset();
            }

            // Recover the viewport dimensions
            s_gfxc->setViewport(origViewport.x, origViewport.y, origViewport.z, origViewport.w);
            s_gfxc->setDepthControl(DepthControl::kEnable);

            return diffuseIrradianceMap;
        }
    }
}