#include "GLTexture.h"

namespace Cyan
{
    struct GLTextureFormat
    {
        GLint internalFormat;
        GLenum format;
        GLenum type;
    };

    static GLTextureFormat translatePixelFormat(const PixelFormat& inFormat)
    {
        GLTextureFormat glTextureFormat = { };
        switch (inFormat)
        {
        case PixelFormat::kR8:
            glTextureFormat.internalFormat = GL_R8;
            glTextureFormat.format = GL_RED;
            glTextureFormat.type = GL_UNSIGNED_BYTE;
            break;
        case PixelFormat::kR16F:
            glTextureFormat.internalFormat = GL_R16F;
            glTextureFormat.format = GL_RED;
            glTextureFormat.type = GL_FLOAT;
            break;
        case PixelFormat::kR32F:
            glTextureFormat.internalFormat = GL_R32F;
            glTextureFormat.format = GL_RED;
            glTextureFormat.type = GL_FLOAT;
            break;
        case PixelFormat::kD24S8:
            glTextureFormat.internalFormat = GL_DEPTH24_STENCIL8;
            glTextureFormat.format = GL_DEPTH_STENCIL;
            glTextureFormat.type = GL_UNSIGNED_INT_24_8;
            break;
        case PixelFormat::kRGB8:
            glTextureFormat.internalFormat = GL_RGB8;
            glTextureFormat.format = GL_RGB;
            glTextureFormat.type = GL_UNSIGNED_BYTE;
            break;
        case PixelFormat::kRGBA8:
            glTextureFormat.internalFormat = GL_RGBA8;
            glTextureFormat.format = GL_RGBA;
            glTextureFormat.type = GL_UNSIGNED_BYTE;
            break;
        case PixelFormat::kRGB16F:
            glTextureFormat.internalFormat = GL_RGB16F;
            glTextureFormat.format = GL_RGB;
            // todo: should type be GL_FLOAT or GL_HALF_FLOAT, running into issue with GL_HALF_FLOAT so using GL_FLOAT for now
            glTextureFormat.type = GL_FLOAT;
            break;
        case PixelFormat::kRGBA16F:
            glTextureFormat.internalFormat = GL_RGBA16F;
            glTextureFormat.format = GL_RGBA;
            // todo: should type be GL_FLOAT or GL_HALF_FLOAT, running into issue with GL_HALF_FLOAT so using GL_FLOAT for now
            glTextureFormat.type = GL_FLOAT;
            break;
        case PixelFormat::kRGB32F:
            glTextureFormat.internalFormat = GL_RGB32F;
            glTextureFormat.format = GL_RGB;
            glTextureFormat.type = GL_FLOAT;
            break;
        case PixelFormat::kRGBA32F:
            glTextureFormat.internalFormat = GL_RGBA32F;
            glTextureFormat.format = GL_RGBA;
            glTextureFormat.type = GL_FLOAT;
            break;
        default:
            assert(0);
            break;
        }
        return glTextureFormat;
    }

    static void setupAddressing(const GHSampler2D::AddressingMode& am, GLenum textureParameterName, GLuint glTextureObject) 
    {
        switch (am)
        {
        case GHSampler2D::AddressingMode::Wrap:
            glTextureParameteri(glTextureObject, textureParameterName, GL_REPEAT);
            break;
        case GHSampler2D::AddressingMode::Clamp:
            glTextureParameteri(glTextureObject, textureParameterName, GL_CLAMP_TO_EDGE);
            break;
        default:
            assert(0);
        }
    };

    static void setupFiltering(const GHSampler2D::FilteringMode& fm, GLenum textureParameterName, GLuint glTextureObject) 
    {
        switch (fm) {
        case GHSampler2D::FilteringMode::Point:
            glTextureParameteri(glTextureObject, textureParameterName, GL_NEAREST);
            break;
        case GHSampler2D::FilteringMode::PointMipmapPoint:
            glTextureParameteri(glTextureObject, textureParameterName, GL_NEAREST_MIPMAP_NEAREST);
            break;
        case GHSampler2D::FilteringMode::Bilinear:
            glTextureParameteri(glTextureObject, textureParameterName, GL_LINEAR);
            break;
        case GHSampler2D::FilteringMode::BilinearMipmapPoint:
            glTextureParameteri(glTextureObject, textureParameterName, GL_LINEAR_MIPMAP_NEAREST);
            break;
        case GHSampler2D::FilteringMode::Trilinear:
            glTextureParameteri(glTextureObject, textureParameterName, GL_LINEAR_MIPMAP_LINEAR);
            break;
        default:
            assert(0);
        }
    };

    GLTexture::GLTexture()
        : GLObject()
    {
    }

    GLTexture::~GLTexture()
    {

    }

