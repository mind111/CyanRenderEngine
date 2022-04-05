#include "stb_image.h"
#include "Texture.h"

namespace Cyan
{
    std::vector<Texture*> TextureManager::s_textures;
    TextureManager* TextureManager::m_singleton = 0;

    static GLenum convertTexFilter(Texture::Filter filter)
    {
        switch(filter)
        {
            case Texture::Filter::LINEAR:
                return GL_LINEAR;
            case Texture::Filter::MIPMAP_LINEAR:
                return GL_LINEAR_MIPMAP_LINEAR;
            default:
                break;
        }
    }

    static GLenum convertTexWrap(Texture::Wrap wrap)
    {
        switch(wrap)
        {
            case Texture::Wrap::CLAMP_TO_EDGE:
                return GL_CLAMP_TO_EDGE;
            default:
                break;
        }
    }

    struct TextureSpecGL
    {
        GLenum m_typeGL;
        GLenum m_dataFormatGL;
        GLenum m_dataType;
        GLenum m_internalFormatGL;
        GLenum m_minGL;
        GLenum m_magGL;
        u8     m_wrapFlag;
        GLenum m_sGL;
        GLenum m_tGL;
        GLenum m_rGL;
    };

    static TextureSpecGL translate(TextureSpec& spec)
    {
        struct DataFormatGL
        {
            GLenum m_internalFormatGL;
            GLenum m_dataFormatGL;
        };

        auto convertTexType = [](Texture::Type type) {
            switch(type)
            {
                case Texture::Type::TEX_2D:
                    return GL_TEXTURE_2D;
                case Texture::Type::TEX_2D_ARRAY:
                    return GL_TEXTURE_2D_ARRAY;
                case Texture::Type::TEX_CUBEMAP:
                    return GL_TEXTURE_CUBE_MAP;
                case Texture::Type::TEX_3D:
                    return GL_TEXTURE_3D;
                default:
                    CYAN_ASSERT(0, "Undefined texture type.")
                    return GL_INVALID_ENUM;
            }
        };

        auto convertDataFormat = [](Texture::ColorFormat format) {
            switch (format)
            {
                case Texture::ColorFormat::R16F:
                    return DataFormatGL { GL_R16F, GL_RED };
                case Texture::ColorFormat::R32F:
                    return DataFormatGL { GL_R32F, GL_RED };
                case Texture::ColorFormat::R32G32F:
                    return DataFormatGL { GL_RG32F, GL_RG };
                case Texture::ColorFormat::Lum32F:
                    return DataFormatGL { GL_LUMINANCE32F_ARB, GL_LUMINANCE };
                case Texture::ColorFormat::R8G8B8: 
                    return DataFormatGL{ GL_RGB8, GL_RGB };
                case Texture::ColorFormat::R8G8B8A8: 
                    return DataFormatGL{ GL_RGBA8, GL_RGBA };
                case Texture::ColorFormat::R16G16B16: 
                    return DataFormatGL{ GL_RGB16F, GL_RGB };
                case Texture::ColorFormat::R16G16B16A16:
                    return DataFormatGL{ GL_RGBA16F, GL_RGBA };
                case Texture::ColorFormat::D24S8:
                    return DataFormatGL{ GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL }; 
                case Texture::ColorFormat::R32G32B32:
                    return DataFormatGL{ GL_RGB32F, GL_RGB }; 
                default:
                    CYAN_ASSERT(0, "Undefined texture color format.")
                    return DataFormatGL{ GL_INVALID_ENUM, GL_INVALID_ENUM };
            }
        };

       auto convertDataType = [](Texture::DataType dataType) {
           switch (dataType)
           {
               case Texture::DataType::UNSIGNED_BYTE:
                    return GL_UNSIGNED_BYTE;
               case Texture::DataType::UNSIGNED_INT:
                    return GL_UNSIGNED_INT;
               case Texture::DataType::UNSIGNED_INT_24_8:
                    return GL_UNSIGNED_INT_24_8;
               case Texture::DataType::Float:
                    return GL_FLOAT;
                default:
                    CYAN_ASSERT(0, "Undefined texture data type parameter.")
                    return GL_INVALID_ENUM;
           }
       };
         
       auto convertTexFilter = [](Texture::Filter filter) {
            switch(filter)
            {
                case Texture::Filter::LINEAR:
                    return GL_LINEAR;
                case Texture::Filter::MIPMAP_LINEAR:
                    return GL_LINEAR_MIPMAP_LINEAR;
                case Texture::Filter::NEAREST:
                    return GL_NEAREST;
                default:
                    CYAN_ASSERT(0, "Undefined texture filter parameter.")
                    return GL_INVALID_ENUM;
            }
        };

        auto convertTexWrap = [](Texture::Wrap wrap) {
            switch(wrap)
            {
                case Texture::Wrap::CLAMP_TO_EDGE:
                    return GL_CLAMP_TO_EDGE;
                case Texture::Wrap::NONE:
                    return 0;
                default:
                    CYAN_ASSERT(0, "Undefined texture wrap parameter.")
                    return GL_INVALID_ENUM;
            }
        };

        TextureSpecGL specGL = { };
        // type
        specGL.m_typeGL = convertTexType(spec.type);
        // format
        DataFormatGL format = convertDataFormat(spec.format);
        specGL.m_dataType = convertDataType(spec.dataType);
        specGL.m_dataFormatGL = format.m_dataFormatGL;
        specGL.m_internalFormatGL = format.m_internalFormatGL;
        // filters
        specGL.m_minGL = convertTexFilter(spec.min);
        specGL.m_magGL = convertTexFilter(spec.mag);
        // linear mipmap filtering
        if (spec.numMips > 1u)
        {
            specGL.m_minGL = convertTexFilter(Texture::Filter::MIPMAP_LINEAR);
        }
        // wraps
        auto wrapS = convertTexWrap(spec.s);
        auto wrapT = convertTexWrap(spec.t);
        auto wrapR = convertTexWrap(spec.r);
        if (wrapS > 0)
        {
            specGL.m_sGL = wrapS;
            specGL.m_wrapFlag |= (1 << 0);
        }
        if (wrapT > 0)
        {
            specGL.m_tGL = wrapT;
            specGL.m_wrapFlag |= (1 << 1);
        }
        if (wrapR > 0)
        {
            specGL.m_rGL = wrapR;
            specGL.m_wrapFlag |= (1 << 2);
        }
        return specGL;
    }

