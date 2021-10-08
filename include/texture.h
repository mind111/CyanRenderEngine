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
            TEX_3D,
            TEX_CUBEMAP
        };

        enum Filter
        {
            LINEAR = 0,
            NEAREST,
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
            R32G32B32,
            R32G32B32A32,
            D24S8
        };

        enum DataType
        {
            UNSIGNED_BYTE,
            UNSIGNED_INT,
            UNSIGNED_INT_24_8,
            Float,
        };

        std::string m_name;
        void* m_data;
        ColorFormat m_format;
        DataType m_dataType;
        Filter m_minFilter;
        Filter m_magFilter;
        Wrap m_wrapS;
        Wrap m_wrapT;
        Wrap m_wrapR;
        Type m_type;
        u32 m_width;
        u32 m_height;
        u32 m_depth;
        GLuint m_id;
    };

    struct TextureSpec
    {
        Texture::Type m_type;
        u32 m_width, m_height, m_depth;
        u32 m_numMips;
        Texture::ColorFormat m_format;
        Texture::DataType m_dataType;
        Texture::Filter m_min;
        Texture::Filter m_mag;
        Texture::Wrap m_s;
        Texture::Wrap m_t;
        Texture::Wrap m_r;
        void*      m_data;
    };

    // TODO: implement following systems
    class TextureManager
    {
    public:
        TextureManager();
        ~TextureManager() { }
        static TextureManager* getSingletonPtr();
        u32 getNumTextures();
        Texture* getTexture(const char* name);
        void addTexture(Texture* texture);
        Texture* createTexture(const char* _name, TextureSpec spec);
        Texture* createTextureHDR(const char* _name, TextureSpec spec);
        Texture* createTexture(const char* _name, const char* _file, TextureSpec& spec);
        Texture* createTextureHDR(const char* _name, const char* _file, TextureSpec& spec);
        Texture* createTexture3D(const char* name, TextureSpec spec);

        static std::vector<Texture*> s_textures;
        static TextureManager* m_singleton;
    };
}