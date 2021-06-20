#pragma once

#include <cstring>
#include <chrono>

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "Mesh.h"
#include "Scene.h"
#include "VertexBuffer.h"
#include "Texture.h"
#include "Shader.h"
#include "Uniform.h"
#include "RenderTarget.h"
#include "Material.h"
#include "LightProbe.h"
#include "GfxContext.h"

/* TODO: 
    * implement a ui to allow dynamically change to different envmap.
    * implement a ui to allow dynamically change to different mesh.
    * get rid of heap memory usage, no more "new" in the code base.
    * implement MSAA.
    * new project structure..? static lib, dll, and driver app ?
    * study about resource management.

    * think about how to handle uniform with same name but different type
    * implement a simple logger
    * remove the concept of shader uniform
    * refactor shader uniforms, only material should keep reference to uniforms
    * maybe switch to use Hammersley sequence showed in Epic's notes?

    * implement handle system, every object can be identified using a handle
    * implement a uniform cache to avoid calling glUniform() on uniforms that has not changed
    * implement DrawCall and Frame struct
    * memory usage visualization
*/

/* FIXME: 
    * Bottom of the Cubemap always have a black square.
    * Look into why the render will contain "black" dots
*/
namespace Cyan
{
    struct Context
    {
        u32 m_numUniforms;
        u32 m_numMeshes;
        u32 m_numTextures;
    };

    struct LinearAllocator
    {
        void* m_data;
        u32 m_pos;
        u32 m_size;

        static LinearAllocator* create(void* data, u32 size)
        {
            auto alloc = new LinearAllocator; 
            alloc->m_data = data;
            alloc->m_pos = 0;
            alloc->m_size = size;
            return alloc;
        }

        void* alloc(u32 sizeInBytes) 
        {
            CYAN_ASSERT(m_pos + sizeInBytes < m_size, "out of memory")
            void* address = (void*)(static_cast<u8*>(m_data) + m_pos);
            m_pos += sizeInBytes;
            return address;
        }
    };

    enum VertexAttribFlag
    {
        kPosition = 0x1,
        kNormal   = (1 << 1),
        kTexcoord = (1 << 2),
        kTangents = (1 << 3)
    };


    void init();

    #define ALLOC_HANDLE(type)   \
    alloc##type##Handle();       \

    u32 allocUniformHandle();
    u32 allocShaderHandle();

    GfxContext* getCurrentGfxCtx();

    /* Memory */
    #define CYAN_ALLOC(size)  Cyan::alloc(size)

    void* alloc(u32 sizeInBytes);

    /* Buffer */
    VertexBuffer* createVertexBuffer(void* _data, u32 _sizeInBytes, u32 _strideInBytes, u32 _numVerts);
    RegularBuffer* createRegularBuffer(u32 totalSize);

    /* Texture */
    Texture* getTexture(const char* _name);
    Texture* createTexture(const char* _name, u32 _width, u32 _height, TextureSpec spec);
    Texture* createTextureHDR(const char* _name, u32 _width, u32 _height, TextureSpec spec);
    Texture* createTexture(const char* _name, const char* _file, TextureSpec& spec);
    Texture* createTextureHDR(const char* _name, const char* _file, TextureSpec& spec);

    /* RenderTarget */
    RenderTarget* createRenderTarget(u32 _width, u32 _height);

    /* Shader */
    Shader* createShader(const char* name, const char* vertSrc, const char* fragSrc);

    /* Buffer */
    void setBuffer(RegularBuffer* _buffer, void* data, u32 _sizeInBytes);

    /* Uniform */
    Uniform* createUniform(const char* _name, Uniform::Type _type);
    UniformBuffer* createUniformBuffer(u32 sizeInBytes=1024);
    void setUniform(Uniform* _uniform, void* _valuePtr);
    void setUniform(Uniform* _uniform, u32 _value);
    void setUniform(Uniform* _uniform, float _value);
    Uniform* createShaderUniform(Shader* _shader, const char* _name, Uniform::Type _type);
    Uniform* createMaterialUniform(Material* _material, const char* _name, Uniform::Type _type);
    UniformHandle getUniformHandle(const char* name);
    Uniform* getUniform(UniformHandle handle);
      
    Material* createMaterial(Shader* _shader);

    /* Mesh */
    Mesh* createMesh(const char* _name, const char* _file);
    Mesh* getMesh(const char* _name);

    /* Scene */
    Scene* createScene(const char* _file);
    LightProbe* getProbe(u32 index);

    namespace Toolkit
    {
        /*
            * How to evaluate OpenGL performance https://www.khronos.org/opengl/wiki/Performance 
        */
        struct ScopedTimer
        {
            using TimeSnapShot = std::chrono::high_resolution_clock::time_point;

            // begin
            ScopedTimer(const char* name, bool print=false) 
            {
                // Flush GPU commands
                glFinish();
                m_timedBlockName.assign(std::string(name)); // is this faster ...?
                m_begin = std::chrono::high_resolution_clock::now();
                m_durationInMs = 0.0;
                m_print = print; 
            }
            ~ScopedTimer() { }

            // end
            void end()
            {
                // Flush GPU commands
                glFinish();
                m_end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(m_end - m_begin);
                m_durationInMs = duration.count();
                if (m_print)
                {
                    printf("%s: %.3f ms \n", m_timedBlockName.c_str(), m_durationInMs);
                }
            }

            TimeSnapShot m_begin;
            TimeSnapShot m_end;
            // elapsed time in %.2f ms
            double m_durationInMs; 
            std::string m_timedBlockName;
            bool m_print;
        };

        //-
        // Cubemap related
        Texture* loadEquirectangularMap(const char* _name, const char* _file, bool _hdr=false);
        Texture* prefilterEnvMapDiffuse(const char* _name, Texture* _envMap);
        Texture* prefilterEnvmapSpecular(Texture* envMap);
        Texture* generateBrdfLUT();
        Texture* createFlatColorTexture(const char* name, u32 width, u32 height, glm::vec4 color);
        LightProbe createLightProbe(const char* name, const char* file, bool hdr=false);

        //-
        // Mesh related
        Mesh* createCubeMesh(const char* _name);
        glm::mat4 computeMeshNormalization(Mesh* _mesh);

    } // namespace Toolkit

    // TODO: Move following structs into their own files
} // namespace Cyan
