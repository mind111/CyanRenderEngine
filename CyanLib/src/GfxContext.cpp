#include "GfxContext.h"
#include "CyanRenderer.h"
#include "CyanAPI.h"

namespace Cyan
{
    i32 GfxContext::TextureBindingManager::alloc()
    {
        for (i32 i = 0; i < bindings.size(); ++i)
        {
            if (bindings[i] == nullptr)
            {
                return i;
            }
        }
        // unreachable
        assert(0);
    }

    void GfxContext::TextureBindingManager::bind(GfxTexture* texture, u32 binding)
    {
        assert(texture != nullptr);
        if (bindings[binding] != nullptr)
        {
            unbind(binding);
        }
        bindings[binding] = texture;
        glBindTextureUnit(binding, texture->getGpuResource());
    }

    void GfxContext::TextureBindingManager::unbind(u32 binding)
    {
        if (bindings[binding] != nullptr)
        {
            bindings[binding] = nullptr;
            glBindTextureUnit(binding, 0);
        }
    }

    i32 GfxContext::ShaderStorageBindingManager::alloc()
    {
        for (i32 i = 0; i < bindings.size(); ++i)
        {
            if (bindings[i] == nullptr)
            {
                return i;
            }
        }
        // unreachable
        assert(0);
    }

    void GfxContext::ShaderStorageBindingManager::bind(ShaderStorageBuffer* buffer, u32 binding)
    {
        assert(buffer != nullptr);
        unbind(binding);
        bindings[binding] = buffer;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer->getGpuResource());
    }

    void GfxContext::ShaderStorageBindingManager::unbind(u32 binding)
    {
        if (bindings[binding] != nullptr)
        {
            bindings[binding] = nullptr;
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, 0);
        }
    }

    GfxContext* Singleton<GfxContext>::singleton = nullptr;
    void GfxContext::initialize()
    {
    }

    glm::uvec2 GfxContext::getDefaultFramebufferSize()
    { 
        i32 width, height;
        glfwGetWindowSize(singleton->m_glfwWindow, &width, &height);
        return glm::uvec2(width, height);
    }

    Framebuffer* GfxContext::createFramebuffer(u32 width, u32 height)
    {
        static std::unordered_map<std::string, std::shared_ptr<Framebuffer>> framebufferMap;

        std::string key = std::to_string(width) + 'x' + std::to_string(height);
        auto entry = framebufferMap.find(key);
        if (entry == framebufferMap.end())
        {
            Framebuffer* framebuffer = Framebuffer::create(width, height);
            framebufferMap.insert({ key, std::shared_ptr<Framebuffer>(framebuffer)});
            return framebuffer;
        }
        else
        {
            return entry->second.get();
        }
    }

    u32 GfxContext::allocTextureUnit()
    {
        return (u32)m_textureBindingManager.alloc();
    }

    u32 GfxContext::allocShaderStorageBinding()
    {
        return (u32)m_shaderStorageBindingManager.alloc();
    }

    void GfxContext::bindProgramPipeline(ProgramPipeline* pipelineObject)
    {
        assert(pipelineObject != nullptr);
        /**
         * todo: how to determine if two pipelineObject are identical? maybe using an unique hash that's generated for each
            pipeline object instance? for now though, simply testing equality by memory address since we are not destroying shaders
            while the application is running
         */
        if (pipelineObject != m_programPipeline)
        {
            if (m_programPipeline != nullptr)
            {
                m_programPipeline->unbind(this);
            }
            glBindProgramPipeline(pipelineObject->getGpuResource());
        }
        m_programPipeline = pipelineObject;
    }

    void GfxContext::unbindProgramPipeline()
    {
        glBindProgramPipeline(0);
        m_programPipeline = nullptr;
    }

    void GfxContext::setCullFace(FrontFace frontFace, FaceCull faceToCull)
    {
        glEnable(GL_CULL_FACE);
        GLenum gl_frontFace, gl_faceToCull;
        switch(frontFace)
        {
            case FrontFace::ClockWise:
                gl_frontFace = GL_CW;
                break;
            case FrontFace::CounterClockWise:
                gl_frontFace = GL_CCW;
                break;
        }
        switch(faceToCull)
        {
            case FaceCull::Front:
                gl_faceToCull = GL_FRONT;
                break;
            case FaceCull::Back:
                gl_faceToCull = GL_BACK;
                break;
        }
        glFrontFace(gl_frontFace);
        glCullFace(gl_faceToCull);
    }

    void GfxContext::setDepthControl(DepthControl ctrl) {
        switch(ctrl)
        {
            case DepthControl::kEnable:
                glEnable(GL_DEPTH_TEST);
                break;
            case DepthControl::kDisable:
                glDisable(GL_DEPTH_TEST);
                break;
            default:
                break;
        }
    }

    void GfxContext::bindTexture(GfxTexture* texture, u32 textureUnit) 
    {
        m_textureBindingManager.bind(texture, textureUnit);
    }

    void GfxContext::unbindTexture(u32 textureUnit) 
    {
        m_textureBindingManager.unbind(textureUnit);
    }

    void GfxContext::bindShaderStorageBuffer(ShaderStorageBuffer* buffer, u32 binding) 
    {
        m_shaderStorageBindingManager.bind(buffer, binding);
    }

    void GfxContext::unbindShaderStorageBuffer(u32 binding) 
    {
        m_shaderStorageBindingManager.unbind(binding);
    }

    void GfxContext::setVertexArray(VertexArray* va) 
    {
        if (va == nullptr) 
        {
            m_vertexArray = nullptr;
            glBindVertexArray(0);
        }
        else
        {
            m_vertexArray = va;
            glBindVertexArray(m_vertexArray->getGpuResource());
        }
    }

    static GLenum translatePrimitiveMode(const PrimitiveMode& inType)
    {
        switch (inType)
        {
            case PrimitiveMode::TriangleList:
                return GL_TRIANGLES;
            case PrimitiveMode::LineStrip:
                return GL_LINE_STRIP;
            case PrimitiveMode::Line:
                return GL_LINES;
            case PrimitiveMode::Points:
                return GL_POINTS;
            default:
                assert(0);
        }
    }

    void GfxContext::setPrimitiveType(PrimitiveMode mode)
    {
        m_gfxPipelineState.primitiveMode = mode;
    }

    void GfxContext::setViewport(Viewport viewport)
    {
        glViewport(viewport.x, viewport.y, viewport.width, viewport.height);

        m_viewport = viewport;
    }

    void GfxContext::setFramebuffer(Framebuffer* framebuffer) 
    {
        if (!framebuffer)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }
        framebuffer->validate();
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->getGpuResource());

        m_framebuffer = framebuffer;
    }

    void GfxContext::setClearColor(glm::vec4 albedo)
    {
        glClearColor(albedo.r, albedo.g, albedo.b, albedo.a);

        m_clearColor = albedo;
    }

    void GfxContext::drawIndexAuto(u32 numVerts, u32 offset) 
    {
        glDrawArrays(translatePrimitiveMode(m_gfxPipelineState.primitiveMode), offset, numVerts);
    }

    void GfxContext::drawIndex(u32 numIndices) 
    {
        glDrawElements(translatePrimitiveMode(m_gfxPipelineState.primitiveMode), numIndices, GL_UNSIGNED_INT, reinterpret_cast<const void*>(0));
    }

