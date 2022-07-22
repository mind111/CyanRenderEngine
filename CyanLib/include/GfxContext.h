#pragma once

#include "glew.h"
#include "stb_image.h"
#include "glm.hpp"

#include "Common.h"
#include "Window.h"
#include "Texture.h"
#include "RenderTarget.h"
#include "VertexArray.h"
#include "Shader.h"
#include "Mesh.h"

namespace Cyan
{
    struct Viewport
    {
        u32 m_x;
        u32 m_y;
        u32 m_width;
        u32 m_height;
    };

    enum class PrimitiveMode
    {
        TriangleList = 0,
        Line,
        LineStrip,
        Points
    };

    enum class FrontFace
    {
        ClockWise = 0,
        CounterClockWise
    };

    enum class FaceCull
    {
        Front = 0,
        Back
    };

    enum class DepthControl
    {
        kDisable = 0,
        kEnable,
        kCount
    };

    class GfxContext : public Singleton<GfxContext>
    {
    public:

        GfxContext(GLFWwindow* window) 
            : m_glfwWindow(window)
        { }
        ~GfxContext() { }

        void setShader(Shader* shader);
        void setTexture(ITextureRenderable* texture, u32 binding);
        void setVertexArray(VertexArray* array);
        void setBuffer(RegularBuffer* buffer, u32 binding);
        void setPrimitiveType(PrimitiveMode type);
        void setViewport(Viewport viewport);
        void setRenderTarget(RenderTarget* rt, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers);
        void setDepthControl(DepthControl ctrl);
        void setClearColor(glm::vec4 color);
        void setCullFace(FrontFace frontFace, FaceCull faceToCull);

        // textures that uses global binding points
        void setPersistentTexture(ITextureRenderable* texture, u32 binding)
        {
            if (texture)
            {
                textureUnits[binding] = 1;
                glBindTextureUnit(binding, texture->getGpuResource());
            }
            else
            {
                textureUnits[binding] = 0;
                glBindTextureUnit(binding, 0);
            }
        }

        // textures that uses different binding points from draw to draw
        i32 setTransientTexture(ITextureRenderable* texture) 
        {
            i32 availableTextureUnit = -1;
            if (texture)
            {
                for (u32 i = 0; i < kMaxNumTextureUnits; ++i)
                {
                    if (textureUnits[i] == 0)
                    {
                        textureUnits[i] = 1;
                        availableTextureUnit = i;
                        break;
                    }
                }
                if (availableTextureUnit >= 0)
                {
                    glBindTextureUnit(availableTextureUnit, texture->getGpuResource());
                }
                else
                {
                    cyanError("No available transient texture binding units")
                }
            }
            return availableTextureUnit;
        }

        void clearTransientTextureBindings()
        {
            for (u32 i = 0; i < kMaxNumTextureUnits; ++i)
            {
                textureUnits[i] = 0;
            }
        }

        void setUniform(Shader* shader, const char* uniformName, f32 data)
        {
            i32 location = shader->getUniformLocation(uniformName);
            if (location >= 0)
            {
                glProgramUniform1f(shader->getGpuResource(), location, data);
            }
        }

        void drawIndexAuto(u32 numVerts, u32 offset=0);
        void drawIndex(u32 numIndices);

        void flip();
        void clear();
        void reset();

    private:
        static constexpr u32 kMaxNumTextureUnits = 32u;
        // bit vector for texture unit bindings
        std::array<u32, kMaxNumTextureUnits> textureUnits;

        GLFWwindow* m_glfwWindow;
        Shader* m_shader = nullptr;
        Viewport m_viewport;
        VertexArray* m_va = nullptr;
        GLenum m_primitiveType;
        u32 transientTextureUnit;
    };
}