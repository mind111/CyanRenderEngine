#pragma once

#include <string>

#include "glew.h"

#include "Common.h"
#include "CyanCore.h"

namespace Cyan
{
    // TODO: how to handle texture name?
    struct Texture : public GpuResource
    {
        enum Type
        {
            TEX_2D = 0,
            TEX_2D_ARRAY,
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
            R16F = 0,
            R32F,
            R32G32F,
            Lum32F,
            R8G8B8,
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

        std::string name;
        void* data;
        ColorFormat format;
        DataType dataType;
        Filter minFilter;
        Filter magFilter;
        Wrap wrapS;
        Wrap wrapT;
        Wrap wrapR;
        Type type;
        u32 width;
        u32 height;
        u32 depth;
        GLuint handle;

        u32 getSizeInBytes()
        {
            u32 bpp = 0u;
            switch(format)
            {
                case R16G16B16:
                    break;
                case R8G8B8:
                    break;
                default:
                    break;
            }
        }
    };

    struct TextureSpec
    {
        Texture::Type type;
        u32 width, height, depth;
        u32 numMips;
        Texture::ColorFormat format;
        Texture::DataType dataType;
        Texture::Filter min;
        Texture::Filter mag;
        Texture::Wrap s;
        Texture::Wrap t;
        Texture::Wrap r;
        void*      data;
    };

    class TextureManager
    {
    public:
        TextureManager();
        ~TextureManager() { }
        static TextureManager* get();
        u32 getNumTextures();
        Texture* getTexture(const char* name);
        void addTexture(Texture* texture);
        Texture* createTexture(const char* name, TextureSpec spec);
        Texture* createTextureHDR(const char* name, TextureSpec spec);
        Texture* createTexture(const char* name, const char* file, TextureSpec& spec);
        Texture* createTextureHDR(const char* name, const char* file, TextureSpec& spec);
        Texture* createTexture3D(const char* name, TextureSpec spec);
        Texture* createArrayTexture2D(const char* name, TextureSpec& spec);
        Texture* createDepthTexture(const char* name, u32 width, u32 height);
        void copyTexture(Texture* dst, Texture* src);

        static std::vector<Texture*> s_textures;
        static TextureManager* m_singleton;
    };
}