    // set texture parameters such as min/mag filters, wrap_s, wrap_t, and wrap_r
    static void setTextureParameters(Texture* texture, TextureSpecGL& specGL)
    {
        glBindTexture(specGL.m_typeGL, texture->handle);
        glTexParameteri(specGL.m_typeGL, GL_TEXTURE_MIN_FILTER, specGL.m_minGL);
        glTexParameteri(specGL.m_typeGL, GL_TEXTURE_MAG_FILTER, specGL.m_magGL);
        if (specGL.m_wrapFlag & (1 << 0 ))
        {
            glTexParameteri(specGL.m_typeGL, GL_TEXTURE_WRAP_S, specGL.m_sGL);
        }
        if (specGL.m_wrapFlag & (1 << 1))
        {
            glTexParameteri(specGL.m_typeGL, GL_TEXTURE_WRAP_T, specGL.m_tGL);
        }
        if (specGL.m_wrapFlag & (1 << 2))
        {
            glTexParameteri(specGL.m_typeGL, GL_TEXTURE_WRAP_R, specGL.m_rGL);
        }
        glBindTexture(specGL.m_typeGL, 0);
    }

    TextureManager* TextureManager::getSingletonPtr()
    {
        return m_singleton;
    }

    void TextureManager::addTexture(Texture* texture) {
        s_textures.push_back(texture);
    }

    u32 TextureManager::getNumTextures() {
        return static_cast<u32>(s_textures.size());
    }

    Texture* TextureManager::getTexture(const char* _name)
    {
        for (auto texture : s_textures)
        {
            if (strcmp(texture->name.c_str(), _name) == 0)
            {
                return texture;
            }
        }
        CYAN_ASSERT(0, "should not reach!") // Unreachable
        return 0;
    }
    
