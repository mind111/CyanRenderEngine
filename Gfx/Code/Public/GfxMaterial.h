#pragma once

#include "Core.h"
#include "GHTexture.h"
#include "Shader.h"

namespace Cyan 
{
    class GfxContext;
    class PixelShader;

    struct MaterialParameter
    {
        MaterialParameter() { }
        MaterialParameter(const char* inName)
            : name(inName)
        {
        }
        virtual ~MaterialParameter() { }

        std::string name;
        bool bInstanced = true;

        virtual void bind(PixelShader* ps) = 0;

        virtual void setInt(i32 data) { assert(0); }
        virtual void setUint(u32 data) { assert(0); }
        virtual void setFloat(f32 data) { assert(0); }
        virtual void setVec2(const glm::vec2& data) { assert(0); }
        virtual void setVec3(const glm::vec3& data) { assert(0); }
        virtual void setVec4(const glm::vec4& data) { assert(0); }
        virtual void setTexture(GHTexture2D* texture) { assert(0); }
    };

    struct IntMaterialParameter : public MaterialParameter 
    {
        IntMaterialParameter(const char* inName)
            : MaterialParameter(inName), data(-1)
        {
        }

        i32 data;

        virtual void setInt(i32 inData) override { data = inData; }
        virtual void bind(PixelShader* ps) override { ps->setUniform(name.c_str(), data); }
    };

    struct UintMaterialParameter : public MaterialParameter 
    {
        UintMaterialParameter(const char* inName)
            : MaterialParameter(inName), data(0)
        {
        }

        u32 data;

        virtual void setUint(u32 inData) override { data = inData; }
        virtual void bind(PixelShader* ps) override { ps->setUniform(name.c_str(), data); }
    };

    struct FloatMaterialParameter : public MaterialParameter 
    {
        FloatMaterialParameter(const char* inName)
            : MaterialParameter(inName), data(0.f)
        {
        }

        f32 data;

        virtual void setFloat(f32 inData) override { data = inData; }
        virtual void bind(PixelShader* ps) override { ps->setUniform(name.c_str(), data); }
    };

    struct Vec2MaterialParameter : public MaterialParameter 
    {
        Vec2MaterialParameter(const char* inName)
            : MaterialParameter(inName), data(glm::vec2(0.f))
        {
        }

        glm::vec2 data;

        virtual void setVec2(const glm::vec2& inData) override { data = inData; }
        virtual void bind(PixelShader* ps) override { ps->setUniform(name.c_str(), data); }
    };

    struct Vec3MaterialParameter : public MaterialParameter 
    {
        Vec3MaterialParameter(const char* inName)
            : MaterialParameter(inName), data(glm::vec3(0.f))
        {
        }

        glm::vec3 data;

        virtual void setVec3(const glm::vec3& inData) override { data = inData; }
        virtual void bind(PixelShader* ps) override { ps->setUniform(name.c_str(), data); }
    };

    struct Vec4MaterialParameter : public MaterialParameter 
    {
        Vec4MaterialParameter(const char* inName)
            : MaterialParameter(inName), data(glm::vec4(0.f))
        {
        }

        glm::vec4 data;

        virtual void setVec4(const glm::vec4& inData) override { data = inData; }
        virtual void bind(PixelShader* ps) override { ps->setUniform(name.c_str(), data); }
    };

    struct Texture2DMaterialParameter : public MaterialParameter 
    {
        Texture2DMaterialParameter(const char* inName)
            : MaterialParameter(inName), texture(nullptr)
        {
        }

        GHTexture2D* texture = nullptr;

        virtual void setTexture(GHTexture2D* inTexture) override { texture = inTexture; }
        virtual void bind(PixelShader* ps) override 
        { 
            if (texture != nullptr)
            {
                bool bBound = false;
                ps->bindTexture(name.c_str(), texture, bBound); 
            }
        }
    };

    class GfxMaterialInstance;

    /**
     * This implementation of material works but has insane amount of overhead due to 
        excessive use of polymorphism, need to revisit and optimize later. Perfect example of
        over engineering but I like it :)
     */
    class GFX_API GfxMaterial
    {
    public:
        friend class GfxMaterialInstance;

        GfxMaterial(const char* materialAssetName, const char* pixelShaderFilePath);
        virtual ~GfxMaterial();

    protected:
        std::shared_ptr<GfxPipeline> m_pipeline = nullptr;
        struct MaterialParameterDesc
        {
            i32 index = -1;
            ShaderUniformDesc desc = { };
        };
        std::unordered_map<std::string, MaterialParameterDesc> m_parameterMap;
    };

    class GFX_API GfxMaterialInstance
    {
    public:
        friend class GfxMaterial;

        GfxMaterialInstance(const char* name, GfxMaterial* parent);
        ~GfxMaterialInstance() { }

        GfxPipeline* bind();
        void unbind();

        void setInt(const char* parameterName, i32 data);
        void setUint(const char* parameterName, u32 data);
        void setFloat(const char* parameterName, f32 data);
        void setVec2(const char* parameterName, const glm::vec2& data);
        void setVec3(const char* parameterName, const glm::vec3& data);
        void setVec4(const char* parameterName, const glm::vec4& data);
        void setTexture(const char* parameterName, GHTexture2D* texture);

    private:
        GfxMaterial* m_parent = nullptr;
        std::vector<MaterialParameter*> m_parameters;
    };
};
