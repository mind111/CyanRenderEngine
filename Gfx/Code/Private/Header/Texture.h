#pragma once

#include "Core.h"

namespace Cyan
{
    /**
     * GHA is short for "Gfx Hardware Abstraction"
     */

    typedef u32 GLuint;

    struct GLGHA
    {

    };

    class GLTexture : public GHATexture
    {
    public:
        GLTexture();
        virtual ~GLTexture();

        virtual void bind() override { }
        virtual void unbind() override { }
    private:
        GLGHA m_gfxObject;
    };
}