    Texture* TextureManager::createTexture(const char* _name, TextureSpec spec)
    {
        Texture* texture = new Texture();

        texture->name = _name;
        texture->width = spec.width;
        texture->height = spec.height;
        texture->type = spec.type;
        texture->format = spec.format;
        texture->dataType = spec.dataType;
        texture->minFilter = spec.min;
        texture->magFilter = spec.mag;
        texture->wrapS = spec.s;
        texture->wrapT = spec.t;
        texture->wrapR = spec.r;
        texture->data = spec.data;

        TextureSpecGL specGL = translate(spec);
        glCreateTextures(specGL.m_typeGL, 1, &texture->handle);
        glBindTexture(specGL.m_typeGL, texture->handle);
        glTexImage2D(specGL.m_typeGL, 0, specGL.m_internalFormatGL, texture->width, texture->height, 0, specGL.m_dataFormatGL, specGL.m_dataType, texture->data);
        glBindTexture(GL_TEXTURE_2D, 0);
        setTextureParameters(texture, specGL);
        if (spec.numMips > 1u)
        {
            glGenerateTextureMipmap(texture->handle);
        }
        s_textures.push_back(texture);
        return texture;
    }

    TextureManager::TextureManager()
    {
        if (!m_singleton)
        {
            m_singleton = this;
        }
        else
        {
            CYAN_ASSERT(0, "More than one instance of TextureManager was created!")
        }
    }

    // TODO: @refactor
    Texture* TextureManager::createTexture3D(const char* name, TextureSpec spec)
    {
        Texture* texture = new Texture();

        texture->name = name;
        texture->width = spec.width;
        texture->height = spec.height;
        texture->depth = spec.depth;
        texture->type = spec.type;
        texture->format = spec.format;
        texture->minFilter = spec.min;
        texture->magFilter = spec.mag;
        texture->wrapS = spec.s; 
        texture->wrapT = spec.t;
        texture->wrapR = spec.r;
        texture->data = spec.data;

        TextureSpecGL specGL = translate(spec);

        glCreateTextures(GL_TEXTURE_3D, 1, &texture->handle);
        glBindTexture(GL_TEXTURE_3D, texture->handle);
        glTexImage3D(GL_TEXTURE_3D, 0, specGL.m_internalFormatGL, spec.width, spec.height, spec.depth, 0, specGL.m_dataFormatGL, GL_UNSIGNED_INT, 0);
        if (spec.numMips > 1u)
        {
            glGenerateMipmap(GL_TEXTURE_3D);
        }
        glBindTexture(GL_TEXTURE_3D, 0u);
        setTextureParameters(texture, specGL);

        s_textures.push_back(texture);
        return texture;
    }

