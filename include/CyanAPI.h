#pragma once

#include <cstring>

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
#include "GfxContext.h"

/* TODO: 
* implement MaterialInstance class
* remove the concept of shader uniform
* shader resource interface query

* implement handle system, every object can be identified using a handle
* implement a uniform cache to avoid calling glUniform() on uniforms that has not changed
* implement DrawCall and Frame struct
* memory usage visualization
* refactor shader uniforms, only material should keep reference to uniforms
* implement shader resource query to get rid of manual call to matl->bindUniform()
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

    u32 allocHandle();

    /* Getter */
    GfxContext* getCurrentGfxCtx();

    /* Buffer */
    VertexBuffer* createVertexBuffer(void* _data, u32 _sizeInBytes, u32 _strideInBytes, u32 _numVerts);
    RegularBuffer* createRegularBuffer(const char* _blockName, Shader* _shader, u32 _binding, u32 _sizeInBytes);

    /* Texture */
    Texture* createTexture(const char* _name, u32 _width, u32 _height, Texture::ColorFormat _format, Texture::Type _type=Texture::Type::TEX_2D, Texture::Filter _filter=Texture::Filter::LINEAR);
    Texture* createTextureHDR(const char* _name, u32 _width, u32 _height, Texture::ColorFormat _format, Texture::Type _type=Texture::Type::TEX_2D, Texture::Filter _filter=Texture::Filter::LINEAR);
    Texture* createTexture(const char* _name, const char* _file, Texture::Type _type=Texture::Type::TEX_2D, Texture::Filter _filter=Texture::Filter::LINEAR);
    Texture* createTextureHDR(const char* _name, const char* _file, Texture::Type _type=Texture::Type::TEX_2D, Texture::Filter _filter=Texture::Filter::LINEAR);
    Texture* getTexture(const char* _name);

    /* RenderTarget */
    RenderTarget* createRenderTarget(u32 _width, u32 _height);

    /* Shader */
    Shader* createShader(const char* vertSrc, const char* fragSrc);

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
      
    Material* createMaterial(Shader* _shader);

    /* Mesh */
    Mesh* createMesh(const char* _name, const char* _file);
    Mesh* getMesh(const char* _name);

    /* Scene */
    Scene* createScene(const char* _file);

    namespace Toolkit
    {
        //-
        // Cubemap related
        Texture* loadEquirectangularMap(const char* _name, const char* _file, bool _hdr=false);
        Texture* generateDiffsueIrradianceMap(const char* _name, Texture* _envMap, bool _hdr=false);

        //-
        // Mesh related
        Mesh* createCubeMesh(const char* _name);
        glm::mat4 computeMeshNormalization(Mesh* _mesh);

    } // namespace Toolkit
} // namespace Cyan
