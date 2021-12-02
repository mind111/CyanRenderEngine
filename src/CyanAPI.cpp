#include <fstream>
#include <unordered_map>
#include <stack>

#include "tiny_obj_loader.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "json.hpp"

#include "Asset.h"
#include "CyanAPI.h"
#include "GfxContext.h"
#include "Camera.h"
#include "GraphicsSystem.h"

namespace Cyan
{
    class MeshManager
    {

    };

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

    static std::vector<Mesh*> s_meshes;
    static std::vector<LightProbe> s_probes;
    static SceneNode s_sceneNodes[kMaxNumSceneNodes] = { };
    static GfxContext* s_gfxc = nullptr;
    static void* s_memory = nullptr;
    static LinearAllocator* s_allocator = nullptr;

    static std::vector<Uniform*> s_uniforms;
    static std::unordered_map<std::string, u32> s_uniformRegistry;
    static std::unordered_map<std::string, u32> s_shaderRegistry;

    static HandleAllocator s_uniformHandleAllocator = { };
    static HandleAllocator s_shaderHandleAllocator = { };

    // each material type share same handle as its according shader 
    static Shader* m_shaders[kMaxNumShaders];
    static Material* m_materialTypes[kMaxNumMaterialTypes];

    static u32 m_numSceneNodes = 0u;

    void init()
    {
        CYAN_ASSERT(!s_gfxc, "Graphics context was already initialized");
        // Create global graphics context
        s_gfxc = new GfxContext;
        s_gfxc->init();
        s_memory = (void*)(new char[1024 * 1024 * 1024]); // 1GB memory pool
        s_allocator = LinearAllocator::create(s_memory, 1024 * 1024 * 1024);
        // reserve 0th item for null handle
        s_uniforms.resize(kMaxNumUniforms + 1);
        s_shaderHandleAllocator.m_nextHandle = 1u;
        s_uniformHandleAllocator.m_nextHandle = 1u;
    }

    LinearAllocator* getAllocator()
    {
        return s_allocator;
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
        s_sceneNodes[m_numSceneNodes].m_meshInstance = nullptr;
        return &s_sceneNodes[m_numSceneNodes++]; 
    }

    SceneNode* createSceneNode(const char* name, Transform transform, Mesh* mesh, bool hasAABB)
    {
        SceneNode* node = allocSceneNode();
        CYAN_ASSERT(name, "Name must be passed to createSceneNode().")
        strcpy(node->m_name, name);
        node->m_localTransform = transform;
        node->m_worldTransform = transform;
        node->m_hasAABB = hasAABB;
        if (mesh)
        {
            node->m_meshInstance = mesh->createInstance();
            if (mesh->m_shouldNormalize)
            {
                glm::mat4 localTransformMat = node->m_localTransform.toMatrix() * mesh->m_normalization;
                node->setLocalTransform(localTransformMat);
            }
        }
        return node;
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
        glNamedBufferData(buffer->m_ssbo, buffer->m_totalSize, nullptr, GL_DYNAMIC_DRAW);
        return buffer;
    }

