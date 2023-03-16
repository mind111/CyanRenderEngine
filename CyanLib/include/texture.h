#pragma once

#include <cassert>
#include <string>
#include <memory>

#include <glew/glew.h>

#include "Common.h"
#include "CyanCore.h"
#include "Asset.h"
#include "Image.h"
#include "GfxTexture.h"

namespace Cyan
{
    struct Texture2DBase : public Asset, Image::Listener 
    {
        Texture2DBase(const char* inName, Image* inImage, const Sampler2D& inSampler2D);
        virtual ~Texture2DBase() { }

        /* Asset Interface */
        virtual void import() override;
        virtual void load() override;
        virtual void onLoaded() override;
        virtual void unload() override;

        /* Image::Listener Interface */
        virtual bool operator==(const Image::Listener& rhs) override
        {
            if (const Texture2DBase* texture = dynamic_cast<const Texture2DBase*>(&rhs))
            {
                return name == texture->name;
            }
            return false;
        }

        virtual void onImageLoaded() override;

        virtual void initGfxResource() = 0;
        virtual GfxTexture2D* getGfxResource() = 0;

        Image* srcImage = nullptr;
        Sampler2D sampler;
        i32 width = -1;
        i32 height = -1;
        i32 numMips = 1;
    };

    struct Texture2D : public Texture2DBase
    {
        Texture2D(const char* inName, Image* inImage, const Sampler2D& inSampler2D);
        virtual ~Texture2D() { }

        /* Texture2DBase interface */
        virtual void initGfxResource() override;
        virtual GfxTexture2D* getGfxResource() override { return gfxTexture.get(); };

        // gfx resource
        std::shared_ptr<GfxTexture2D> gfxTexture = nullptr;
    };
}
