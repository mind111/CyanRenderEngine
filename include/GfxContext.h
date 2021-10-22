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

    enum PrimitiveType
    {
        TriangleList = 0,
        Line,
        LineStrip
    };

    enum FrontFace
    {
        ClockWise = 0,
        CounterClockWise
    };

    enum FaceCull
    {
        Front = 0,
        Back
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


        //- getters
        RenderTarget* getRenderTarget();

        //-
        void setWindow(Window* _window);
        //-
        void setShader(Shader* _shader);
        void setUniform(Uniform* _uniform);
        void setSampler(Uniform* _sampler, u32 binding);
        void setTexture(Texture* _texture, u32 binding);
        void setVertexArray(VertexArray* _array);
        void setBuffer(RegularBuffer* _buffer, u32 binding);
        void setPrimitiveType(PrimitiveType _type);
        void setViewport(Viewport viewport);
        void setRenderTarget(RenderTarget* _rt, u16 drawBufferIdx); 
        void setRenderTarget(RenderTarget* rt, u32* drawBuffers, u32 numBuffers); 
        void setDepthControl(DepthControl _ctrl);
        void setClearColor(glm::vec4 color);
        void setCullFace(FrontFace frontFace, FaceCull faceToCull);
        //-
        void drawIndexAuto(u32 _numVerts, u32 _offset=0);
        void drawIndex(u32 numIndices);
        //-
        void flip();
        void clear();
        void reset();

        Window* m_window;
        Shader* m_shader;
        RenderTarget* m_currentRenderTarget;
        Viewport m_viewport;
        GLenum m_primitiveType;
        VertexArray* m_vertexArray;
        GLuint m_vao;
        GLuint m_fbo;
    };
}