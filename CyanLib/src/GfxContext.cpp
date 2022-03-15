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

    RenderTarget* GfxContext::getRenderTarget()
    {
        return m_currentRenderTarget;
    }

    void GfxContext::setWindow(Window* _window)
    {
        m_window = _window;
        m_viewport = { 0, 0, static_cast<u32>(_window->width), static_cast<u32>(_window->height) };
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

    void GfxContext::setVertexArray(VertexArray* array)
    {
        if (!array) {
            m_vertexArray = nullptr;
            this->m_vao = 0u; 
            glBindVertexArray(this->m_vao);
            return;
        }
        this->m_vao = array->m_vao; 
        glBindVertexArray(array->m_vao);
        m_vertexArray = array;
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
            case PrimitiveType::Line:
                m_primitiveType = GL_LINES;
                break;
            case PrimitiveType::Points:
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

    void GfxContext::setRenderTarget(RenderTarget* rt, const std::initializer_list<i32>& drawBuffers)
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
            i32 val = *(drawBuffers.begin() + i);
            if (val > -1)
            {
                buffers[i] = GL_COLOR_ATTACHMENT0 + val;
            }
            else
            {
                buffers[i] = GL_NONE;
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
        glDrawBuffers(numBuffers, buffers);
    }

/*
    void GfxContext::setRenderTarget(RenderTarget* rt, u16 drawBufferIdx) 
    { 
        if (!rt)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, rt->m_frameBuffer);
        rt->setDrawBuffer(drawBufferIdx);
    }

    void GfxContext::setRenderTarget(RenderTarget* rt, i32* drawBuffers, u32 numBuffers)
    {
        if (!rt)
            CYAN_ASSERT(0, "RenderTarget is null!");
        glBindFramebuffer(GL_FRAMEBUFFER, rt->m_frameBuffer);
        rt->setDrawBuffers(drawBuffers, numBuffers);
        rt->bindDrawBuffers();
    }

    void GfxContext::setRenderTarget(RenderTarget* renderTarget)
    {
        if (!renderTarget)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->m_frameBuffer);
        renderTarget->bindDrawBuffers();
    }
    */

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
        glfwSwapBuffers(m_window->mpWindow);
    }
}