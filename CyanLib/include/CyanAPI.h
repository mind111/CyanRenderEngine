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

    /* RenderTarget */
    RenderTarget* createRenderTarget(u32 width, u32 height);
    RenderTarget* createDepthOnlyRenderTarget(u32 width, u32 height);

    struct TriangleIndex
    {
        u32 submeshIndex;
        u32 triangleIndex;
    };

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
            timedBlockName.assign(std::string(name)); // is this faster ...?
            start = std::chrono::high_resolution_clock::now();
            durationInMS = 0.0;
            bPrint = print; 
        }
        ~GpuTimer() {}

        // end
        void end()
        {
            // Flush GPU commands
            glFinish();
            stop = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
            durationInMS = duration.count();
            if (bPrint)
                cyanInfo("%s: %.3f ms", timedBlockName.c_str(), durationInMS);
        }

        f32 getDurationInMS()
        {
            return (f32)durationInMS;
        }

        TimeSnapShot start;
        TimeSnapShot stop;
        // elapsed time in %.2f ms
        f64 durationInMS; 
        std::string timedBlockName;
        bool bPrint;
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
} // namespace Cyan
