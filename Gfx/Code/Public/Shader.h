#pragma once

#include "Core.h"
#include "GHShader.h"
#include "GHPipeline.h"

namespace Cyan
{
#define SHADER_TEXT_PATH "C:\\dev\\cyanRenderEngine\\Gfx\\Shader\\"

    class Shader
    {
    public:
        virtual ~Shader() { }
        std::string getName() { return m_name; }

        template <typename T>
        void setUniform(const char* name, const T& value)
        {
            m_GHO->setUniform(name, value);
        }

        void bindTexture(const char* samplerName, GHTexture* texture, bool& outBound);
        void unbindTexture(const char* samplerName);

        void resetState();

    protected:
        struct TextureBinding 
        {
            const char* samplerName = nullptr;
            GHTexture* texture = nullptr;
        };

        Shader(const char* name);

        std::string m_name;
        std::shared_ptr<GHShader> m_GHO = nullptr;
        std::unordered_map<const char*, TextureBinding> m_textureBindingMap;
    };

    class VertexShader : public Shader
    {
    public:
        VertexShader(const char* name, const std::string& text);
        virtual ~VertexShader();

        std::shared_ptr<GHVertexShader> getGHO();
    };

    class PixelShader : public Shader
    {
    public:
        PixelShader(const char* name, const std::string& text);
        virtual ~PixelShader();

        std::shared_ptr<GHPixelShader> getGHO();
    };

    class Pipeline
    {
    public:
        virtual ~Pipeline() { }
    protected: 
        Pipeline(const char* name);

        std::string m_name;
    };

    class GfxPipeline : public Pipeline
    {
    public:
        GfxPipeline(const char* name, std::shared_ptr<VertexShader> vs, std::shared_ptr<PixelShader> ps);
        ~GfxPipeline();

        void bind();
        void unbind();

        template <typename T>
        void setUniform(const char* name, const T& value)
        {
            assert(bBound);

            m_vs->setUniform(name, value);
            m_ps->setUniform(name, value);
        }

        void setTexture(const char* samplerName, GHTexture* texture);

    private:
        std::shared_ptr<VertexShader> m_vs;
        std::shared_ptr<PixelShader> m_ps;
        std::unique_ptr<GHGfxPipeline> m_GHO = nullptr;
        bool bBound = false;
    };

    class ShaderManager
    {
    public:
        ~ShaderManager();

        static ShaderManager* get();

        template <typename T>
        static std::shared_ptr<T> findShader(const char* name)
        {
            auto cached = s_instance->m_shaderMap.find(name);
            if (cached != s_instance->m_shaderMap.end())
            {
                std::shared_ptr<T> found = std::dynamic_pointer_cast<T>(cached->second);
                return found;
            }
            return nullptr;
        }

        template <typename T>
        static std::shared_ptr<T> createShader(const char* name, const char* shaderTextPath)
        {
            std::string path(shaderTextPath);
            std::string text;
            auto pos = path.find_last_of('.');
            if (pos != std::string::npos)
            {
                std::string suffix = path.substr(pos);
                // enforce a standard suffix for glsl shader
                if (suffix == ".glsl")
                {
                    std::ifstream shaderTextFile(shaderTextPath);
                    if (shaderTextFile.is_open())
                    {
                        std::string line;
                        while (std::getline(shaderTextFile, line)) 
                        {
                            text += line;
                            text += '\n';
                        }
                        shaderTextFile.close();
                    }
                }
                else
                {
                    cyanError("Shader text file must ends with .glsl");
                    assert(0);
                }
            }

            auto shader = std::make_shared<T>(name, text);
            s_instance->m_shaderMap.insert({ std::string(name), shader });
            return shader;
        }

        template <typename T>
        static std::shared_ptr<T> findOrCreateShader(bool& outFound, const char* name, const char* shaderTextPath)
        {
            auto found = findShader<T>(name);
            outFound = (found != nullptr);
            if (outFound)
            {
                return found;
            }
            assert(!outFound);
            return createShader<T>(name, shaderTextPath);
        }

        template <typename T>
        static std::shared_ptr<T> findPipeline(bool& outFound, const char* name)
        {
            auto cached = s_instance->m_pipelineMap.find(name);
            if (cached != s_instance->m_pipelineMap.end())
            {
                std::shared_ptr<T> found = std::dynamic_pointer_cast<T>(cached->second);
                return found;
            }
            return nullptr;
        }

        static std::shared_ptr<GfxPipeline> findOrCreateGfxPipeline(bool& outFound, std::shared_ptr<VertexShader> vs, std::shared_ptr<PixelShader> ps);
        static std::shared_ptr<GfxPipeline> createGfxPipeline(const char* name, std::shared_ptr<VertexShader> vs, std::shared_ptr<PixelShader> ps);

    private:
        ShaderManager();

        static ShaderManager* s_instance;
        std::unordered_map<std::string, std::shared_ptr<Shader>> m_shaderMap;
        std::unordered_map<std::string, std::shared_ptr<Pipeline>> m_pipelineMap;
    };
}