    static constexpr i32 kNumTextureUnits = 16;
    static std::queue<i32> s_freeUnits;
    static std::array<GLTexture*, kNumTextureUnits> s_textureBindings;

    static i32 allocTextureUnit()
    {
        static bool bInitialized = false;
        if (!bInitialized)
        {
            for (i32 unit = 0; unit < kNumTextureUnits; ++unit)
            {
                s_textureBindings[unit] = nullptr;
                s_freeUnits.push(unit);
            }
        }

        if (!s_freeUnits.empty())
        {
            i32 unit = s_freeUnits.front();
            s_freeUnits.pop();
            assert(s_textureBindings[unit] == nullptr);
            return unit;
        }
        else
        {
            assert(0); // unreachable
            return -1;
        }
    }

    static void releaseTextureUnit(i32 textureUnit)
    {
        auto boundTexture = s_textureBindings[textureUnit];
        assert(boundTexture != nullptr);
        boundTexture->unbind();
        s_textureBindings[textureUnit] = nullptr;
        s_freeUnits.push(textureUnit);
    }

    void GLTexture::bind()
    {
        i32 textureUnit = allocTextureUnit();
        if (textureUnit >= 0)
        {
            glBindTextureUnit(textureUnit, m_name);
            m_boundTextureUnit = textureUnit;
        }
    }

    void GLTexture::unbind()
    {
        glBindTextureUnit(m_boundTextureUnit, 0);
        releaseTextureUnit(m_boundTextureUnit);
        m_boundTextureUnit = INVALID_TEXTURE_UNIT;
    }

    bool GLTexture::isBound()
    {
        return (m_boundTextureUnit != INVALID_TEXTURE_UNIT);
    }

    // helper function to reduce code duplication
    void GLTexture::initTexture2D(GLuint glTextureObject, const GHTexture2D::Desc& desc, const GHSampler2D& defaultSampler)
    {
        glBindTexture(GL_TEXTURE_2D, glTextureObject);
        auto glPixelFormat = translatePixelFormat(desc.pf);
        // todo: verify input pixel data validity
#if 1 
        glTexImage2D(GL_TEXTURE_2D, 0, glPixelFormat.internalFormat, desc.width, desc.height, 0, glPixelFormat.format, glPixelFormat.type, desc.data);
#else
        // allocate texture memory
        glTextureStorage2D(glTextureObject, desc.numMips, glPixelFormat.internalFormat, desc.width, desc.height);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc.width, desc.height, glPixelFormat.format, glPixelFormat.type, desc.data);
#endif
        glBindTexture(GL_TEXTURE_2D, 0);

        // setup default sampling parameters
        setupAddressing(defaultSampler.m_addressingX, GL_TEXTURE_WRAP_S, glTextureObject);
        setupAddressing(defaultSampler.m_addressingY, GL_TEXTURE_WRAP_T, glTextureObject);

        setupFiltering(defaultSampler.m_filteringMin, GL_TEXTURE_MIN_FILTER, glTextureObject);
        setupFiltering(defaultSampler.m_filteringMag, GL_TEXTURE_MAG_FILTER, glTextureObject);
    }

    GLTexture2D::GLTexture2D(const GHTexture2D::Desc& desc)
        : GLTexture(), GHTexture2D(desc)
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &m_name);
    }

    GLTexture2D::~GLTexture2D()
    {
    }

    void GLTexture2D::init()
    {
        GLTexture::initTexture2D(m_name, m_desc, m_defaultSampler);
    }

    void GLTexture2D::bind()
    {
        GLTexture::bind();
    }

    void GLTexture2D::unbind()
    {
        GLTexture::unbind();
    }

    void GLTexture2D::getMipSize(i32& outWidth, i32& outHeight, i32 mipLevel)
    {
        glGetTexLevelParameteriv(GL_TEXTURE_2D, mipLevel, GL_TEXTURE_WIDTH, &outWidth);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, mipLevel, GL_TEXTURE_HEIGHT, &outHeight);
    }

    GLDepthTexture::GLDepthTexture(const GHDepthTexture::Desc& desc)
        : GLTexture(), GHDepthTexture(desc)
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &m_name);
    }

    GLDepthTexture::~GLDepthTexture()
    {

    }

    void GLDepthTexture::init()
    {
        GLTexture::initTexture2D(m_name, m_desc, m_defaultSampler);
    }

    void GLDepthTexture::bind()
    {
        GLTexture::bind();
    }

    void GLDepthTexture::unbind()
    {
        GLTexture::unbind();
    }

    void GLDepthTexture::getMipSize(i32& outWidth, i32& outHeight, i32 mipLevel)
    {
        NOT_IMPLEMENTED_ERROR()
    }
}