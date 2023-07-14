#pragma once

#include "glew.h"

#define GL_INVALID_NAME -1

namespace Cyan
{
    class GLObject
    {
    public:
        GLuint getName() { return m_name; }
    protected:
        GLObject() : m_name(GL_INVALID_NAME) { }
        GLuint m_name = GL_INVALID_NAME;
    };
}
