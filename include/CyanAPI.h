#pragma once

#include <cstring>
#include <chrono>

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
    General:
        * add a asset manager UI that can display all the loaded assets.
        * add support for selecting entities via UI.
        * add support for adding entities via UI.
        * add support for saving the scene settings (serialization....?).
        * look into how to reduce load time (loading mesh, loading textures etc).
        * new project structure..? static lib, dll, and driver app ?
        * study about resource management.
        * think about how to handle uniform with same name but different type
        * memory usage visualization
    Rendering:
        Problems:
            * Specular anti-aliasing:
                * when either ndotv or ndotl is zero, the denominator in microfacet BRDF will be a 
                  really small number which cause the evaluated BRDF to be a larget number, causing
                  really bright specular highlights, and they tend to aliase around edge of the mesh.
                  To be more precise, this won't happen when ndotl is zero as this case will be culled by
                  the cosine theta (ndotl) term in the rendering equation. Strong highlight is valid especially
                  when both ndotv and ndotl are small numbers, because the fresnel will then be strong. Need
                  to research about how to combat specular aliasing.
                * This is a extreme advanced topic for now. Long term studying goal

            * A lot of noise in environment maps that contains high frequency details, this happens
              in both prefiltering diffuse/specular map
                * Would gaussian blurring help in this case?

            * histogram auto-exposure:

        * Normal Mapping:
            * draw debug normals
            * re-orthonormalize the normals after tangent space normal mapping
            * correct filtering
        * implement temporal anti-aliasing first to combat specular aliasing
        * look into how to reduce specular noise (NDF filtering ...?)
        * look into parameters in Disney principled BRDF
            * Disney's reparameterization of alpha in GGX specular is mainly because of matching
            with their diffuse BRDF
        * post-processing
            * auto-exposure  
        * study subsurface scattering
    Optimization:
        * implement a uniform cache to avoid calling glUniform() on uniforms that has not changed
        * implement DrawCall and Frame struct
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
        kTangents = (1 << 3),
        kLightMapUv = (1 << 4)
    };

    void init();

    #define ALLOC_HANDLE(type)   \
    alloc##type##Handle();       \

    u32 allocUniformHandle();
    u32 allocShaderHandle();

    GfxContext* getCurrentGfxCtx();

    /* Memory */
    LinearAllocator* getAllocator();
    #define CYAN_ALLOC(size)  Cyan::alloc(size)

    void* alloc(u32 sizeInBytes);

    // Buffers
    VertexArray*  createVertexArray(VertexBuffer* vb);
    VertexBuffer* createVertexBuffer(void* _data, u32 _sizeInBytes, u32 strideInBytes, u32 _numVerts);
    RegularBuffer* createRegularBuffer(u32 totalSize);

    /* Texture */
    void     addTexture(Texture* texture);
    Texture* getTexture(const char* _name);
    u32      getNumTextures();

    /* RenderTarget */
    RenderTarget* createRenderTarget(u32 _width, u32 _height);
    RenderTarget* createDepthRenderTarget(u32 width, u32 height);

    /* Shader */
    Shader* getShader(const char* name);
    Shader* createShader(const char* name, const char* vertSrc, const char* fragSrc);
    Shader* createCsShader(const char* name, const char* csSrc);
    Shader* createVsGsPsShader(const char* name, const char* vsSrcFile, const char* fsSrcFile, const char* gsSrcFile);

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
    void addMesh(Mesh* mesh);
    Mesh* createMesh(const char* name, std::string& file, bool normalize);
    Mesh* getMesh(const char* _name);
    struct TriangleIndex
    {
        u32 submeshIndex;
        u32 triangleIndex;
    };

    /* Scene */
    SceneNode* allocSceneNode();
    SceneNode* createSceneNode(const char* name, Transform transform, Mesh* mesh=nullptr, bool hasAABB=true);
    Scene* createScene(const char* name, const char* _file);
    LightProbe* getProbe(u32 index);
    u32         getNumProbes();

    /* voxel cone tracing */
    void voxelizeScene();
    void voxelizeMesh();

    namespace Toolkit
    {
        /*
            * How to evaluate OpenGL performance https://www.khronos.org/opengl/wiki/Performance 
        */
        struct GpuTimer
        {
            using TimeSnapShot = std::chrono::high_resolution_clock::time_point;

            // begin
            GpuTimer(const char* name, bool print=false) 
            {
                // Flush GPU commands
                glFinish();
                m_timedBlockName.assign(std::string(name)); // is this faster ...?
                m_begin = std::chrono::high_resolution_clock::now();
                m_durationInMs = 0.0;
                m_print = print; 
            }
            ~GpuTimer() {}

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

        struct ScopedTimer
        {
            using TimeSnapShot = std::chrono::high_resolution_clock::time_point;

            // begin
            ScopedTimer(const char* name, bool print=false) 
            {
                m_timedBlockName.assign(std::string(name)); // is this faster ...?
                m_begin = std::chrono::high_resolution_clock::now();
                m_durationInMs = 0.0;
                m_print = print; 
            }
            ~ScopedTimer() {
                end();
            }

            // end
            void end()
            {
                m_end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(m_end - m_begin);
                m_durationInMs = duration.count();
                u32 durationInSeconds = 0u;
                if (m_durationInMs > 1000.f)
                {
                    durationInSeconds = static_cast<u32>(round(m_durationInMs)) / 1000u;
                    m_durationInMs = f32(static_cast<u32>(round(m_durationInMs)) % 1000u);
                }
                if (m_print)
                {
                    if (durationInSeconds > 0) 
                        cyanInfo("%s: %d seconds %.3f ms", m_timedBlockName.c_str(), durationInSeconds, m_durationInMs)
                    else 
                        cyanInfo("%s: %.3f ms", m_timedBlockName.c_str(), m_durationInMs)
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
        void computeAABB(Mesh* mesh);
        Mesh* createCubeMesh(const char* _name);
        glm::mat4 computeMeshNormalization(Mesh* mesh);

    }; // namespace Toolkit

    namespace AssetGen
    {
        Mesh* createTerrain(float extendX, float extendY);
    }; // namespace AssetGen

    struct ByteBuffer
    {
        u32 m_pos;
        u32 m_size;
        void* m_data;

        f32 readF32();
        glm::vec2 readVec2();
        glm::vec3 readVec3();
        glm::vec4 readVec4();
    };

    // TODO: Move following structs into their own files
} // namespace Cyan
