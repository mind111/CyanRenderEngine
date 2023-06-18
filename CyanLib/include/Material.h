#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <array>

#include "Asset.h"
#include "Texture.h"
#include "Shader.h"
#include "SceneCamera.h"

#define MATERIAL_SOURCE_PATH "C:/dev/cyanRenderEngine/materials/"

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
        virtual void setTexture(Texture2D* texture) { assert(0); }
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

        Texture2D* texture = nullptr;

        virtual void setTexture(Texture2D* inTexture) override { texture = inTexture; }
        virtual void bind(PixelShader* ps) override 
        { 
            if (texture != nullptr && texture->isInited())
            {
                ps->setTexture(name.c_str(), texture->getGfxResource()); 
            }
        }
    };

    class MaterialInstance;

    /**
     * This implementation of material works but has insane amount of overhead due to 
        excessive use of polymorphism, need to revisit and optimize later. Perfect example of
        over engineering but I like it :)
     */
    class Material : public Asset
    {
    public:
        friend class MaterialInstance;

        using SetupDefaultInstance = std::function<void(MaterialInstance*)>;
        Material(const char* name, const char* materialSourcePath, const SetupDefaultInstance& setupDefaultInstance);
        virtual ~Material();

        static const char* getAssetTypeName() { return "Material"; }

        void bind(GfxContext* ctx, const glm::mat4& localToWorldMatrix, const SceneCamera::ViewParameters& viewParameters);

        void setInt(const char* parameterName, i32 data);
        void setUint(const char* parameterName, u32 data);
        void setFloat(const char* parameterName, f32 data);
        void setVec2(const char* parameterName, const glm::vec2& data);
        void setVec3(const char* parameterName, const glm::vec3& data);
        void setVec4(const char* parameterName, const glm::vec4& data);
        void setTexture(const char* parameterName, Texture2D* texture);

        static constexpr const char* defaultOpaqueMaterialPath = MATERIAL_SOURCE_PATH "M_DefaultOpaque_p.glsl";

    protected:
        std::shared_ptr<PixelPipeline> m_pipeline = nullptr;
        struct MaterialParameterDesc
        {
            i32 index = -1;
            Shader::UniformDesc desc = { };
        };
        std::unordered_map<std::string, MaterialParameterDesc> m_parameterMap;
        std::function<void(Material*)> m_setupDefaultInstance;
        std::unique_ptr<MaterialInstance> m_defaultInstance = nullptr;

    };

    class MaterialInstance : public Asset
    {
    public:
        friend class Material;

        MaterialInstance(const char* name, Material* parent);
        ~MaterialInstance() { }

        void setInt(const char* parameterName, i32 data);
        void setUint(const char* parameterName, u32 data);
        void setFloat(const char* parameterName, f32 data);
        void setVec2(const char* parameterName, const glm::vec2& data);
        void setVec3(const char* parameterName, const glm::vec3& data);
        void setVec4(const char* parameterName, const glm::vec4& data);
        void setTexture(const char* parameterName, Texture2D* texture);

    private:
        Material* m_parent = nullptr;
        std::vector<MaterialParameter*> m_parameters;
    };
};