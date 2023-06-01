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

    enum class DepthControl 
    {
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

        void bindProgramPipeline(ProgramPipeline* pipelineObject);
        void unbindProgramPipeline();
        u32 allocTextureUnit();
        void bindTexture(GfxTexture* texture, u32 textureUnit);
        void unbindTexture(u32 textureUnit);
        // todo: implement this
        void setImage(GfxTexture2DArray* textuerArray, u32 binding, u32 layered = true, u32 layer = 0) { }
        u32 allocShaderStorageBinding();
        void bindShaderStorageBuffer(ShaderStorageBuffer* buffer, u32 binding);
        void unbindShaderStorageBuffer(u32 binding);
        void setVertexArray(VertexArray* array);
        void setPrimitiveType(PrimitiveMode type);
        void setViewport(Viewport viewport);
        void setFramebuffer(Framebuffer* framebuffer);
        void setDepthControl(DepthControl ctrl);
        void setClearColor(glm::vec4 albedo);
        void setCullFace(FrontFace frontFace, FaceCull faceToCull);
#if 0
        void setUniform(Shader* shader, const char* uniformName, f32 data) 
        {
            i32 location = shader->getUniformLocation(uniformName);
            if (location >= 0)
            {
                glProgramUniform1f(shader->getGpuResource(), location, data);
            }
        }
#endif

        void drawIndexAuto(u32 numVerts, u32 offset=0);
        void drawIndex(u32 numIndices);
        // void multiDrawArrayIndirect(IndirectDrawBuffer* indirectDrawBuffer);

        void reset();

        void present();
        void clear();

        Framebuffer* createFramebuffer(u32 width, u32 height);
    private:

        GLFWwindow* m_glfwWindow = nullptr;
        Framebuffer* m_framebuffer = nullptr;
        Viewport m_viewport;
        ProgramPipeline* m_programPipeline = nullptr;
        GfxPipelineState m_gfxPipelineState;
        VertexArray* m_vertexArray = nullptr;
        glm::vec4 m_clearColor = glm::vec4(.2f, .2f, .2f, 1.f);

        struct TextureBindingManager
        {
            i32 alloc();
            void bind(GfxTexture* texture, u32 binding);
            void unbind(u32 binding);

            // todo: this is problematic, hard coding this hardware constraints for now
            static constexpr i32 kMaxNumTextureUnits = 32;
            std::array<GfxTexture*, kMaxNumTextureUnits> bindings;
        } m_textureBindingManager;

        struct ShaderStorageBindingManager
        {
            i32 alloc();
            void bind(ShaderStorageBuffer* buffer, u32 binding);
            void unbind(u32 binding);
            // todo: this is problematic, hard coding this hardware constraints for now

            static constexpr i32 kMaxNumBindingUnits = 32;
            std::array<ShaderStorageBuffer*, kMaxNumBindingUnits> bindings;
        } m_shaderStorageBindingManager;
    };
}