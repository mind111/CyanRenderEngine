#include "RenderTexture.h"

namespace Cyan
{
    std::unordered_multimap<GfxTexture2D::Spec, GfxTexture2D*> RenderTexture2D::cache;
    std::unordered_multimap<GfxDepthTexture2D::Spec, GfxDepthTexture2D*> RenderDepthTexture2D::cache;

    RenderTexture::RenderTexture(const char* inName)
        : name(inName), refCount(nullptr)
    {
        refCount = std::make_shared<u32>(1);
    }

    RenderTexture::RenderTexture(const RenderTexture& src)
    {
        name = src.name;
        refCount = src.refCount;
        (*refCount)++;
    }

    RenderTexture& RenderTexture::operator=(const RenderTexture& src)
    {
        if (refCount != nullptr)
        {
            (*refCount)--;
        }
        name = src.name;
        refCount = src.refCount;
        (*refCount)++;
        return *this;
    }

    RenderTexture::~RenderTexture()
    {
        (*refCount)--;
    }

    RenderTexture2D::RenderTexture2D(const char* inName, const GfxTexture2D::Spec& inSpec, const Sampler2D& inSampler)
        : RenderTexture(inName), texture(nullptr)
    {
        // search in the texture cache
        auto entry = cache.find(inSpec);
        if (entry == cache.end())
        {
            // create a new texture and add it to the cache
            texture = GfxTexture2D::create(inSpec, inSampler);
        }
        else
        {
            // recycle a texture and remove it from the cache
            texture = entry->second;
            cache.erase(entry);
        }
    }

    RenderTexture2D::RenderTexture2D(const RenderTexture2D& src)
        : RenderTexture(src), texture(src.texture)
    { }

    RenderTexture2D& RenderTexture2D::operator=(const RenderTexture2D& src)
    {
        RenderTexture::operator=(src);
        texture = src.texture;
        return *this;
    }

    /** 
        * It may seem weird that release() is called when refCount is still 1, this is because RenderTexture::~RenderTexture() is called 
        after RenderTexture2D::~RenderTexture2D() gets called, thus when refCount is 1 here, it means the refCount is about
        to go to 0, and resource can be released here.
    */
    RenderTexture2D::~RenderTexture2D()
    {
        if (*refCount == 1u)
        {
            release();
        }
    }

    void RenderTexture2D::release()
    {
        cache.insert({ texture->getSpec(), texture });
        texture = nullptr;
    }

    RenderDepthTexture2D::RenderDepthTexture2D(const char* inName, const GfxDepthTexture2D::Spec& inSpec, const Sampler2D& inSampler)
        : RenderTexture(inName)
    {
        // search in the texture cache
        auto entry = cache.find(inSpec);
        if (entry == cache.end())
        {
            // create a new texture and add it to the cache
            texture = GfxDepthTexture2D::create(inSpec, inSampler);
        }
        else
        {
            // recycle a texture and remove it from the cache
            texture = entry->second;
            cache.erase(entry);
        }
    }

    RenderDepthTexture2D::RenderDepthTexture2D(const RenderDepthTexture2D& src)
        : RenderTexture(src), texture(src.texture)
    {
    }

    RenderDepthTexture2D& RenderDepthTexture2D::operator=(const RenderDepthTexture2D& src)
    {
        RenderTexture::operator=(src);
        texture = src.texture;
        return *this;
    }

    RenderDepthTexture2D::~RenderDepthTexture2D()
    { 
        if (*refCount == 1u)
        {
            release();
        }
    }

    void RenderDepthTexture2D::release()
    {
        cache.insert({ texture->getSpec(), static_cast<GfxDepthTexture2D*>(texture) });
        texture = nullptr;
    }
}
