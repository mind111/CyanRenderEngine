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

namespace Cyan
{
    void init();

    // Buffers
    VertexBuffer* createVertexBuffer(void* data, u32 sizeInBytes, VertexSpec&& vertexSpec);
    RegularBuffer* createRegularBuffer(u32 totalSize);

    /* RenderTarget */
    RenderTarget* createRenderTarget(u32 _width, u32 _height);
    RenderTarget* createDepthRenderTarget(u32 width, u32 height);

    /* Buffer */
    void setBuffer(RegularBuffer* _buffer, void* data, u32 _sizeInBytes);

    struct TriangleIndex
    {
        u32 submeshIndex;
        u32 triangleIndex;
    };

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
                    cyanInfo("%s: %.3f ms \n", m_timedBlockName.c_str(), m_durationInMs);
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
#if 0
        Texture* loadEquirectangularMap(const char* _name, const char* _file, bool _hdr=false);
        Texture* prefilterEnvMapDiffuse(const char* _name, Texture* _envMap);
        Texture* prefilterEnvmapSpecular(Texture* envMap);
        Texture* generateBrdfLUT();
        Texture* createFlatColorTexture(const char* name, u32 width, u32 height, glm::vec4 color);
#endif

        //-
        // Mesh related
        void computeAABB(Mesh* parent);
        Mesh* createCubeMesh(const char* _name);
        glm::mat4 computeMeshNormalization(Mesh* parent);

    }; // namespace Toolkit

    namespace AssetGen
    {
        Mesh* createTerrain(float extendX, float extendY);
    }; // namespace AssetGen

} // namespace Cyan
