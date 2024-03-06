#include "GfxHardwareAbstraction/GHInterface/GfxHardwareContext.h"
#include "Shader.h"
#include "GfxHardwareAbstraction/GHInterface/GHTexture.h"
#include "GfxContext.h"

namespace Cyan
{
    Shader::Shader(const char* name)
        : m_name(name)
    {
    }

    void Shader::bindTexture(const char* samplerName, GHTexture* texture, bool& outBound)
    {
        auto entry = m_textureBindingMap.find(samplerName);
        if (entry == m_textureBindingMap.end())
        {
            m_GHO->bindTexture(samplerName, texture, outBound);
            if (outBound)
            {
                m_textureBindingMap.insert({ std::string(samplerName), texture });
            }
        }
        else
        {
            if (entry->second != texture)
            {
                m_GHO->bindTexture(samplerName, texture, outBound);
                m_textureBindingMap[samplerName] = texture;
            }
        }
    }

    void Shader::unbindTexture(const char* samplerName)
    {
        const auto& entry = m_textureBindingMap.find(samplerName);
        if (entry != m_textureBindingMap.end())
        {
            // auto texture = entry->second.texture;
            auto texture = entry->second;
            m_GHO->unbindTexture(samplerName, texture);
            entry->second = nullptr;
        }
        else
        {
            assert(0);
        }
    }

    void Shader::bindRWBuffer(const char* bufferName, GHRWBuffer* buffer)
    {
        bool bOutBound = false;
        auto entry = m_RWBufferBindingMap.find(bufferName);
        if (entry == m_RWBufferBindingMap.end())
        {
            m_GHO->bindRWBuffer(bufferName, buffer, bOutBound);
            if (bOutBound)
            {
                m_RWBufferBindingMap.insert({ std::string(bufferName), buffer });
            }
        }
        else
        {
            if (entry->second != buffer)
            {
                m_GHO->bindRWBuffer(bufferName, buffer, bOutBound);
                m_RWBufferBindingMap[bufferName] = buffer;
            }
        }
    }

    void Shader::unbindRWBuffer(const char* bufferName)
    {
        const auto& entry = m_RWBufferBindingMap.find(bufferName);
        if (entry != m_RWBufferBindingMap.end())
        {
            assert(entry->second != nullptr);
            m_GHO->unbindRWBuffer(entry->first.c_str());
            entry->second = nullptr;
        }
        else
        {
            assert(0);
        }
    }

    void Shader::bindAtomicCounter(const char* name, GHAtomicCounterBuffer* atomicCounter)
    {
        bool bOutBound = false;
        auto entry = m_atomicCounterBindingMap.find(name);
        if (entry == m_atomicCounterBindingMap.end())
        {
            m_GHO->bindAtomicCounter(name, atomicCounter, bOutBound);
            if (bOutBound)
            {
                m_atomicCounterBindingMap.insert({ std::string(name), atomicCounter });
            }
        }
        else
        {
            if (atomicCounter != entry->second)
            {
                m_GHO->bindAtomicCounter(name, atomicCounter, bOutBound);
                m_atomicCounterBindingMap[name] = atomicCounter;
            }
        }
    }

    void Shader::unbindAtomicCounter(const char* name)
    {
        auto entry = m_atomicCounterBindingMap.find(name);
        if (entry != m_atomicCounterBindingMap.end())
        {
            assert(entry->second != nullptr);
            m_GHO->unbindAtomicCounter(name);
            entry->second = nullptr;
        }
        else
        {
            assert(0);
        }
    }

    void Shader::bindRWTexture(const char* name, GHTexture* RWTexture, u32 mipLevel)
    {
        bool bBound = false;
        auto entry = m_RWTextureBindingMap.find(name);
        if (entry == m_RWTextureBindingMap.end())
        {
             m_GHO->bindRWTexture(name, RWTexture, mipLevel, bBound);
             if (bBound)
             {
                 m_RWTextureBindingMap.insert({ std::string(name), RWTexture });
             }
        }
        else
        {
            if (entry->second != RWTexture)
            {
                 m_GHO->bindRWTexture(name, RWTexture, mipLevel, bBound);
                 m_RWTextureBindingMap[name] = RWTexture;
            }
        }
    }

