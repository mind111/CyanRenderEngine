#include "GfxContext.h"
#include "CyanRenderer.h"
#include "CyanAPI.h"

namespace Cyan
{
    /* Global graphics context */
    static GfxContext* s_gfxc = nullptr;

    void GfxContext::init()
    {
        m_window = nullptr;
        m_shader = nullptr;
        m_primitiveType = -1;
        m_vao = 0;
        m_fbo = 0;
    }

    void GfxContext::setWindow(Window* _window)
    {
        m_window = _window;
        m_viewport = glm::vec4(0.f, 0.f, _window->width, _window->height);
    }

    void GfxContext::setShader(Shader* _shader)
    {
        if (!_shader)
        {
            m_shader->unbind();
            m_shader = nullptr;
        }
        else
        {
            if ((!m_shader) || 
            (m_shader && (_shader->m_programId != m_shader->m_programId)))
            {
                m_shader = _shader;
                m_shader->bind();
            }
        }
    }

    void GfxContext::setUniform(Uniform* _uniform)
    {
            m_shader->setUniform(_uniform);
    }

    void GfxContext::setSampler(Uniform* _sampler, u32 binding)
    {
        m_shader->setUniform(_sampler, (i32)binding);
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
            glBindTextureUnit(binding, _texture->m_id);
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

    void GfxContext::setVertexArray(VertexArray* _array)
    {
        this->m_vao = _array->m_vao; 
        glBindVertexArray(_array->m_vao);
    }

    void GfxContext::setPrimitiveType(PrimitiveType _type)
    {
        switch (_type)
        {
            case PrimitiveType::TriangleList:
                m_primitiveType = GL_TRIANGLES;
                break;
            case PrimitiveType::LineStrip:
                m_primitiveType = GL_LINE_STRIP;
                break;
            default:
                break;
        }
    }

    void GfxContext::setViewport(u32 _left, u32 _bottom, u32 _right, u32 _top)
    {
        glViewport(_left, _bottom, _right, _top);
        m_viewport.x = _left;
        m_viewport.y = _bottom;
        m_viewport.z = _right;
        m_viewport.w = _top;
    }

    void GfxContext::setRenderTarget(RenderTarget* _rt, u16 drawBufferIdx) 
    { 
        glBindFramebuffer(GL_FRAMEBUFFER, _rt->m_frameBuffer);
        _rt->setDrawBuffer(drawBufferIdx);
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

    void GfxContext::drawIndex(u32 _numVerts, u32 _offset) 
    {
        glDrawArrays(m_primitiveType, _offset, _numVerts);
    }

    void GfxContext::clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void GfxContext::flip()
    {
        glfwSwapBuffers(m_window->mpWindow);
    }
}