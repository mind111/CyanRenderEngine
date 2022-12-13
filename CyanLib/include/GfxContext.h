#pragma once

#include <string>
#include <unordered_map>

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
#include "ShaderStorageBuffer.h"

namespace Cyan {
    struct Viewport
    {
        u32 x;
        u32 y;
        u32 width;
        u32 height;
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

    enum class DepthControl {
        kDisable = 0,
        kEnable,
        kCount
    };

    class GfxContext : public Singleton<GfxContext> {
    public:
        GfxContext(GLFWwindow* window) 
            : m_glfwWindow(window)
        { }
        ~GfxContext() { }

        void setShader(Shader* shader);
        void setTexture(ITextureRenderable* texture, u32 binding);

        template<typename T>
        void setShaderStorageBuffer(ShaderStorageBuffer<T>* buffer, u32 binding) {
            auto entry = shaderStorageBufferBindingMap[buffer->name];
            if (entry) {
                // gfx context state change: the binding unit of @buffer needs to be updated
                if (entry->second != binding) {
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer->getGpuObject());
                    entry->second = binding;
                }
            }
            else {
                shaderStorageBufferBindingMap.insert({ buffer->name, binding });
            }
        }

        void setVertexArray(VertexArray* array);
        void setPrimitiveType(PrimitiveMode type);
        void setViewport(Viewport viewport);
        void setRenderTarget(RenderTarget* renderTarget);
        void setRenderTarget(RenderTarget* rt, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers);
        void setDepthControl(DepthControl ctrl);
        void setClearColor(glm::vec4 albedo);
        void setCullFace(FrontFace frontFace, FaceCull faceToCull);

        void setUniform(Shader* shader, const char* uniformName, f32 data) {
            i32 location = shader->getUniformLocation(uniformName);
            if (location >= 0)
            {
                glProgramUniform1f(shader->getGpuObject(), location, data);
            }
        }

        void drawIndexAuto(u32 numVerts, u32 offset=0);
        void drawIndex(u32 numIndices);

        void flip();
        void clear();
        void reset();

    private:
        static constexpr u32 kMaxNumTextureUnits = 32u;
        static constexpr u32 kMaxNumShaderStorageBufferBindigs = 32u;
        // bit vector for texture unit bindings
        std::array<u32, kMaxNumTextureUnits> textureUnits;

        GLFWwindow* m_glfwWindow;
        Shader* m_shader = nullptr;
        Viewport m_viewport;
        VertexArray* m_va = nullptr;
        GLenum m_primitiveType;
        u32 transientTextureUnit;
        std::unordered_map<std::string, u32> shaderStorageBufferBindingMap;
    };
}