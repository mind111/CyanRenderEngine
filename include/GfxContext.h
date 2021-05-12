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
    enum PrimitiveType
    {
        TriangleList = 0,
        LineStrip
    };

    enum DepthControl
    {
        kDisable = 0,
        kEnable = 1
    };

    class GfxContext
    {
    public:
        GfxContext() { }
        ~GfxContext() { }

        void init();

        //-
        void setWindow(Window* _window);
        //-
        void setShader(Shader* _shader);
        void setUniform(Uniform* _uniform);
        void setSampler(Uniform* _sampler, u32 binding);
        void setTexture(Texture* _texture, u32 binding);
        void setVertexArray(VertexArray* _array);
        void setBuffer(RegularBuffer* _buffer);
        void setPrimitiveType(PrimitiveType _type);
        void setViewport(u32 _left, u32 _bottom, u32 _right, u32 _top);
        void setRenderTarget(RenderTarget* _rt, u16 _colorBufferFlag); 
        void setDepthControl(DepthControl _ctrl);
        void setClearColor(glm::vec4 color);
        //-
        void drawIndex(u32 _numVerts, u32 _offset=0);
        //-
        void flip();
        void clear();
        void reset();

        Window* m_window;
        Shader* m_shader;
        glm::vec4 m_viewport;
        GLenum m_primitiveType;
        GLuint m_vao;
        GLuint m_fbo;
    };
}