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
#include "Framebuffer.h"
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

    struct GfxPipelineState 
    {
        DepthControl depth = DepthControl::kEnable;
        PrimitiveMode primitiveMode = PrimitiveMode::TriangleList;
    };

    class GfxContext : public Singleton<GfxContext> 
    {
    public:
        GfxContext(GLFWwindow* window) 
            : m_glfwWindow(window)
        { 
        }
        ~GfxContext() { }

        void initialize();

        static glm::uvec2 getDefaultFramebufferSize();

        void setPixelPipeline(PixelPipeline* pixelPipelineObject, const std::function<void(VertexShader*, PixelShader*)>& setupShaders = [](VertexShader* vs, PixelShader* ps) {});
        void setGeometryPipeline(GeometryPipeline* geometryPipelineObject, const std::function<void(VertexShader*, GeometryShader*, PixelShader*)>& setupShaders = [](VertexShader*, GeometryShader*, PixelShader*) {});
        void setComputePipeline(ComputePipeline* computePipelineObject, const std::function<void(ComputeShader*)>& setupShaders);

        void setTexture(GfxTexture* texture, u32 textureUnit);
        // todo: implement this
        void setImage(GfxTexture2DArray* textuerArray, u32 binding, u32 layered = true, u32 layer = 0) { }

        void setShaderStorageBuffer(ShaderStorageBuffer* buffer) 
        {
            auto entry = m_shaderStorageBindingMap.find(buffer->getBlockName());
            if (entry == m_shaderStorageBindingMap.end())  
            {
                m_shaderStorageBindingMap[buffer->getBlockName()] = m_nextShaderStorageBinding;
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
        void setFramebuffer(Framebuffer* framebuffer);
        void setDepthControl(DepthControl ctrl);
        void setClearColor(glm::vec4 albedo);
        void setCullFace(FrontFace frontFace, FaceCull faceToCull);

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
        void multiDrawArrayIndirect(IndirectDrawBuffer* indirectDrawBuffer);

        void reset();

        void flip();
        void clear();

        Framebuffer* createFramebuffer(u32 width, u32 height);
    private:
        void setShaderInternal(Shader* shader);
        void setProgramPipelineInternal();
        void resetTextureBindingState();
        u32 allocTextureUnit();

        GLFWwindow* m_glfwWindow = nullptr;

        Framebuffer* m_framebuffer = nullptr;
        Viewport m_viewport;

        PixelPipeline* m_pixelPipeline = nullptr;
        std::unordered_map<std::string, u32> m_shaderStorageBindingMap;
        u32 m_nextShaderStorageBinding = 0u;

        GfxPipelineState m_gfxPipelineState;

        VertexArray* m_vertexArray = nullptr;
        glm::vec4 m_clearColor = glm::vec4(.2f, .2f, .2f, 1.f);

        // some hardware constants
        static i32 kMaxCombinedTextureUnits;
        static i32 kMaxCombinedShaderStorageBlocks;
        u32 numUsedTextureUnits = 0;
        std::vector<GfxTexture*> m_textureBindings;
    };
}