#include <stack>

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
        case PixelFormat::kR32I:
            glTextureFormat.internalFormat = GL_R32I;
            glTextureFormat.format = GL_RED_INTEGER;
            glTextureFormat.type = GL_INT;
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

    static void setupSampler2DFiltering(const Sampler2DFilteringMode& fm, GLenum textureParameterName, GLuint glTextureObject) 
    {
        switch (fm) {
        case Sampler2DFilteringMode::Point:
            glTextureParameteri(glTextureObject, textureParameterName, GL_NEAREST);
            break;
        case Sampler2DFilteringMode::PointMipmapPoint:
            glTextureParameteri(glTextureObject, textureParameterName, GL_NEAREST_MIPMAP_NEAREST);
            break;
        case Sampler2DFilteringMode::Bilinear:
            glTextureParameteri(glTextureObject, textureParameterName, GL_LINEAR);
            break;
        case Sampler2DFilteringMode::BilinearMipmapPoint:
            glTextureParameteri(glTextureObject, textureParameterName, GL_LINEAR_MIPMAP_NEAREST);
            break;
        case Sampler2DFilteringMode::Trilinear:
            glTextureParameteri(glTextureObject, textureParameterName, GL_LINEAR_MIPMAP_LINEAR);
            break;
        default:
            assert(0);
        }
    };

    static void setupSampler3DFiltering(const Sampler3DFilteringMode& fm, GLenum textureParameterName, GLuint glTextureObject) 
    {
        switch (fm) {
        case Sampler3DFilteringMode::Point:
            glTextureParameteri(glTextureObject, textureParameterName, GL_NEAREST);
            break;
        case Sampler3DFilteringMode::PointMipmapPoint:
            glTextureParameteri(glTextureObject, textureParameterName, GL_NEAREST_MIPMAP_NEAREST);
            break;
        case Sampler3DFilteringMode::Trilinear:
            glTextureParameteri(glTextureObject, textureParameterName, GL_LINEAR);
            break;
        case Sampler3DFilteringMode::TrilinearMipmapPoint:
            glTextureParameteri(glTextureObject, textureParameterName, GL_LINEAR_MIPMAP_NEAREST);
            break;
        case Sampler3DFilteringMode::Quadrilinear:
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

    template <typename T, u32 kNumBindingUnits>
    class BindingManager
    {
    public:
        BindingManager()
        {
            for (i32 unit = kNumBindingUnits - 1; unit >= 0; --unit)
            {
                m_bindings[unit] = nullptr;
                m_freeBindings.push(unit);
            }
        }

        ~BindingManager()
        {
        }

        i32 alloc(T* toBind)
        {
            if (!m_freeBindings.empty())
            {
                i32 unit = m_freeBindings.top();
                m_freeBindings.pop();
                assert(m_bindings[unit] == nullptr);
                m_bindings[unit] = toBind;
                return unit;
            }
            else
            {
                assert(0); // unreachable
                return -1;
            }
        }

        void release(u32 bindingUnit)
        {
            m_bindings[bindingUnit] = nullptr;
            m_freeBindings.push(bindingUnit);
        }

    private:
        std::array<T*, kNumBindingUnits> m_bindings;
        std::stack<u32> m_freeBindings;
    };

    static BindingManager<GLTexture, 16> s_textureBindingManager;

    void GLTexture::bindAsTexture()
    {
        assert(!isBoundAsImage());
        if (!isBoundAsTexture())
        {
            i32 textureUnit = s_textureBindingManager.alloc(this);
            if (textureUnit >= 0)
            {
                glBindTextureUnit(textureUnit, m_name);
                m_boundTextureUnit = textureUnit;
            }
        }
    }

    void GLTexture::unbindAsTexture()
    {
        glBindTextureUnit(m_boundTextureUnit, 0);
        s_textureBindingManager.release(m_boundTextureUnit);
        m_boundTextureUnit = INVALID_TEXTURE_UNIT;
    }

    static BindingManager<GLTexture, 8> s_imageBindingManager;
    
    void GLTexture::bindAsImage(u32 mipLevel, const PixelFormat& pf)
    {
        assert(!isBoundAsTexture());
        if (!isBoundAsImage())
        {
            i32 imageUnit = s_imageBindingManager.alloc(this);
            if (imageUnit >= 0)
            {
                const auto& glPixelFormat = translatePixelFormat(pf);
                glBindImageTexture((GLuint)imageUnit, getName(), mipLevel, GL_FALSE, 0, GL_READ_WRITE, glPixelFormat.internalFormat);
                m_boundImageUnit = imageUnit;
            }
        }
    }

    void GLTexture::unbindAsImage()
    {
        assert(isBoundAsImage());
        // since we are unbinding here, so passing a ad-hoc pixel format
        glBindImageTexture(m_boundImageUnit, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
        s_imageBindingManager.release(m_boundImageUnit);
        m_boundImageUnit = INVALID_IMAGE_UNIT;
    }

    bool GLTexture::isBoundAsTexture()
    {
        return (m_boundTextureUnit != INVALID_TEXTURE_UNIT);
    }

    bool GLTexture::isBoundAsImage()
    {
        return (m_boundImageUnit != INVALID_IMAGE_UNIT);
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

        setupSampler2DFiltering(defaultSampler.m_filteringMin, GL_TEXTURE_MIN_FILTER, glTextureObject);
        setupSampler2DFiltering(defaultSampler.m_filteringMag, GL_TEXTURE_MAG_FILTER, glTextureObject);
    }

    void GLTexture::initTexture3D(GLuint glTextureObject, const GHTexture3D::Desc& desc, const GHSampler3D& defaultSampler)
    {
        glBindTexture(GL_TEXTURE_3D, glTextureObject);
        auto glPixelFormat = translatePixelFormat(desc.pf);
        // todo: verify input pixel data validity
#if 1 
        glTexImage3D(GL_TEXTURE_3D, 0, glPixelFormat.internalFormat, desc.width, desc.height, desc.depth, 0, glPixelFormat.format, glPixelFormat.type, desc.data);

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
        glBindTexture(GL_TEXTURE_3D, 0);

        // setup default sampling parameters
        setupSamplerAddresing(defaultSampler.getAddressingModeX(), GL_TEXTURE_WRAP_S, glTextureObject);
        setupSamplerAddresing(defaultSampler.getAddressingModeY(), GL_TEXTURE_WRAP_T, glTextureObject);
        setupSamplerAddresing(defaultSampler.getAddressingModeZ(), GL_TEXTURE_WRAP_R, glTextureObject);

        setupSampler3DFiltering(defaultSampler.getFilteringModeMin(), GL_TEXTURE_MIN_FILTER, glTextureObject);
        setupSampler3DFiltering(defaultSampler.getFilteringModeMag(), GL_TEXTURE_MAG_FILTER, glTextureObject);
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

        setupSampler2DFiltering(defaultSampler.getFilteringModeMin(), GL_TEXTURE_MIN_FILTER, glTextureObject);
        setupSampler2DFiltering(defaultSampler.getFilteringModeMag(), GL_TEXTURE_MAG_FILTER, glTextureObject);
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

    void GLTexture2D::bindAsTexture()
    {
        GLTexture::bindAsTexture();
    }

    void GLTexture2D::bindAsRWTexture(u32 mipLevel)
    {
        GLTexture::bindAsImage(mipLevel, getDesc().pf);
    }

    void GLTexture2D::unbindAsRWTexture()
    {
        GLTexture::unbindAsImage();
    }

    void GLTexture2D::unbindAsTexture()
    {
        GLTexture::unbindAsTexture();
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

    void GLDepthTexture::bindAsTexture()
    {
        GLTexture::bindAsTexture();
    }

    void GLDepthTexture::bindAsRWTexture(u32 mipLevel)
    {
        GLTexture::bindAsImage(mipLevel, getDesc().pf);
    }

    void GLDepthTexture::unbindAsRWTexture()
    {
        GLTexture::unbindAsImage();
    }

    void GLDepthTexture::unbindAsTexture()
    {
        GLTexture::unbindAsTexture();
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

    void GLTextureCube::bindAsTexture()
    {
        GLTexture::bindAsTexture();
    }

    void GLTextureCube::bindAsRWTexture(u32 mipLevel)
    {
        GLTexture::bindAsImage(mipLevel, getDesc().pf);
    }

    void GLTextureCube::unbindAsRWTexture()
    {
        GLTexture::unbindAsImage();
    }

    void GLTextureCube::unbindAsTexture()
    {
        GLTexture::unbindAsTexture();
    }

    void GLTextureCube::getMipSize(i32& outSize, i32 mipLevel)
    {
        glGetTextureLevelParameteriv(getName(), mipLevel, GL_TEXTURE_WIDTH, &outSize);
    }

    GLTexture3D::GLTexture3D(const GHTexture3D::Desc& desc, const GHSampler3D& sampler3D)
        : GLTexture()
        , GHTexture3D(desc, sampler3D)
    {
        glCreateTextures(GL_TEXTURE_3D, 1, &m_name);
    }

    GLTexture3D::~GLTexture3D()
    {

    }

    void GLTexture3D::init()
    {
        GLTexture::initTexture3D(m_name, m_desc, m_defaultSampler);
    }

    void GLTexture3D::bindAsTexture()
    {
        GLTexture::bindAsTexture();
    }

    void GLTexture3D::bindAsRWTexture(u32 mipLevel)
    {
        GLTexture::bindAsImage(mipLevel, getDesc().pf);
    }

    void GLTexture3D::unbindAsRWTexture()
    {
        GLTexture::unbindAsImage();
    }

    void GLTexture3D::unbindAsTexture()
    {
        GLTexture::unbindAsTexture();
    }

    void GLTexture3D::getMipSize(i32& outWidth, i32& outHeight, i32& outDepth, i32 mipLevel)
    {
        glGetTextureLevelParameteriv(getName(), mipLevel, GL_TEXTURE_WIDTH, &outWidth);
        glGetTextureLevelParameteriv(getName(), mipLevel, GL_TEXTURE_HEIGHT, &outHeight);
        glGetTextureLevelParameteriv(getName(), mipLevel, GL_TEXTURE_DEPTH, &outDepth);
    }
}