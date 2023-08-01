#pragma once

#include "Core.h"
#include "Texture.h"
#include "Shader.h"

#define MATERIAL_SOURCE_PATH "C:/dev/cyanRenderEngine/Engine/Material/"

namespace Cyan 
{
    class GfxContext;
    class PixelShader;
    class GfxMaterial;
    class GfxMaterialInstance;

    class MaterialInstance;

    using MaterialSetupFunc = std::function<void(class Material*)>;
    /**
     * This implementation of material works but has insane amount of overhead due to 
        excessive use of polymorphism, need to revisit and optimize later. Perfect example of
        over engineering but I like it :)
     */
    class Material : public Asset
    {
    public:
        friend class MaterialInstance;

        Material(const char* name, const char* materialSourcePath, const MaterialSetupFunc& setupDefaultInstance);
        virtual ~Material();

        MaterialInstance* createInstance(const char* instanceName);
        void removeInstance(const char* instanceName);

        static const char* getAssetTypeName() { return "Material"; }
        MaterialInstance* getDefaultInstance() { return m_defaultInstance.get(); }

        void setInt(const char* parameterName, i32 data);
        void setUint(const char* parameterName, u32 data);
        void setFloat(const char* parameterName, f32 data);
        void setVec2(const char* parameterName, const glm::vec2& data);
        void setVec3(const char* parameterName, const glm::vec3& data);
        void setVec4(const char* parameterName, const glm::vec4& data);
        void setTexture(const char* parameterName, Texture2D* texture);

        static constexpr const char* defaultOpaqueMaterialPath = MATERIAL_SOURCE_PATH "M_DefaultOpaque_p.glsl";

    protected:
        const char* m_materialSourcePath = nullptr;
        MaterialSetupFunc m_defaultInstanceSetupFunc;
        std::unique_ptr<MaterialInstance> m_defaultInstance = nullptr;
        std::vector<MaterialInstance*> m_instances;

        /* Danger Zone: this member should only accessed by the render thread */
        std::unique_ptr<GfxMaterial> m_gfxMaterial = nullptr;
    };

    class MaterialInstance : public Asset
    {
    public:
        friend class Material;

        MaterialInstance(const char* name, Material* parent);
        ~MaterialInstance();

        GfxMaterialInstance* getGfxMaterialInstance() { return m_gfxMaterialInstance.get(); }

        void setInt(const char* parameterName, i32 data);
        void setUint(const char* parameterName, u32 data);
        void setFloat(const char* parameterName, f32 data);
        void setVec2(const char* parameterName, const glm::vec2& data);
        void setVec3(const char* parameterName, const glm::vec3& data);
        void setVec4(const char* parameterName, const glm::vec4& data);
        void setTexture(const char* parameterName, Texture2D* texture);

    private:
        Material* m_parent = nullptr;
        std::unique_ptr<GfxMaterialInstance> m_gfxMaterialInstance = nullptr;
    };
};