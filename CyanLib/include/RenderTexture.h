#pragma once

#include <unordered_map>

#include "Texture.h"

namespace Cyan
{
    struct RenderTexture
    {
        RenderTexture() = default;
        RenderTexture(const char* inName);
        RenderTexture(const RenderTexture& src);
        RenderTexture& operator=(const RenderTexture& src);
        virtual ~RenderTexture();
        virtual void release() = 0;
    protected:
        std::string m_name;
        std::shared_ptr<u32> refCount = nullptr;
    };

    struct RenderTexture2D : public RenderTexture
    {
        RenderTexture2D() = default;

        RenderTexture2D(const char* inName, GfxTexture2D* srcGfxTexture, const Sampler2D& inSampler = {});
        RenderTexture2D(const char* inName, const GfxTexture2D::Spec& inSpec, const Sampler2D& inSampler = {});
        RenderTexture2D(const RenderTexture2D& src);
        RenderTexture2D& operator=(const RenderTexture2D& src);
        virtual ~RenderTexture2D();
        virtual void release() override;

        static u32 getNumAllocatedGfxTexture2D() { return cache.size(); }
        GfxTexture2D* getGfxTexture2D() const { return texture; }
    protected:
        GfxTexture2D* texture = nullptr;
    private:
        /** Note - @mind:
         * Using multi-map here because we want to allow duplicates, storing multiple textures having the same
         * spec. Do note that find() will return an iterator to any instance of key-value pair among all values
         * associated with that key, so the order of returned values are not guaranteed.
         */
        static std::unordered_multimap<GfxTexture2D::Spec, GfxTexture2D*> cache;
    };
     
    struct RenderDepthTexture2D : public RenderTexture
    {
        RenderDepthTexture2D() = default;
        RenderDepthTexture2D(const char* inName, const GfxDepthTexture2D::Spec& inSpec, const Sampler2D& inSampler = {});
        RenderDepthTexture2D(const RenderDepthTexture2D& src);
        RenderDepthTexture2D& operator=(const RenderDepthTexture2D& src);
        virtual ~RenderDepthTexture2D() override;
        virtual void release() override;

        static u32 getNumAllocatedGfxDepthTexture2D() { return cache.size(); }
        GfxDepthTexture2D* getGfxDepthTexture2D() const { return texture; }
    protected:
        GfxDepthTexture2D* texture = nullptr;
    private:
        /** Note - @mind:
         * Using multi-map here because we want to allow duplicates, storing multiple textures having the same
         * spec. Do note that find() will return an iterator to any instance of key-value pair among all values
         * associated with that key, so the order of returned values are not guaranteed.
         */
        static std::unordered_multimap<GfxDepthTexture2D::Spec, GfxDepthTexture2D*> cache;
    };
}