    void Shader::unbindRWTexture(const char* name)
    {
        auto entry = m_RWTextureBindingMap.find(name);
        if (entry != m_RWTextureBindingMap.end())
        {
            entry->second->unbindAsRWTexture();
            entry->second = nullptr;
        }
        else
        {
            assert(0);
        }
    }

    void Shader::resetState()
    {
        for (const auto& textureBinding : m_textureBindingMap)
        {
            unbindTexture(textureBinding.first.c_str());
        }
        for (const auto& RWBufferBinding : m_RWBufferBindingMap)
        {
            unbindRWBuffer(RWBufferBinding.first.c_str());
        }
        for (const auto& atomicCounterBinding : m_atomicCounterBindingMap)
        {
            unbindAtomicCounter(atomicCounterBinding.first.c_str());
        }
        for (const auto& RWTextureBinding : m_RWTextureBindingMap)
        {
            unbindRWTexture(RWTextureBinding.first.c_str());
        }
    }

    VertexShader::VertexShader(const char* name, const std::string& text)
        : Shader(name)
    {
        m_GHO.reset(GfxHardwareContext::get()->createVertexShader(text));
    }

    VertexShader::~VertexShader()
    {
    }

    GHVertexShader* VertexShader::getGHO() 
    { 
        /**
         * Note: This cast should be safe here since m_GHO is guaranteed to be type VertexShader
         */
        return static_cast<GHVertexShader*>(m_GHO.get()); 
    }

    PixelShader::PixelShader(const char* name, const std::string& text)
        : Shader(name)
    {
        m_GHO.reset(GfxHardwareContext::get()->createPixelShader(text));
    }

    PixelShader::~PixelShader()
    {
    }

    GHPixelShader* PixelShader::getGHO()
    {
        /**
         * Note: This cast should be safe here since m_GHO is guaranteed to be type VertexShader
         */
        return static_cast<GHPixelShader*>(m_GHO.get()); 
    }

    ComputeShader::ComputeShader(const char* name, const std::string& text)
        : Shader(name)
    {
        m_GHO.reset(GfxHardwareContext::get()->createComputeShader(text));
    }

    ComputeShader::~ComputeShader()
    {

    }

    GHComputeShader* ComputeShader::getGHO()
    {
        return static_cast<GHComputeShader*>(m_GHO.get()); 
    }

    GeometryShader::GeometryShader(const char* name, const std::string& text)
        : Shader(name)
    {
        m_GHO.reset(GfxHardwareContext::get()->createGeometryShader(text));
    }
    
    GeometryShader::~GeometryShader()
    {

    }

    GHGeometryShader* GeometryShader::getGHO()
    {
        return static_cast<GHGeometryShader*>(m_GHO.get()); 
    }

    Pipeline::Pipeline(const char* name)
        : m_name(name)
    {

    }

    GfxPipeline::GfxPipeline(const char* name, std::shared_ptr<VertexShader> vs, std::shared_ptr<PixelShader> ps)
        : Pipeline(name), m_vs(vs), m_ps(ps)
    {
        m_GHO.reset(GfxHardwareContext::get()->createGfxPipeline(vs->m_GHO, ps->m_GHO));
    }

    GfxPipeline::~GfxPipeline()
    {
    }

    void GfxPipeline::bind()
    {
        m_GHO->bind();
        bBound = true;
    }

    void GfxPipeline::unbind()
    {
        m_vs->resetState();
        m_ps->resetState();

        m_GHO->unbind();
        bBound = false;
    }