#if 0
    /** todo:
     * This call requires a vertex array object being bound, since I'm using programmable vertex pulling, I'm not using vertex array object
     for multiDrawIndirect, need to use a proper vertex array object to pass the vertex data instead later. For now can bind a dummy vao to 
     suppress the API error.
     */
    void GfxContext::multiDrawArrayIndirect(IndirectDrawBuffer* indirectDrawBuffer)
    {
        indirectDrawBuffer->bind();
        // todo: the offset and stride parameter should be configurable at some point
        glMultiDrawArraysIndirect(translatePrimitiveMode(m_gfxPipelineState.primitiveMode), 0, indirectDrawBuffer->numDrawcalls(), 0);
    }
#endif

    void GfxContext::clear()
    {
        glClearColor(.2f, .2f, .2f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void GfxContext::reset()
    {
        if (m_framebuffer)
        {
            m_framebuffer->reset();
            m_framebuffer->unbind();
            m_framebuffer = nullptr;
        }

        m_viewport = {};
        if (m_programPipeline != nullptr)
        {
            m_programPipeline->unbind(this);
        }
        m_gfxPipelineState = GfxPipelineState { };
        if (m_vertexArray != nullptr)
        {
            m_vertexArray->unbind();
        }
    }

    void GfxContext::present()
    {
        glfwSwapBuffers(m_glfwWindow);
    }
}