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
    class Texture2DBase : public Asset, public Image::Listener 
    {
    public:
        Texture2DBase(const char* name, std::shared_ptr<Image> image, const Sampler2D& sampler);
        virtual ~Texture2DBase() { }

        static const char* getAssetTypeName() { return "Texture2DBase"; }

        /* Image::Listener Interface */
        virtual bool operator==(const Image::Listener& rhs) override
        {
            if (const Texture2DBase* texture = dynamic_cast<const Texture2DBase*>(&rhs))
            {
                return m_name == texture->m_name;
            }
            return false;
        }

        virtual void onImageLoaded() override;

        virtual GfxTexture2D* getGfxResource() = 0;

        std::shared_ptr<Image> m_srcImage = nullptr;
        Sampler2D m_sampler;
        i32 m_width = -1;
        i32 m_height = -1;
        i32 m_numMips = 1;
    };

    class Texture2D : public Texture2DBase
    {
    public:
        Texture2D(const char* name, std::shared_ptr<Image> image, const Sampler2D& sampler);
        virtual ~Texture2D() { }

        static const char* getAssetTypeName() { return "Texture2D"; }

        /* Texture2DBase interface */
        virtual GfxTexture2D* getGfxResource() override { return m_gfxTexture.get(); };
        virtual void onImageLoaded() override;

        void initGfxResource();

        // gfx resource
        std::shared_ptr<GfxTexture2D> m_gfxTexture = nullptr;
    };
}
