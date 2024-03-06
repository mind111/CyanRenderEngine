#pragma once

#include "Core.h"
#include "Gfx.h"
#include "GfxHardwareAbstraction/GHInterface/GHShader.h"
#include "GfxHardwareAbstraction/GHInterface/GHPipeline.h"
#include "GfxHardwareAbstraction/GHInterface/GHBuffer.h"

namespace Cyan
{
#define ENGINE_SHADER_PATH "C:/dev/cyanRenderEngine/Gfx/Shader/"

    class GFX_API Shader
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
        void bindRWBuffer(const char* bufferName, GHRWBuffer* buffer);
        void unbindRWBuffer(const char* bufferName);
        void bindAtomicCounter(const char* name, GHAtomicCounterBuffer* atomicCounter);
        void unbindAtomicCounter(const char* name);
        void bindRWTexture(const char* name, GHTexture* RWTexture, u32 mipLevel);
        void unbindRWTexture(const char* name);

        void resetState();

    protected:

        Shader(const char* name);

        std::string m_name;
        std::shared_ptr<GHShader> m_GHO = nullptr;
        std::unordered_map<std::string, GHTexture*> m_textureBindingMap;
        std::unordered_map<std::string, GHTexture*> m_RWTextureBindingMap;
        std::unordered_map<std::string, GHRWBuffer*> m_RWBufferBindingMap;
        std::unordered_map<std::string, GHAtomicCounterBuffer*> m_atomicCounterBindingMap;
    };

    class GFX_API VertexShader : public Shader
    {
    public:
        friend class GfxPipeline;
        friend class GeometryPipeline;

        VertexShader(const char* name, const std::string& text);
        virtual ~VertexShader();

        GHVertexShader* getGHO();
    };

    class GFX_API PixelShader : public Shader
    {
    public:
        friend class GfxPipeline;
        friend class GeometryPipeline;

        PixelShader(const char* name, const std::string& text);
        virtual ~PixelShader();

        GHPixelShader* getGHO();
    };

    class GFX_API GeometryShader : public Shader
    {
    public:
        friend class GeometryPipeline;

        GeometryShader(const char* name, const std::string& text);
        virtual ~GeometryShader();

        GHGeometryShader* getGHO();
    };

    class GFX_API ComputeShader : public Shader
    {
    public:
        friend class ComputePipeline;

        ComputeShader(const char* name, const std::string& text);
        virtual ~ComputeShader();

        GHComputeShader* getGHO();
    };

    class GFX_API Pipeline
    {
    public:
        virtual ~Pipeline() { }

        virtual void setUniform(const char* name, i32 value) = 0;
        virtual void setUniform(const char* name, u32 value) = 0;
        virtual void setUniform(const char* name, f32 value) = 0;
        virtual void setUniform(const char* name, const glm::uvec2& value) = 0;
        virtual void setUniform(const char* name, const glm::ivec2& value) = 0;
        virtual void setUniform(const char* name, const glm::ivec3& value) = 0;
        virtual void setUniform(const char* name, const glm::vec2& value) = 0;
        virtual void setUniform(const char* name, const glm::vec3& value) = 0;
        virtual void setUniform(const char* name, const glm::vec4& value) = 0;
        virtual void setUniform(const char* name, const glm::mat4& value) = 0;
        virtual void setTexture(const char* samplerName, GHTexture* texture) = 0;
        virtual void setRWBuffer(const char* name, GHRWBuffer* buffer) = 0;
        virtual void bindAtomicCounter(const char* name, GHAtomicCounterBuffer* atomicCounter) = 0;
        virtual void setRWTexture(const char* name, GHTexture* RWTexture, u32 mipLevel) = 0;
    protected: 
        Pipeline(const char* name);

        std::string m_name;
    };

    class GFX_API GfxPipeline : public Pipeline
    {
    public:
        GfxPipeline(const char* name, std::shared_ptr<VertexShader> vs, std::shared_ptr<PixelShader> ps);
        ~GfxPipeline();

        void bind();
        void unbind();

        virtual void setUniform(const char* name, i32 value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, u32 value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, f32 value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::uvec2& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::ivec2& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::ivec3& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::vec2& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::vec3& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::vec4& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::mat4& value) override { setUniformInternal(name, value); }
        virtual void setTexture(const char* samplerName, GHTexture* texture) override;
        virtual void setRWBuffer(const char* name, GHRWBuffer* buffer) override;
        virtual void bindAtomicCounter(const char* name, GHAtomicCounterBuffer* atomicCounter) override;
        virtual void setRWTexture(const char* name, GHTexture* RWTexture, u32 mipLevel) override;

        VertexShader* getVertexShader() { return m_vs.get(); }
        PixelShader* getPixelShader() { return m_ps.get(); }

        template <typename T>
        void setUniformInternal(const char* name, const T& value)
        {
            assert(bBound);

            m_vs->setUniform(name, value);
            m_ps->setUniform(name, value);
        }


    private:
        std::shared_ptr<VertexShader> m_vs;
        std::shared_ptr<PixelShader> m_ps;
        std::unique_ptr<GHGfxPipeline> m_GHO = nullptr;
        bool bBound = false;
    };

    class GFX_API ComputePipeline : public Pipeline
    {
    public:
        ComputePipeline(const char* name, std::shared_ptr<ComputeShader> cs);
        ~ComputePipeline();

        void bind();
        void unbind();

        virtual void setUniform(const char* name, i32 value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, u32 value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, f32 value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::uvec2& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::ivec2& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::ivec3& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::vec2& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::vec3& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::vec4& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::mat4& value) override { setUniformInternal(name, value); }
        virtual void setTexture(const char* samplerName, GHTexture* texture) override;
        virtual void setRWTexture(const char* name, GHTexture* RWTexture, u32 mipLevel) override;
        virtual void setRWBuffer(const char* name, GHRWBuffer* buffer) override;
        virtual void bindAtomicCounter(const char* name, GHAtomicCounterBuffer* atomicCounter) override;

        ComputeShader* getComputeShader() { return m_cs.get(); }

        template <typename T>
        void setUniformInternal(const char* name, const T& value)
        {
            assert(bBound);

            m_cs->setUniform(name, value);
        }

    private:
        std::shared_ptr<ComputeShader> m_cs = nullptr;
        std::unique_ptr<GHComputePipeline> m_GHO = nullptr;
        bool bBound = false;
    };

    class GFX_API GeometryPipeline : public Pipeline
    {
    public:
        GeometryPipeline(const char* name, std::shared_ptr<VertexShader> vs, std::shared_ptr<GeometryShader> gs, std::shared_ptr<PixelShader> ps);
        ~GeometryPipeline();

        void bind();
        void unbind();

        virtual void setUniform(const char* name, i32 value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, u32 value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, f32 value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::uvec2& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::ivec2& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::ivec3& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::vec2& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::vec3& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::vec4& value) override { setUniformInternal(name, value); }
        virtual void setUniform(const char* name, const glm::mat4& value) override { setUniformInternal(name, value); }
        virtual void setTexture(const char* samplerName, GHTexture* texture) override;
        virtual void setRWTexture(const char* name, GHTexture* RWTexture, u32 mipLevel) override;
        virtual void setRWBuffer(const char* name, GHRWBuffer* buffer) override;
        virtual void bindAtomicCounter(const char* name, GHAtomicCounterBuffer* counter) override;

        template <typename T>
        void setUniformInternal(const char* name, const T& value)
        {
            assert(bBound);

            m_vs->setUniform(name, value);
            m_gs->setUniform(name, value);
            m_ps->setUniform(name, value);
        }

    private:
        std::shared_ptr<VertexShader> m_vs = nullptr;
        std::shared_ptr<GeometryShader> m_gs = nullptr;
        std::shared_ptr<PixelShader> m_ps = nullptr;
        std::unique_ptr<GHGeometryPipeline> m_GHO = nullptr;
        bool bBound = false;
    };

    class GFX_API ShaderManager
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
        static std::shared_ptr<ComputePipeline> findOrCreateComputePipeline(bool& outFound, std::shared_ptr<ComputeShader> cs);
        static std::shared_ptr<ComputePipeline> createComputePipeline(const char* name, std::shared_ptr<ComputeShader> cs);
        static std::shared_ptr<GeometryPipeline> createGeometryPipeline(const char* name, std::shared_ptr<VertexShader> vs, std::shared_ptr<GeometryShader> gs, std::shared_ptr<PixelShader> ps);
        static std::shared_ptr<GeometryPipeline> findOrCreateGeometryPipeline(bool& outFound, std::shared_ptr<VertexShader> vs, std::shared_ptr<GeometryShader> gs, std::shared_ptr<PixelShader> ps);
    private:
        ShaderManager();

        static ShaderManager* s_instance;
        std::unordered_map<std::string, std::shared_ptr<Shader>> m_shaderMap;
        std::unordered_map<std::string, std::shared_ptr<Pipeline>> m_pipelineMap;
    };
}
