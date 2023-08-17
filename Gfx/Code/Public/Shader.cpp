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
        m_GHO->bindTexture(samplerName, texture, outBound);
    }

    void Shader::unbindTexture(const char* samplerName)
    {
        const auto& entry = m_textureBindingMap.find(samplerName);
        if (entry != m_textureBindingMap.end())
        {
            auto texture = entry->second.texture;
            m_GHO->unbindTexture(samplerName, texture);
        }
    }

    void Shader::resetState()
    {
        for (const auto& binding : m_textureBindingMap)
        {
            unbindTexture(binding.first);
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
}