    Texture* TextureManager::createTextureHDR(const char* name, TextureSpec spec)
    {
        Texture* texture = new Texture();

        texture->name = name;
        texture->width = spec.width;
        texture->height = spec.height;
        texture->type = spec.type;
        texture->dataType = spec.dataType;
        texture->format = spec.format;
        texture->minFilter = spec.min;
        texture->magFilter = spec.mag;
        texture->wrapS = spec.s; 
        texture->wrapT = spec.t;
        texture->wrapR = spec.r;
        texture->data = spec.data;

        TextureSpecGL specGL = translate(spec);

        glCreateTextures(specGL.m_typeGL, 1, &texture->handle);
        glBindTexture(specGL.m_typeGL, texture->handle);
        switch(spec.type)
        {
            case Texture::Type::TEX_2D:
                glTexImage2D(GL_TEXTURE_2D, 0, specGL.m_internalFormatGL, texture->width, texture->height, 0, specGL.m_dataFormatGL, GL_FLOAT, texture->data);
                break;
            case Texture::Type::TEX_CUBEMAP:
                for (int f = 0; f < 6; f++)
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, specGL.m_internalFormatGL, texture->width, texture->height, 0, specGL.m_dataFormatGL, GL_FLOAT, nullptr);
                break;
            default:
                break;
        }
        glBindTexture(specGL.m_typeGL, 0);
        setTextureParameters(texture, specGL);
        if (spec.numMips > 1u)
        {
            glGenerateTextureMipmap(texture->handle);
        }
        s_textures.push_back(texture);
        return texture;
    }

    // when loading a texture from file, TextureSpec.m_format will be modified as 
    // the format is detmermined after loading the image, so @spec needs to be taken
    // as a reference
    Texture* TextureManager::createTexture(const char* name, const char* filePath, TextureSpec& spec)
    {
        int w, h, numChannels;
        Texture* texture = new Texture();
        stbi_set_flip_vertically_on_load(1);
        unsigned char* pixels = stbi_load(filePath, &w, &h, &numChannels, 0);

        spec.format = numChannels == 3 ? Texture::ColorFormat::R8G8B8 : Texture::ColorFormat::R8G8B8A8;
        texture->name = name;
        texture->width = w;
        texture->height = h;
        texture->type = spec.type;
        texture->dataType = spec.dataType;
        texture->format = spec.format;
        texture->minFilter = spec.min;
        texture->magFilter = spec.mag;
        texture->wrapS = spec.s; 
        texture->wrapT = spec.t;
        texture->wrapR = spec.r;
        texture->data = pixels;

        TextureSpecGL specGL = translate(spec);

        glCreateTextures(specGL.m_typeGL, 1, &texture->handle);
        glTextureStorage2D(texture->handle, spec.numMips, specGL.m_internalFormatGL, texture->width, texture->height);
        glTextureSubImage2D(texture->handle, 0, 0, 0, texture->width, texture->height, specGL.m_dataFormatGL, GL_UNSIGNED_BYTE, texture->data);
        if (spec.numMips > 1u)
        {
            glGenerateTextureMipmap(texture->handle);
        }
        s_textures.push_back(texture);
        stbi_image_free(pixels);
        return texture;
    }

    // when loading a texture from file, TextureSpec.m_format will be modified as 
    // the format is detmermined after loading the image, so @spec needs to be taken
    // as a reference
    Texture* TextureManager::createTextureHDR(const char* name, const char* filePath, TextureSpec& spec)
    {
        int w, h, numChannels;
        Texture* texture = new Texture();
        stbi_set_flip_vertically_on_load(1);
        float* pixels = stbi_loadf(filePath, &w, &h, &numChannels, 0);

        spec.format = numChannels == 3 ? Texture::ColorFormat::R16G16B16 : Texture::ColorFormat::R16G16B16A16;
        texture->name = name;
        texture->width = w;
        texture->height = h;
        texture->type = spec.type;
        texture->dataType = spec.dataType;
        texture->format = spec.format;
        texture->minFilter = spec.min;
        texture->magFilter = spec.mag;
        texture->wrapS = spec.s; 
        texture->wrapT = spec.t;
        texture->wrapR = spec.r;
        texture->data = pixels;

        TextureSpecGL specGL = translate(spec);

        glCreateTextures(specGL.m_typeGL, 1, &texture->handle);
        glTextureStorage2D(texture->handle, spec.numMips, specGL.m_internalFormatGL, texture->width, texture->height);
        glTextureSubImage2D(texture->handle, 0, 0, 0, texture->width, texture->height, specGL.m_dataFormatGL, GL_FLOAT, texture->data);
        setTextureParameters(texture, specGL);
        if (spec.numMips > 1u)
        {
            glGenerateTextureMipmap(texture->handle);
        }
        s_textures.push_back(texture);
        stbi_image_free(pixels);
        return texture;
    }

    Texture* TextureManager::createArrayTexture2D(const char* name, TextureSpec& spec)
    {
        Texture* texture = new Texture();

        texture->name = name;
        texture->width = spec.width;
        texture->height = spec.height;
        texture->depth = spec.depth;
        texture->type = spec.type;
        texture->dataType = spec.dataType;
        texture->format = spec.format;
        texture->minFilter = spec.min;
        texture->magFilter = spec.mag;
        texture->wrapS = spec.s; 
        texture->wrapT = spec.t;
        texture->wrapR = spec.r;
        texture->data = spec.data;

        TextureSpecGL specGL = translate(spec);
        // create texture arrays
        {
            glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &texture->handle);
            glBindTexture(GL_TEXTURE_2D_ARRAY, texture->handle);
            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, specGL.m_internalFormatGL, texture->width, texture->height, texture->depth);
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        }
        setTextureParameters(texture, specGL);
        if (spec.numMips > 1u)
        {
            glGenerateTextureMipmap(texture->handle);
        }
        s_textures.push_back(texture);
        return texture;
    }

    Texture* TextureManager::createDepthTexture(const char* name, u32 width, u32 height)
    {
        TextureSpec spec = { };
        spec.width = width;
        spec.height =height;
        spec.format = Texture::ColorFormat::D24S8; // 32 bits
        spec.type = Texture::Type::TEX_2D;
        spec.dataType = Texture::DataType::UNSIGNED_INT_24_8;
        spec.min = Texture::Filter::NEAREST;
        spec.mag = Texture::Filter::NEAREST;
        spec.r = Texture::Wrap::CLAMP_TO_EDGE;
        spec.s = Texture::Wrap::CLAMP_TO_EDGE;
        return createTexture(name, spec);
    }

    void copyTexture(Texture* dst, Texture* src)
    {

    }
}