    VertexBuffer* createVertexBuffer(void* data, u32 sizeInBytes, u32 strideInBytes, u32 numVerts)
    {
        VertexBuffer* vb = new VertexBuffer;
        u32 offset = 0;
        vb->m_data = new char[sizeInBytes];
        vb->m_sizeInBytes = sizeInBytes;
        vb->m_strideInBytes = strideInBytes;
        vb->m_numVerts = numVerts;
        // let vertex buffer owns a copy of the raw vertex data
        memcpy(vb->m_data, data, vb->m_sizeInBytes);
        glCreateBuffers(1, &vb->m_vbo);
        glNamedBufferData(vb->m_vbo, sizeInBytes, vb->m_data, GL_STATIC_DRAW);
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

    Mesh* createMesh(const char* name, std::string& file, bool normalize)
    {
        auto assetManager = GraphicsSystem::getSingletonPtr()->getAssetManager(); 
        return assetManager->loadMesh(file, name, normalize);
    }

    Mesh* getMesh(const char* _name)
    {
        if (_name)
        {
            for (auto mesh : s_meshes)
            {
                if (strcmp(mesh->m_name.c_str(), _name) == 0)
                    return mesh; 
            }
        }
        return 0;
    }

    Scene* createScene(const char* name, const char* file)
    {
        Toolkit::GpuTimer loadSceneTimer("createScene()", true);
        Scene* scene = new Scene;
        scene->m_name = std::string(name);

        scene->m_lastProbe = nullptr;
        scene->m_currentProbe = nullptr;
        scene->m_rootEntity = nullptr;
        // create root entity
        scene->m_rootEntity = SceneManager::getSingletonPtr()->createEntity(scene, "SceneRoot", Transform());
        auto assetManager = GraphicsSystem::getSingletonPtr()->getAssetManager(); 
        assetManager->loadScene(scene, file);
        loadSceneTimer.end();
        return scene;
    }

    // TODO: implement this
    Shader* createVsGsPsShader(const char* name, const char* vsSrc, const char* gsSrc, const char* fsSrc)
    {
        using ShaderEntry = std::pair<std::string, u32>;

        ShaderSource src = {0};
        src.vsSrc = vsSrc;
        src.gsSrc = gsSrc;
        src.fsSrc = fsSrc;

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
        shader->init(src, Shader::Type::VsGsPs);
        return shader;
    }

    Shader* getShader(const char* name)
    {
        auto itr = s_shaderRegistry.find(std::string(name));
        if (itr != s_shaderRegistry.end())
        {
            return m_shaders[itr->second]; 
        }
        return nullptr;
    }

    u32 findShader(const char* name)
    {
        auto itr = s_shaderRegistry.find(std::string(name));
        if (itr != s_shaderRegistry.end())
        {
            return itr->second;
        }
        return 0u;
    }

    Shader* createShader(const char* name, const char* vertSrc, const char* fragSrc)
    {
        using ShaderEntry = std::pair<std::string, u32>;
        // found a existing shader
        if (u32 foundShader = findShader(name))
        {
            return m_shaders[foundShader];
        }

        // TODO: Memory management
        u32 handle = ALLOC_HANDLE(Shader)
        CYAN_ASSERT(handle < kMaxNumShaders,  "Too many shader created!!!")
        m_shaders[handle] = new Shader();
        Shader* shader = m_shaders[handle];
        shader->m_name = std::string(name);
        s_shaderRegistry.insert(ShaderEntry(shader->m_name, handle));
        shader->buildVsPsFromSource(vertSrc, fragSrc);
        return shader;
    }

    Shader* createCsShader(const char* name, const char* csSrc)
    {
        using ShaderEntry = std::pair<std::string, u32>;
        // found a existing shader
        if (u32 foundShader = findShader(name))
        {
            return m_shaders[foundShader];
        }

        // TODO: Memory management
        u32 handle = ALLOC_HANDLE(Shader)
        CYAN_ASSERT(handle < kMaxNumShaders,  "Too many shader created!!!")
        m_shaders[handle] = new Shader();
        Shader* shader = m_shaders[handle];
        shader->m_name = std::string(name);
        s_shaderRegistry.insert(ShaderEntry(shader->m_name, handle));
        shader->buildCsFromSource(csSrc);
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

    // depth only render target; mainly for directional shadow map
    RenderTarget* createDepthRenderTarget(u32 width, u32 height)
    {
        RenderTarget* rt = new RenderTarget();
        rt->m_width = width;
        rt->m_height = height;
        glCreateFramebuffers(1, &rt->m_frameBuffer);
        return rt;
    }

    u32 findUniform(const char* name)
    {
        auto itr = s_uniformRegistry.find(std::string(name));
        if (itr != s_uniformRegistry.end())
        {
            return itr->second; 
        }
        return 0;
    }

    Uniform* createUniform(const char* name, Uniform::Type type)
    {
        // TODO: build hashkey using name and type
        if (u32 foundHandle = findUniform(name)) 
        {
            return s_uniforms[foundHandle];
        }

        u32 handle = ALLOC_HANDLE(Uniform)
        // allocate from global uniform buffer
        Uniform* uniform = (Uniform*)s_allocator->alloc(sizeof(Uniform));
        s_uniforms.insert(s_uniforms.begin() + handle, uniform);
        uniform->m_type = type;
        strcpy(uniform->m_name, name);
        u32 size = uniform->getSize();
        uniform->m_valuePtr = s_allocator->alloc(size);
        memset((u8*)uniform->m_valuePtr, 0x0, size);
        s_uniformRegistry.insert(std::pair<std::string,u32>(std::string(name), handle));
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
            // printf("[Warning] Unknown uniform with name %s\n", name);
        }
        return (UniformHandle)-1;
    }

    Uniform* getUniform(UniformHandle handle)
    {
        // CYAN_ASSERT(handle < kMaxNumUniforms, "Uniform handle %u out of bound", handle)
        if (handle > kMaxNumUniforms) return nullptr;
        return s_uniforms[handle];
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
            case GL_SAMPLER_2D_ARRAY:
                return Uniform::Type::u_sampler2DArray;
            case GL_SAMPLER_3D:
                return Uniform::Type::u_sampler3D;
            case GL_SAMPLER_2D_SHADOW:
                return Uniform::Type::u_sampler2DShadow;
            case GL_SAMPLER_CUBE:
                return Uniform::Type::u_samplerCube;
            case GL_IMAGE_3D:
                return Uniform::Type::u_image3D;
            case GL_UNSIGNED_INT_IMAGE_3D:
                return Uniform::Type::u_uimage3D;
            case GL_UNSIGNED_INT:
                return Uniform::Type::u_uint;
            case GL_UNSIGNED_INT_ATOMIC_COUNTER:
                return Uniform::Type::u_atomic_uint;
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
                    case Uniform::Type::u_sampler2DArray:
                    case Uniform::Type::u_samplerCube:
                    case Uniform::Type::u_image3D:   
                    case Uniform::Type::u_uimage3D:
                    case Uniform::Type::u_sampler3D:
                    case Uniform::Type::u_sampler2DShadow:
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
            subMesh->m_vertexArray->m_vertexBuffer->m_vertexAttribs.push_back({VertexAttrib::DataType::Float, 3, 3 * sizeof(f32), 0 });
            subMesh->m_vertexArray->init();
            subMesh->m_triangles.m_numVerts = 0;

            cubeMesh->m_subMeshes.push_back(subMesh);
            cubeMesh->m_normalization = glm::mat4(1.f);
            computeAABB(cubeMesh);
            s_meshes.push_back(cubeMesh);
            return cubeMesh;
        }

        void computeAABB(Mesh* mesh)
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

            // note(min): FLT_MIN, FLT_MAX returns positive float limits
            float meshXMin =  FLT_MAX; 
            float meshXMax = -FLT_MAX;
            float meshYMin =  FLT_MAX; 
            float meshYMax = -FLT_MAX;
            float meshZMin =  FLT_MAX; 
            float meshZMax = -FLT_MAX;

            for (auto sm : mesh->m_subMeshes)
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
            // TODO: need to watch out for zmin and zmax as zmin is actually greater than 0 
            // in OpenGL style right-handed coordinate system. This is why meshZMax is put in 
            // pMin while meshZMin is put in pMax.
            mesh->m_aabb.m_pMin = glm::vec4(meshXMin, meshYMin, meshZMax, 1.0f);
            mesh->m_aabb.m_pMax = glm::vec4(meshXMax, meshYMax, meshZMin, 1.0f);
            // TODO: cleanup
            mesh->m_aabb.init();
        }

