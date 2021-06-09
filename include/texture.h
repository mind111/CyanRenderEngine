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
            LINEAR = 0,
            MIPMAP_LINEAR,
        };

        enum Wrap
        {
            CLAMP_TO_EDGE = 0,
            NONE
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
        Filter m_minFilter;
        Filter m_magFilter;
        Wrap m_wrapS;
        Wrap m_wrapT;
        Wrap m_wrapR;
        Type m_type;
        u32 m_width;
        u32 m_height;
        GLuint m_id;
    };

    struct TextureSpec
    {
        Texture::Type m_type;
        Texture::ColorFormat m_format;
        Texture::Filter m_min;
        Texture::Filter m_mag;
        Texture::Wrap m_s;
        Texture::Wrap m_t;
        Texture::Wrap m_r;
    };
}

class TextureUtils
{
public:
    static unsigned char* loadImage(const char* filename, int* w, int* h, int* numChannels);
};