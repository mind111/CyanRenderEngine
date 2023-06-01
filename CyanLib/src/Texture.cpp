#include <algorithm>
#include <stbi/stb_image.h>

#include "Texture.h"
#include "GraphicsSystem.h"

namespace Cyan
{
    Texture2DBase::Texture2DBase(const char* name, std::shared_ptr<Image> image, const Sampler2D& sampler)
        : Asset(name)
        , Image::Listener(image.get())
        , m_srcImage(image)
        , m_sampler(sampler)
    {
    }

    void Texture2DBase::onImageLoaded()
    {
        m_width = m_srcImage->m_width;
        m_height = m_srcImage->m_height;
        switch (m_sampler.minFilter)
        {
        case Sampler2D::Filtering::kTrilinear:
        case Sampler2D::Filtering::kMipmapPoint:
            m_numMips = std::max(1u, (u32)std::log2(std::min(m_srcImage->m_width, m_srcImage->m_height)));
            break;
        default:
            break;
        }
        m_state = State::kLoaded;
    }

    Texture2D::Texture2D(const char* name, std::shared_ptr<Image> image, const Sampler2D& sampler)
        : Texture2DBase(name, image, sampler)
    {
        subscribe();
    }

    void Texture2D::onImageLoaded()
    {
        Texture2DBase::onImageLoaded();

        ENQUEUE_GFX_COMMAND(InitTexture2DCommand, [this]() {
            initGfxResource();
            })
    }

    void Texture2D::initGfxResource()
    {
        GfxTexture2D::Spec spec(*m_srcImage, (m_numMips > 1));
        assert(m_gfxTexture == nullptr);
        m_gfxTexture = std::shared_ptr<GfxTexture2D>(GfxTexture2D::create(m_srcImage->m_pixels.get(), spec, m_sampler));
        m_state = State::kInitialized;
    }
}
