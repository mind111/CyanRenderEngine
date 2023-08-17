#pragma once

#include "Core.h"
#include "Asset.h"
#include "GfxHardwareAbstraction/GHInterface/GHTexture.h"
#include "Image.h"

namespace Cyan
{
    class Texture : public Asset
    {
    public:
        virtual ~Texture();
    protected:
        Texture(const char* name);
    };

    class Texture2D : public Texture 
    {
    public:
        Texture2D(const char* name, std::unique_ptr<GHTexture2D> GHTexture);
        Texture2D(const char* name, const GHSampler2D& sampler2D, Image* image, bool bGenerateMipmap);
        virtual ~Texture2D();

        GHTexture2D* getGHTexture() { return m_GHTexture.get(); }

    private:
        Image* m_image = nullptr;
        std::unique_ptr<GHTexture2D> m_GHTexture = nullptr;
    };
}
