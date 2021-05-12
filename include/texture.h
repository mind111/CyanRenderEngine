#pragma once

#include <string>

#include "glew.h"

#include "Common.h"

namespace Cyan
{
    // TODO: miplevels?
    struct Texture
    {
        enum Type
        {
            TEX_2D = 0,
            TEX_CUBEMAP
        };

        enum Filter
        {
            LINEAR = 0
        };
        
        enum ColorFormat
        {
            R8G8B8 = 0,
            R16G16B16,
            R8G8B8A8,
            R16G16B16A16,
        };

        std::string m_name;
        void* m_data;

        ColorFormat m_format;
        Filter m_filter;
        Type m_type;

        u32 m_width;
        u32 m_height;
        GLuint m_id;
    };
}

class TextureUtils
{
public:
    static unsigned char* loadImage(const char* filename, int* w, int* h, int* numChannels);
};