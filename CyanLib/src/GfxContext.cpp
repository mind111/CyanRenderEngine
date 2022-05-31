#include "GfxContext.h"
#include "CyanRenderer.h"
#include "CyanAPI.h"

namespace Cyan
{
    GfxContext* Singleton<GfxContext>::singleton = nullptr;

    void GfxContext::setShader(Shader* shader)
    {
        if (!shader)
        {
            m_shader->unbind();
            m_shader = nullptr;
        }
        else
        {
            if ((!m_shader) || (m_shader && (shader->getGpuResource() != m_shader->getGpuResource())))
            {
                m_shader = shader;
                m_shader->bind();
            }
        }
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

    void GfxContext::setDepthControl(DepthControl _ctrl)
    {
        switch(_ctrl)
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

    void GfxContext::setTexture(Texture* _texture, u32 binding)
    {
        // reset a texture unit
        if (!_texture)
        {
            glBindTextureUnit(binding, 0);
        }
        else
        {
            glBindTextureUnit(binding, _texture->handle);
        }
    }

    void GfxContext::setBuffer(RegularBuffer* _buffer, u32 binding)
    {
        // glBindBuffer(GL_SHADER_STORAGE_BUFFER, _buffer->m_ssbo);
        // void* baseAddr = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
        if (!_buffer)
        {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, 0);
            return;
        }

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, _buffer->m_ssbo);
        void* baseAddr = glMapNamedBuffer(_buffer->m_ssbo, GL_WRITE_ONLY);

        memcpy(baseAddr, _buffer->m_data, _buffer->m_sizeToUpload);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    void GfxContext::setVertexArray(VertexArray* va)
    {
        if (!va) 
        {
            m_va = nullptr;
            glBindVertexArray(0);
            return;
        }
        m_va = va;
        glBindVertexArray(m_va->getGLObject());
    }

    void GfxContext::setPrimitiveType(PrimitiveMode _type)
    {
        switch (_type)
        {
            case PrimitiveMode::TriangleList:
                m_primitiveType = GL_TRIANGLES;
                break;
            case PrimitiveMode::LineStrip:
                m_primitiveType = GL_LINE_STRIP;
                break;
            case PrimitiveMode::Line:
                m_primitiveType = GL_LINES;
                break;
            case PrimitiveMode::Points:
                m_primitiveType = GL_POINTS;
                break;
            default:
                break;
        }
    }

    void GfxContext::setViewport(Viewport viewport)
    {
        m_viewport = viewport;
        glViewport(viewport.m_x, viewport.m_y, viewport.m_width, viewport.m_height);
    }

    void GfxContext::setRenderTarget(RenderTarget* rt, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers)
    {
        if (!rt)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }
        GLenum* buffers = static_cast<GLenum*>(_alloca(sizeof(drawBuffers)));
        i32 numBuffers = drawBuffers.size();
        for (i32 i = 0; i < numBuffers; ++i)
        {
            const RenderTargetDrawBuffer& drawBuffer = *(drawBuffers.begin() + i);
            if (drawBuffer.binding > -1)
            {
                buffers[i] = GL_COLOR_ATTACHMENT0 + drawBuffer.binding;
            }
            else
            {
                buffers[i] = GL_NONE;
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
        glDrawBuffers(numBuffers, buffers);
    }

    void GfxContext::setClearColor(glm::vec4 color)
    {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    // TODO: Fix this
    void GfxContext::reset() 
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        setShader(nullptr);
    }

    void GfxContext::drawIndexAuto(u32 _numVerts, u32 _offset) 
    {
        glDrawArrays(m_primitiveType, _offset, _numVerts);
    }

    void GfxContext::drawIndex(u32 numIndices) 
    {
        glDrawElements(m_primitiveType, numIndices, GL_UNSIGNED_INT, reinterpret_cast<const void*>(0));
    }

    void GfxContext::clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void GfxContext::flip()
    {
        glfwSwapBuffers(m_glfwWindow);
    }
}