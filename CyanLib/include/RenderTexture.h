#pragma once

#include <unordered_map>

#include "Texture.h"

namespace Cyan
{
    struct RenderTexture2D
    {
        RenderTexture2D() = default;
        RenderTexture2D(const RenderTexture2D& src)
            : texture(src.texture), refCount(src.refCount)
        {
            (*refCount)++;
        }

        RenderTexture2D(const char* inName, const GfxTexture2D::Spec& spec, const Sampler2D& inSampler = {})
            : name(inName), refCount(nullptr), texture(nullptr)
        {
            refCount = new u32(1u);

            // search in the texture cache
            auto entry = cache.find(spec);
            if (entry == cache.end())
            {
                // create a new texture and add it to the cache
                texture = GfxTexture2D::create(spec, inSampler);
            }
            else
            {
                // recycle a texture and remove it from the cache
                texture = entry->second;
                cache.erase(entry);
            }
        }

        ~RenderTexture2D()
        {
            (*refCount)--;
            if (*refCount == 0u)
            {
                release();
                delete refCount;
            }
        }

        RenderTexture2D& operator=(const RenderTexture2D& src)
        {
            refCount = src.refCount;
            (*refCount)++;
            texture = src.texture;
            return *this;
        }

        static u32 getNumAllocatedGfxTexture2D() { return cache.size(); }

        GfxTexture2D* getGfxTexture2D() const
        {
            return texture;
        }

        std::string name;

    private:
        void release()
        {
            cache.insert({ texture->getSpec(), texture });
            texture = nullptr;
        }
        
        /** Note - @mind:
         * Using multi-map here because we want to allow duplicates, storing multiple textures having the same
         * spec. Do note that find() will return an iterator to any instance of key-value pair among all values
         * associated with that key, so the order of returned values are not guaranteed.
         */
        static std::unordered_multimap<GfxTexture2D::Spec, GfxTexture2D*> cache;

        u32* refCount = nullptr;
        GfxTexture2D* texture = nullptr;
    };
}
