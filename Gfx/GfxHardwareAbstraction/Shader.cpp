#include "GfxHardwareContext.h"
#include "Shader.h"
#include "GHTexture.h"
#include "GfxContext.h"

namespace Cyan
{
    Shader::Shader(const char* name)
        : m_name(name)
    {
    }

    void Shader::bindTexture(const char* samplerName, GHTexture* texture)
    {
        m_GHO->bindTexture(samplerName, texture);
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

    VertexShader::VertexShader(const char* name, const char* text)
        : Shader(name)
    {
        m_GHO.reset(GfxHardwareContext::get()->createVertexShader(text));
    }

    VertexShader::~VertexShader()
    {
    }

    std::shared_ptr<GHVertexShader> VertexShader::getGHO() 
    { 
        /**
         * Note: This cast should be safe here since m_GHO is guaranteed to be type VertexShader
         */
        return std::static_pointer_cast<GHVertexShader>(m_GHO); 
    }

    PixelShader::PixelShader(const char* name, const char* text)
        : Shader(name)
    {
        m_GHO.reset(GfxHardwareContext::get()->createPixelShader(text));
    }

    PixelShader::~PixelShader()
    {
    }

    std::shared_ptr<Cyan::GHPixelShader> PixelShader::getGHO()
    {
        /**
         * Note: This cast should be safe here since m_GHO is guaranteed to be type VertexShader
         */
        return std::static_pointer_cast<GHPixelShader>(m_GHO); 
    }

    Pipeline::Pipeline(const char* name)
        : m_name(name)
    {

    }

    GfxPipeline::GfxPipeline(const char* name, std::shared_ptr<VertexShader> vs, std::shared_ptr<PixelShader> ps)
        : Pipeline(name), m_vs(vs), m_ps(ps)
    {
        m_GHO.reset(GfxHardwareContext::get()->createGfxPipeline(vs->getGHO(), ps->getGHO()));
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
}
