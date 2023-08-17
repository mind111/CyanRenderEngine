#pragma once

#include "Core.h"
#include "GfxHardwareAbstraction/GHInterface/GHTexture.h"

namespace Cyan
{
    class RenderTexture
    {
    public:
        RenderTexture() = default;
        RenderTexture(const char* name);
        RenderTexture(const RenderTexture& src);
        RenderTexture& operator=(const RenderTexture& src);
        virtual ~RenderTexture();
        virtual void release() = 0;
    protected:
        std::string m_name;
        std::shared_ptr<u32> m_refCount = nullptr;
    };

    class RenderTexture2D : public RenderTexture
    {
    public:
        RenderTexture2D() = default;

        RenderTexture2D(const char* name, GHTexture2D* srcGHTexture2D, const GHSampler2D& sampler = { });
        RenderTexture2D(const char* name, const GHTexture2D::Desc& desc, const GHSampler2D& sampler = { });
        RenderTexture2D(const RenderTexture2D& src);
        RenderTexture2D& operator=(const RenderTexture2D& src);
        virtual ~RenderTexture2D();
        virtual void release() override;

        static u32 getNumAllocatedGfxTexture2D() { return static_cast<u32>(s_cache.size()); }
        GHTexture2D* getGHTexture2D() const { return m_GHTexture2D.get(); }
    protected:
        std::shared_ptr<GHTexture2D> m_GHTexture2D = nullptr;
    private:
        /** Note - @mind:
         * Using multi-map here because we want to allow duplicates, storing multiple textures having the same
         * spec. Do note that find() will return an iterator to any instance of key-value pair among all values
         * associated with that key, so the order of returned values are not guaranteed.
         */
        static std::unordered_multimap<GHTexture2D::Desc, std::shared_ptr<GHTexture2D>> s_cache;
    };
}
