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

    static void setupSamplerAddresing(const SamplerAddressingMode& am, GLenum textureParameterName, GLuint glTextureObject) 
    {
        switch (am)
        {
        case SamplerAddressingMode::Wrap:
            glTextureParameteri(glTextureObject, textureParameterName, GL_REPEAT);
            break;
        case SamplerAddressingMode::Clamp:
            glTextureParameteri(glTextureObject, textureParameterName, GL_CLAMP_TO_EDGE);
            break;
        default:
            assert(0);
        }
    };

    static void setupSamplerFiltering(const SamplerFilteringMode& fm, GLenum textureParameterName, GLuint glTextureObject) 
    {
        switch (fm) {
        case SamplerFilteringMode::Point:
            glTextureParameteri(glTextureObject, textureParameterName, GL_NEAREST);
            break;
        case SamplerFilteringMode::PointMipmapPoint:
            glTextureParameteri(glTextureObject, textureParameterName, GL_NEAREST_MIPMAP_NEAREST);
            break;
        case SamplerFilteringMode::Bilinear:
            glTextureParameteri(glTextureObject, textureParameterName, GL_LINEAR);
            break;
        case SamplerFilteringMode::BilinearMipmapPoint:
            glTextureParameteri(glTextureObject, textureParameterName, GL_LINEAR_MIPMAP_NEAREST);
            break;
        case SamplerFilteringMode::Trilinear:
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
    static std::queue<i32> s_freeTextureUnits;
    static std::array<GLTexture*, kNumTextureUnits> s_textureBindings;

    static i32 allocTextureUnit()
    {
        static bool bInitialized = false;
        if (!bInitialized)
        {
            for (i32 unit = 0; unit < kNumTextureUnits; ++unit)
            {
                s_textureBindings[unit] = nullptr;
                s_freeTextureUnits.push(unit);
            }
        }

        if (!s_freeTextureUnits.empty())
        {
            i32 unit = s_freeTextureUnits.front();
            s_freeTextureUnits.pop();
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
        s_freeTextureUnits.push(textureUnit);
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

        // todo: this needs some improvements, ideally would like to specify how many mips to generate specifically
        if (desc.numMips > 1)
        {
            glGenerateTextureMipmap(glTextureObject);
        }
#else
        // allocate texture memory
        glTextureStorage2D(glTextureObject, desc.numMips, glPixelFormat.internalFormat, desc.width, desc.height);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc.width, desc.height, glPixelFormat.format, glPixelFormat.type, desc.data);
#endif
        glBindTexture(GL_TEXTURE_2D, 0);

        // setup default sampling parameters
        setupSamplerAddresing(defaultSampler.m_addressingX, GL_TEXTURE_WRAP_S, glTextureObject);
        setupSamplerAddresing(defaultSampler.m_addressingY, GL_TEXTURE_WRAP_T, glTextureObject);

        setupSamplerFiltering(defaultSampler.m_filteringMin, GL_TEXTURE_MIN_FILTER, glTextureObject);
        setupSamplerFiltering(defaultSampler.m_filteringMag, GL_TEXTURE_MAG_FILTER, glTextureObject);
    }

    // helper function to reduce code duplication
    void GLTexture::initTextureCube(GLuint glTextureObject, const GHTextureCube::Desc& desc, const GHSamplerCube& defaultSampler)
    {
        glBindTexture(GL_TEXTURE_CUBE_MAP, glTextureObject);
        auto glPixelFormat = translatePixelFormat(desc.pf);
        for (i32 f = 0; f < 6; ++f)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, glPixelFormat.internalFormat, desc.resolution, desc.resolution, 0, glPixelFormat.format, glPixelFormat.type, nullptr);
        }

        // todo: this needs some improvements, ideally would like to specify how many mips to generate specifically
        if (desc.numMips > 1)
        {
            glGenerateTextureMipmap(glTextureObject);
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        // setup default sampling parameters
        setupSamplerAddresing(defaultSampler.getAddressingModeX(), GL_TEXTURE_WRAP_S, glTextureObject);
        setupSamplerAddresing(defaultSampler.getAddressingModeY(), GL_TEXTURE_WRAP_T, glTextureObject);

        setupSamplerFiltering(defaultSampler.getFilteringModeMin(), GL_TEXTURE_MIN_FILTER, glTextureObject);
        setupSamplerFiltering(defaultSampler.getFilteringModeMag(), GL_TEXTURE_MAG_FILTER, glTextureObject);
    }

    GLTexture2D::GLTexture2D(const GHTexture2D::Desc& desc, const GHSampler2D& sampler2D)
        : GLTexture(), GHTexture2D(desc, sampler2D)
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

    void* GLTexture2D::getGHO()
    {
        return (void*)(m_name);
    }

    void GLTexture2D::getMipSize(i32& outWidth, i32& outHeight, i32 mipLevel)
    {
        glGetTextureLevelParameteriv(getName(), mipLevel, GL_TEXTURE_WIDTH, &outWidth);
        glGetTextureLevelParameteriv(getName(), mipLevel, GL_TEXTURE_HEIGHT, &outHeight);
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
        glGetTextureLevelParameteriv(getName(), mipLevel, GL_TEXTURE_WIDTH, &outWidth);
        glGetTextureLevelParameteriv(getName(), mipLevel, GL_TEXTURE_HEIGHT, &outHeight);
    }

    GLTextureCube::GLTextureCube(const GHTextureCube::Desc& desc, const GHSamplerCube& samplerCube)
        : GLTexture(), GHTextureCube(desc, samplerCube)
    {
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_name);
    }

    GLTextureCube::~GLTextureCube()
    {

    }

    void GLTextureCube::init()
    {
        GLTexture::initTextureCube(m_name, m_desc, m_defaultSampler);
    }

    void GLTextureCube::bind()
    {
        GLTexture::bind();
    }

    void GLTextureCube::unbind()
    {

    }

    void GLTextureCube::getMipSize(i32& outSize, i32 mipLevel)
    {
        glGetTextureLevelParameteriv(getName(), mipLevel, GL_TEXTURE_WIDTH, &outSize);
    }
}