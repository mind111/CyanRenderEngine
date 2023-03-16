#include <stbi/stb_image.h>

#include "Texture.h"
#include "CyanEngine.h"
#include "AssetManager.h"
#include "AssetImporter.h"
#include "GfxContext.h"

namespace Cyan
{
    Texture2DBase::Texture2DBase(const char* inName, Image* inImage, const Sampler2D& inSampler)
        : Asset(inName)
        , Image::Listener(inImage)
        , srcImage(inImage)
        , sampler(inSampler)
    {
    }

    void Texture2DBase::import()
    {
        srcImage->import();
    }

    void Texture2DBase::load()
    {

    }

    void Texture2DBase::onLoaded()
    {
        width = srcImage->width;
        height = srcImage->height;
        switch (sampler.minFilter)
        {
        case Sampler2D::Filtering::kTrilinear:
        case Sampler2D::Filtering::kMipmapPoint:
            numMips = max(1, std::log2(min(srcImage->width, srcImage->height)));
            break;
        default:
            break;
        }
        state = State::kLoaded;
    }

    void Texture2DBase::unload()
    {
    }

    void Texture2DBase::onImageLoaded()
    {
        onLoaded();

        if (isMainThread())
        {
            // if we are on main thread, then immediately initialize gfx resources
            initGfxResource();
        }
        else
        {
            AssetManager::deferredInitAsset(this, [this](Asset* asset) {
                initGfxResource();
            });
        }
    }

    Texture2D::Texture2D(const char* inName, Image* inImage, const Sampler2D& inSampler)
        : Texture2DBase(inName, inImage, inSampler)
    {
        if (srcImage->isLoaded())
        {
            onImageLoaded();
        }
    }

    void Texture2D::initGfxResource()
    {
        GfxTexture2D::Spec spec(*srcImage, (numMips > 1));
        assert(gfxTexture == nullptr);
        gfxTexture = std::shared_ptr<GfxTexture2D>(GfxTexture2D::create(srcImage->pixels.get(), spec, sampler));
        state = State::kInitialized;
    }
}