    void GfxPipeline::setTexture(const char* samplerName, GHTexture* texture)
    {
        bool bBoundToVS = false, bBoundToPS = false;
        m_vs->bindTexture(samplerName, texture, bBoundToVS);
        m_ps->bindTexture(samplerName, texture, bBoundToPS);
        // assert(bBoundToVS | bBoundToPS);
    }

    void GfxPipeline::setRWBuffer(const char* name, GHRWBuffer* buffer)
    {
        m_vs->bindRWBuffer(name, buffer);
        m_ps->bindRWBuffer(name, buffer);
    }

    void GfxPipeline::bindAtomicCounter(const char* name, GHAtomicCounterBuffer* atomicCounter)
    {
        m_vs->bindAtomicCounter(name, atomicCounter);
        m_ps->bindAtomicCounter(name, atomicCounter);
    }

    void GfxPipeline::setRWTexture(const char* name, GHTexture* RWTexture, u32 mipLevel)
    {
        m_vs->bindRWTexture(name, RWTexture, mipLevel);
        m_ps->bindRWTexture(name, RWTexture, mipLevel);
    }

    ComputePipeline::ComputePipeline(const char* name, std::shared_ptr<ComputeShader> cs)
        : Pipeline(name)
        , m_cs(cs)
    {
        m_GHO.reset(GfxHardwareContext::get()->createComputePipeline(cs->m_GHO));
    }

    ComputePipeline::~ComputePipeline()
    {

    }

    void ComputePipeline::bind()
    {
        m_GHO->bind();
        bBound = true;
    }

    void ComputePipeline::unbind()
    {
        m_cs->resetState();
        m_GHO->unbind();
        bBound = false;
    }

    void ComputePipeline::setTexture(const char* samplerName, GHTexture* texture)
    {
        bool bBound = false;
        m_cs->bindTexture(samplerName, texture, bBound);
    }

    void ComputePipeline::setRWBuffer(const char* name, GHRWBuffer* buffer)
    {
        m_cs->bindRWBuffer(name, buffer);
    }
 
    void ComputePipeline::setRWTexture(const char* name, GHTexture* RWTexture, u32 mipLevel)
    {
        bool bBound = false;
        m_cs->bindRWTexture(name, RWTexture, mipLevel);
    }

    void ComputePipeline::bindAtomicCounter(const char* name, GHAtomicCounterBuffer* atomicCounter)
    {
        bool bBound = false;
        m_cs->bindAtomicCounter(name, atomicCounter);
    }

    GeometryPipeline::GeometryPipeline(const char* name, std::shared_ptr<VertexShader> vs, std::shared_ptr<GeometryShader> gs, std::shared_ptr<PixelShader> ps)
        : Pipeline(name)
        , m_vs(vs)
        , m_gs(gs)
        , m_ps(ps)
    {
        m_GHO.reset(GfxHardwareContext::get()->createGeometryPipeline(vs->m_GHO, gs->m_GHO, ps->m_GHO));
    }

    GeometryPipeline::~GeometryPipeline()
    {

    }

    void GeometryPipeline::bind()
    {
        m_GHO->bind();
        bBound = true;
    }

    void GeometryPipeline::unbind()
    {
        m_vs->resetState();
        m_gs->resetState();
        m_ps->resetState();
        m_GHO->unbind();
        bBound = false;
    }

    void GeometryPipeline::setTexture(const char* samplerName, GHTexture* texture)
    {
        bool bBound = false;
        m_vs->bindTexture(samplerName, texture, bBound);
        m_gs->bindTexture(samplerName, texture, bBound);
        m_ps->bindTexture(samplerName, texture, bBound);
    }

    void GeometryPipeline::setRWBuffer(const char* name, GHRWBuffer* buffer)
    {
        m_vs->bindRWBuffer(name, buffer);
        m_gs->bindRWBuffer(name, buffer);
        m_ps->bindRWBuffer(name, buffer);
    }