        glm::mat4 computeMeshNormalization(Mesh* mesh)
        {
            computeAABB(mesh);

            float meshXMin = mesh->m_aabb.m_pMin.x;
            float meshXMax = mesh->m_aabb.m_pMax.x;
            float meshYMin = mesh->m_aabb.m_pMin.y;
            float meshYMax = mesh->m_aabb.m_pMax.y;
            float meshZMin = mesh->m_aabb.m_pMin.z;
            float meshZMax = mesh->m_aabb.m_pMax.z;

            f32 scale = Max(Cyan::fabs(meshXMin), Cyan::fabs(meshXMax));
            scale = Max(scale, Max(Cyan::fabs(meshYMin), Cyan::fabs(meshYMax)));
            scale = Max(scale, Max(Cyan::fabs(meshZMin), Cyan::fabs(meshZMax)));
            return glm::scale(glm::mat4(1.f), glm::vec3(1.f / scale));
        }

        // TODO: implement this to handle scale for submeshes correctly
        glm::mat4 computeMeshesNormalization(std::vector<Mesh*> meshes) {
            for (auto& mesh : meshes) {
                continue;
            }
            return glm::mat4(1.f);
        }

        // Load equirectangular map into a cubemap
        Texture* loadEquirectangularMap(const char* _name, const char* _file, bool _hdr)
        {
            auto textureManager = TextureManager::getSingletonPtr();
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
                spec.m_dataType = Texture::DataType::Float;
                equirectMap = textureManager->createTextureHDR(_name, _file, spec);
                spec.m_type = Texture::Type::TEX_CUBEMAP;
                spec.m_width = kViewportWidth;
                spec.m_height = kViewportHeight;
                spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
                spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
                spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
                envmap = textureManager->createTextureHDR(_name, spec);
            }
            else
            {
                spec.m_dataType = Texture::DataType::UNSIGNED_BYTE;
                equirectMap = textureManager->createTexture(_name, _file, spec);
                spec.m_type = Texture::Type::TEX_CUBEMAP;
                spec.m_width = kViewportWidth;
                spec.m_height = kViewportHeight;
                spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
                spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
                spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
                envmap = textureManager->createTexture(_name, spec);
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
            Mesh* cubeMesh = Cyan::getMesh("CubeMesh");
            if (!cubeMesh)
            {
                cubeMesh = Cyan::Toolkit::createCubeMesh("CubeMesh");
            }
            // Cache viewport config
            Viewport origViewport = s_gfxc->m_viewport;
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
                s_gfxc->setViewport({ 0, 0, kViewportWidth, kViewportHeight });
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
            s_gfxc->setViewport(origViewport);
            s_gfxc->setDepthControl(DepthControl::kEnable);
            return envmap;
        }

