#pragma once

#include <string>
#include <unordered_map>
#include <functional>

#include <glew/glew.h>
#include <stbi/stb_image.h>
#include <glm/glm.hpp>

#include "Singleton.h"
#include "Common.h"
#include "Window.h"
#include "Texture.h"
#include "RenderTarget.h"
#include "VertexArray.h"
#include "Shader.h"
#include "Mesh.h"
#include "ShaderStorageBuffer.h"

namespace Cyan 
{
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

    class GfxContext : public Singleton<GfxContext> 
    {
    public:
        GfxContext(GLFWwindow* window) 
            : m_glfwWindow(window)
        { }
        ~GfxContext() { }

        void setPixelPipeline(PixelPipeline* pixelPipelineObject, const std::function<void(VertexShader*, PixelShader*)>& setupShaders = [](VertexShader* vs, PixelShader* ps) {});
        void setGeometryPipeline(GeometryPipeline* geometryPipelineObject, const std::function<void(VertexShader*, GeometryShader*, PixelShader*)>& setupShaders = [](VertexShader*, GeometryShader*, PixelShader*) {});
        void setComputePipeline(ComputePipeline* computePipelineObject, const std::function<void(ComputeShader*)>& setupShaders);

        void setTexture(Texture* texture, u32 binding);
        // todo: implement this
        void setImage(Texture2DArray* textuerArray, u32 binding, u32 layered = true, u32 layer = 0) { }

        template<typename T>
        void setShaderStorageBuffer(ShaderStorageBuffer<T>* buffer) 
        {
            auto entry = m_shaderStorageBindingMap.find(buffer->name);
            if (entry == m_shaderStorageBindingMap.end())  
            {
                m_shaderStorageBindingMap[buffer->name] = m_nextShaderStorageBinding;
                buffer->bind(m_nextShaderStorageBinding++);
            }
            else 
            {
                buffer->bind(entry->second);
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

    private:
        void setShaderInternal(Shader* shader);

        static constexpr u32 kMaxNumTextureUnits = 32u;
        std::unordered_map<std::string, u32> m_textureBindingMap;
        u32 m_nextTextureBindingUnit = 0u;

        static constexpr u32 kMaxNumShaderStorageBindigs = 32u;
        std::unordered_map<std::string, u32> m_shaderStorageBindingMap;
        u32 m_nextShaderStorageBinding = 0u;

        GLFWwindow* m_glfwWindow;
        Shader* m_shader = nullptr;
        Viewport m_viewport;
        VertexArray* m_va = nullptr;
        GLenum m_primitiveType;
    };
}