    void GeometryPipeline::setRWTexture(const char* name, GHTexture* RWTexture, u32 mipLevel)
    {
        bool bBound = false;
        m_vs->bindRWTexture(name, RWTexture, mipLevel);
        m_gs->bindRWTexture(name, RWTexture, mipLevel);
        m_ps->bindRWTexture(name, RWTexture, mipLevel);
    }

    void GeometryPipeline::bindAtomicCounter(const char* name, GHAtomicCounterBuffer* counter)
    {
        bool bBound = false;
        m_vs->bindAtomicCounter(name, counter);
        m_gs->bindAtomicCounter(name, counter);
        m_ps->bindAtomicCounter(name, counter);
    }

    ShaderManager::ShaderManager()
    {

    }

    ShaderManager* ShaderManager::s_instance = nullptr;

    ShaderManager::~ShaderManager()
    {
        // todo: destroy all created shader/pipeline resources here
    }

    ShaderManager* ShaderManager::get()
    {
        static auto instance = std::unique_ptr<ShaderManager>(new ShaderManager());
        if (s_instance == nullptr)
        {
            s_instance = instance.get();
        }
        return s_instance;
    }

    std::shared_ptr<GfxPipeline> ShaderManager::findOrCreateGfxPipeline(bool& outFound, std::shared_ptr<VertexShader> vs, std::shared_ptr<PixelShader> ps)
    {
        std::string gfxPipelineName = "Gfx_" + vs->getName() + "_" + ps->getName();
        auto found = findPipeline<GfxPipeline>(outFound, gfxPipelineName.c_str());
        if (found != nullptr)
        {
            return found;
        }
        return createGfxPipeline(gfxPipelineName.c_str(), vs, ps);
    }

    std::shared_ptr<GfxPipeline> ShaderManager::createGfxPipeline(const char* name, std::shared_ptr<VertexShader> vs, std::shared_ptr<PixelShader> ps)
    {
        auto gfxPipeline = std::make_shared<GfxPipeline>(name, vs, ps);
        s_instance->m_pipelineMap.insert({ std::string(name), gfxPipeline });
        return gfxPipeline;
    }

    std::shared_ptr<Cyan::ComputePipeline> ShaderManager::findOrCreateComputePipeline(bool& outFound, std::shared_ptr<ComputeShader> cs)
    {
        std::string computePipelineName = "Compute_" + cs->getName();
        auto found = findPipeline<ComputePipeline>(outFound, computePipelineName.c_str());
        if (found != nullptr)
        {
            return found;
        }
        return createComputePipeline(computePipelineName.c_str(), cs);
    }

    std::shared_ptr<Cyan::ComputePipeline> ShaderManager::createComputePipeline(const char* name, std::shared_ptr<ComputeShader> cs)
    {
        auto computePipeline = std::make_shared<ComputePipeline>(name, cs);
        s_instance->m_pipelineMap.insert({ std::string(name), computePipeline });
        return computePipeline;
    }

    std::shared_ptr<Cyan::GeometryPipeline> ShaderManager::createGeometryPipeline(const char* name, std::shared_ptr<VertexShader> vs, std::shared_ptr<GeometryShader> gs, std::shared_ptr<PixelShader> ps)
    {
        auto geometryPipeline = std::make_shared<GeometryPipeline>(name, vs, gs, ps);
        s_instance->m_pipelineMap.insert({ std::string(name), geometryPipeline });
        return geometryPipeline;
    }

    std::shared_ptr<Cyan::GeometryPipeline> ShaderManager::findOrCreateGeometryPipeline(bool& outFound, std::shared_ptr<VertexShader> vs, std::shared_ptr<GeometryShader> gs, std::shared_ptr<PixelShader> ps)
    {
        std::string geometryPipelineName = "Geometry" + vs->getName() + "_" + gs->getName() + "_" + ps->getName();
        auto found = findPipeline<GeometryPipeline>(outFound, geometryPipelineName.c_str());
        if (found != nullptr)
        {
            return found;
        }
        return createGeometryPipeline(geometryPipelineName.c_str(), vs, gs, ps);
    }
}
