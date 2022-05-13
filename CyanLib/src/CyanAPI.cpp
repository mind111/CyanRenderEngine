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
    static const u32 kMaxNumSceneNodes = 100000;

    static std::vector<Mesh*> s_meshes;
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

    VertexBuffer* createVertexBuffer(void* data, u32 sizeInBytes, VertexSpec&& vertexSpec)
    {
        VertexBuffer* vb = new VertexBuffer(data, sizeInBytes, std::move(vertexSpec));
        return vb;
    }

#if 0
    Mesh* createMesh(const char* name, std::string& file, bool normalize, bool generateLightMapUv)
    {
        auto assetManager = GraphicsSystem::get()->getAssetManager(); 
        return assetManager->loadMesh(file, name, normalize, generateLightMapUv);
    }
#endif

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
        cyanInfo("Building VsGsPs shader: %s", name);
        shader->init(src, Shader::Type::VsGsPs);
        return shader;
    }

    Shader* getMaterialShader(const char* name)
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
        cyanInfo("Building VsPs shader: %s", name);
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
        cyanInfo("Building compute shader: %s", name);
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

    RenderTarget* createRenderTarget(u32 width, u32 height)
    {
        RenderTarget* rt = new RenderTarget();
        rt->width = width;
        rt->height = height;
        glCreateFramebuffers(1, &rt->fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
        {
            glCreateRenderbuffers(1, &rt->rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, rt->rbo);
            {
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, rt->width, rt->height);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt->rbo);
            }
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        rt->validate();
        return rt;
    }

    // depth only render target
    RenderTarget* createDepthRenderTarget(u32 width, u32 height)
    {
        RenderTarget* rt = new RenderTarget();
        rt->width = width;
        rt->height = height;
        glCreateFramebuffers(1, &rt->fbo);
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
#if 0
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
#endif
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
#if 0
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

        GLuint programId = material->m_shader->handle;
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
#endif

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
#if 0
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
                for (auto attrib : sm->m_vertexArray->vb->attributes)
                    vertexSize += attrib.m_size;

                float xMin = findMin((float*)sm->m_vertexArray->vb->m_data, sm->m_numVerts, vertexSize, 0);
                float yMin = findMin((float*)sm->m_vertexArray->vb->m_data, sm->m_numVerts, vertexSize, 1);
                float zMin = findMin((float*)sm->m_vertexArray->vb->m_data, sm->m_numVerts, vertexSize, 2);
                float xMax = findMax((float*)sm->m_vertexArray->vb->m_data, sm->m_numVerts, vertexSize, 0);
                float yMax = findMax((float*)sm->m_vertexArray->vb->m_data, sm->m_numVerts, vertexSize, 1);
                float zMax = findMax((float*)sm->m_vertexArray->vb->m_data, sm->m_numVerts, vertexSize, 2);
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
            mesh->m_aabb.pmin = glm::vec4(meshXMin, meshYMin, meshZMax, 1.0f);
            mesh->m_aabb.pmax = glm::vec4(meshXMax, meshYMax, meshZMin, 1.0f);
            // TODO: cleanup
            mesh->m_aabb.init();
        }

        glm::mat4 computeMeshNormalization(Mesh* mesh)
        {
            computeAABB(mesh);

            float meshXMin = mesh->m_aabb.pmin.x;
            float meshXMax = mesh->m_aabb.pmax.x;
            float meshYMin = mesh->m_aabb.pmin.y;
            float meshYMax = mesh->m_aabb.pmax.y;
            float meshZMin = mesh->m_aabb.pmin.z;
            float meshZMax = mesh->m_aabb.pmax.z;

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
#endif
#if 0
        // Load equirectangular map into a cubemap
        Texture* loadEquirectangularMap(const char* _name, const char* _file, bool _hdr)
        {
            auto textureManager = TextureManager::get();
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
            auto textureManager = TextureManager::get();
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
            auto textureManager = TextureManager::get();
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
            auto textureManager = TextureManager::get();

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
#endif

        // create a flat color albedo map via fragment shader
        Texture* createFlatColorTexture(const char* name, u32 width, u32 height, glm::vec4 color)
        {
            auto textureManager = TextureManager::get();
            TextureSpec spec = { };
            spec.type = Texture::Type::TEX_2D;
            spec.format = Texture::ColorFormat::R8G8B8A8;
            spec.dataType = Texture::DataType::UNSIGNED_BYTE;
            spec.width = width;
            spec.numMips = 1u;
            spec.height = height;
            spec.min = Texture::Filter::LINEAR;
            spec.mag = Texture::Filter::LINEAR;
            spec.s = Texture::Wrap::NONE;
            spec.t = Texture::Wrap::NONE;
            spec.r = Texture::Wrap::NONE;
            spec.data = nullptr;
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
//            rt->attachColorBuffer(texture);
            rt->setColorBuffer(texture, 0u);

            auto gfxc = getCurrentGfxCtx();
            Viewport origViewport = gfxc->m_viewport;
            gfxc->setViewport({ 0u, 0u, texture->width, texture->height });
            gfxc->setShader(shader);
            // gfxc->setRenderTarget(rt, 0);
            gfxc->setRenderTarget(rt, { 0 });
            gfxc->setUniform(u_color);
            glBindVertexArray(vao);
            gfxc->setDepthControl(Cyan::DepthControl::kDisable);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            gfxc->setDepthControl(Cyan::DepthControl::kEnable);
            gfxc->setViewport(origViewport);
            gfxc->reset();

            return texture;
        }
    } // Toolkitn
} // Cyan