        Texture* prefilterEnvMapDiffuse(const char* _name, Texture* envMap)
        {
            auto textureManager = TextureManager::getSingletonPtr();
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
                spec.m_dataType = Texture::DataType::Float;
                diffuseIrradianceMap = textureManager->createTextureHDR(envMap->m_name.c_str(), spec);
            }
            else
            {
                spec.m_dataType = Texture::DataType::UNSIGNED_BYTE;
                diffuseIrradianceMap = textureManager->createTexture(envMap->m_name.c_str(), spec);
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

            Mesh* cubeMesh = Cyan::getMesh("CubeMesh");
            if (!cubeMesh)
            {
                cubeMesh = Cyan::Toolkit::createCubeMesh("CubeMesh");
            }
            // Cache viewport config
            Viewport origViewport = s_gfxc->m_viewport;
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
                s_gfxc->setViewport({ 0, 0, kViewportWidth, kViewportHeight });
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
            s_gfxc->setViewport(origViewport);
            s_gfxc->setDepthControl(DepthControl::kEnable);
            return diffuseIrradianceMap;
        }

        Texture* prefilterEnvmapSpecular(Texture* envMap)
        {
            CYAN_ASSERT(envMap->m_type == Texture::Type::TEX_CUBEMAP, "Cannot prefilter a non-cubemap texture")
            auto textureManager = TextureManager::getSingletonPtr();
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
                spec.m_dataType = Texture::DataType::Float;
                prefilteredEnvMap = textureManager->createTextureHDR(envMap->m_name.c_str(), spec);
            }
            else
            {
                spec.m_dataType = Texture::DataType::UNSIGNED_BYTE;
                prefilteredEnvMap = textureManager->createTexture(envMap->m_name.c_str(), spec);
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
            Mesh* cubeMesh = Cyan::getMesh("CubeMesh");
            if (!cubeMesh)
            {
                cubeMesh = Cyan::Toolkit::createCubeMesh("CubeMesh");
            }
            // Cache viewport config
            Viewport origViewport = s_gfxc->m_viewport;
            const u32 kNumMips = 10u;
            u32 mipWidth = prefilteredEnvMap->m_width; 
            u32 mipHeight = prefilteredEnvMap->m_height;
            RenderTarget* rts[kNumMips];;
            for (u32 mip = 0; mip < kNumMips; ++mip)
            {
                rts[mip] = createRenderTarget(mipWidth, mipHeight);
                rts[mip]->attachColorBuffer(prefilteredEnvMap, mip);
                s_gfxc->setViewport({ 0u, 0u, mipWidth, mipHeight });
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
            s_gfxc->setViewport(origViewport);
            s_gfxc->setDepthControl(DepthControl::kEnable);
            return prefilteredEnvMap;
        }

        // integrate brdf for specular IBL
        Texture* generateBrdfLUT()
        {
            auto textureManager = TextureManager::getSingletonPtr();

            const u32 kTexWidth = 512u;
            const u32 kTexHeight = 512u;
            TextureSpec spec = { };
            spec.m_type = Texture::Type::TEX_2D;
            spec.m_format = Texture::ColorFormat::R16G16B16A16;
            spec.m_dataType = Texture::DataType::Float;
            spec.m_numMips = 1u;
            spec.m_width = kTexWidth;
            spec.m_height = kTexHeight;
            spec.m_min = Texture::Filter::LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_data = nullptr;
            Texture* outputTexture = textureManager->createTextureHDR("integrateBrdf", spec); 
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
            Viewport origViewport = gfxc->m_viewport;
            gfxc->setViewport({ 0, 0, kTexWidth, kTexHeight } );
            gfxc->setShader(shader);
            gfxc->setRenderTarget(rt, 0);
            glBindVertexArray(vao);
            gfxc->setDepthControl(Cyan::DepthControl::kDisable);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            gfxc->setDepthControl(Cyan::DepthControl::kEnable);
            gfxc->setViewport(origViewport);
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
            auto textureManager = TextureManager::getSingletonPtr();
            TextureSpec spec = { };
            spec.m_type = Texture::Type::TEX_2D;
            spec.m_format = Texture::ColorFormat::R8G8B8A8;
            spec.m_dataType = Texture::DataType::UNSIGNED_BYTE;
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
            Texture* texture = textureManager->createTexture(name, spec);
            RenderTarget* rt = createRenderTarget(width, height);
            Uniform* u_color = createUniform("color", Uniform::Type::u_vec4);
            setUniform(u_color, &color.r);
            rt->attachColorBuffer(texture);

            auto gfxc = getCurrentGfxCtx();
            Viewport origViewport = gfxc->m_viewport;
            gfxc->setViewport({ 0u, 0u, texture->m_width, texture->m_height });
            gfxc->setShader(shader);
            gfxc->setRenderTarget(rt, 0);
            gfxc->setUniform(u_color);
            glBindVertexArray(vao);
            gfxc->setDepthControl(Cyan::DepthControl::kDisable);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            gfxc->setDepthControl(Cyan::DepthControl::kEnable);
            gfxc->setViewport(origViewport);
            gfxc->reset();

            return texture;
        }
    } // Toolkit

    namespace AssetGen
    {
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
            mesh->m_name = std::string("TerrainMesh");
            Mesh::SubMesh* subMesh = new Cyan::Mesh::SubMesh;
            subMesh->m_numVerts = (u32)vertices.size();
            u32 stride = sizeof(Vertex);
            u32 offset = 0;
            VertexBuffer* vb = Cyan::createVertexBuffer(vertices.data(), subMesh->m_numVerts * sizeof(Vertex), stride, subMesh->m_numVerts);
            vb->m_vertexAttribs.push_back({ VertexAttrib::DataType::Float, 3, stride, offset });
            offset += sizeof(glm::vec3);
            vb->m_vertexAttribs.push_back({ VertexAttrib::DataType::Float, 3, stride, offset });
            offset += sizeof(glm::vec3);
            vb->m_vertexAttribs.push_back({ VertexAttrib::DataType::Float, 3, stride, offset });
            offset += sizeof(glm::vec3);
            vb->m_vertexAttribs.push_back({ VertexAttrib::DataType::Float, 2, stride, offset });
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