#include "RenderTexture.h"

namespace Cyan
{
    std::unordered_multimap<GHTexture2D::Desc, std::shared_ptr<GHTexture2D>> RenderTexture2D::s_cache;

    RenderTexture::RenderTexture(const char* name)
        : m_name(name)
        , m_refCount(nullptr)
    {
        m_refCount = std::make_shared<u32>(1);
    }

    RenderTexture::RenderTexture(const RenderTexture& src)
    {
        m_name = src.m_name;
        m_refCount = src.m_refCount;
        (*m_refCount)++;
    }

    RenderTexture& RenderTexture::operator=(const RenderTexture& src)
    {
        if (m_refCount != nullptr)
        {
            (*m_refCount)--;
        }
        m_name = src.m_name;
        m_refCount = src.m_refCount;
        (*m_refCount)++;
        return *this;
    }

    RenderTexture::~RenderTexture()
    {
        (*m_refCount)--;
    }

    RenderTexture2D::RenderTexture2D(const char* name, const GHTexture2D::Desc& desc, const GHSampler2D& sampler)
        : RenderTexture(name)
        , m_GHTexture2D(nullptr)
    {
        // search in the texture cache
        auto entry = s_cache.find(desc);
        if (entry == s_cache.end())
        {
            // create a new texture and add it to the cache
            m_GHTexture2D = std::move(GHTexture2D::create(desc));
        }
        else
        {
            // recycle a texture and remove it from the cache
            m_GHTexture2D = entry->second;
            s_cache.erase(entry);
        }
    }

    RenderTexture2D::RenderTexture2D(const RenderTexture2D& src)
        : RenderTexture(src), m_GHTexture2D(src.m_GHTexture2D)
    { }

    RenderTexture2D::RenderTexture2D(const char* name, GHTexture2D* srcGHTexture, const GHSampler2D& sampler)
        : RenderTexture2D(name, m_GHTexture2D->getDesc(), sampler)
    {

    }

    RenderTexture2D& RenderTexture2D::operator=(const RenderTexture2D& src)
    {
        RenderTexture::operator=(src);
        m_GHTexture2D = src.m_GHTexture2D;
        return *this;
    }

    /** 
        * It may seem weird that release() is called when refCount is still 1, this is because RenderTexture::~RenderTexture() is called 
        after RenderTexture2D::~RenderTexture2D() gets called, thus when refCount is 1 here, it means the refCount is about
        to go to 0, and resource can be released here.
    */
    RenderTexture2D::~RenderTexture2D()
    {
        if (*m_refCount == 1u)
        {
            release();
        }
    }

    void RenderTexture2D::release()
    {
        s_cache.insert({ m_GHTexture2D->getDesc(), m_GHTexture2D });
        m_GHTexture2D.reset();
    }
}
