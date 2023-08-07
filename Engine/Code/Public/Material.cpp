#include "Material.h"
#include "GfxMaterial.h"
#include "GfxModule.h"

namespace Cyan 
{
    Material::Material(const char* name, const char* materialSourcePath, const MaterialSetupFunc& setupDefaultInstance)
        : Asset(name)
        , m_materialSourcePath(materialSourcePath)
        , m_defaultInstanceSetupFunc(setupDefaultInstance)
    {
        std::string taskName = "CreateGfxMaterial: " + getName();

        // create gfx material
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            taskName.c_str(),
            [this](Frame& frame) {
                m_gfxMaterial = std::make_unique<GfxMaterial>(getName().c_str(), m_materialSourcePath);
            }
        );

        // create and setup the default instance
        std::string instanceName = "Default_" + getName();
        m_defaultInstance = std::make_unique<MaterialInstance>(instanceName.c_str(), this);
        m_defaultInstanceSetupFunc(this);
    }

    Material::~Material()
    {

    }

    MaterialInstance* Material::createInstance(const char* instanceName)
    {
        return new MaterialInstance(instanceName, this);
    }

    void Material::removeInstance(const char* instanceName)
    {
        // todo: implement this
    }

    void Material::setInt(const char* parameterName, i32 data)
    {
        m_defaultInstance->setInt(parameterName, data);
    }

    void Material::setUint(const char* parameterName, u32 data)
    {
        m_defaultInstance->setUint(parameterName, data);
    }

    void Material::setFloat(const char* parameterName, f32 data)
    {
        m_defaultInstance->setFloat(parameterName, data);
    }

    void Material::setVec2(const char* parameterName, const glm::vec2& data)
    {
        m_defaultInstance->setVec2(parameterName, data);
    }

    void Material::setVec3(const char* parameterName, const glm::vec3& data)
    {
        m_defaultInstance->setVec3(parameterName, data);
    }

    void Material::setVec4(const char* parameterName, const glm::vec4& data)
    {
        m_defaultInstance->setVec4(parameterName, data);
    }

    void Material::setTexture(const char* parameterName, Texture2D* texture)
    {
        m_defaultInstance->setTexture(parameterName, texture);
    }

    MaterialInstance::MaterialInstance(const char* name, Material* parent)
        : Asset(name), m_parent(parent)
    {
        std::string taskName = "CreateGfxMaterialInstance: " + getName();
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            taskName.c_str(),
            [this](Frame& frame) {
                m_gfxMaterialInstance = std::make_unique<GfxMaterialInstance>(getName().c_str(), m_parent->m_gfxMaterial.get());
            }
        );
    }

    MaterialInstance::~MaterialInstance()
    {

    }

    void MaterialInstance::setInt(const char* parameterName, i32 data)
    {
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "MaterialInstance::setInt",
            [this, parameterName, data](Frame& frame) {
                m_gfxMaterialInstance->setInt(parameterName, data);
            }
        );
    }

    void MaterialInstance::setUint(const char* parameterName, u32 data)
    {
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "MaterialInstance::setUint",
            [this, parameterName, data](Frame& frame) {
                m_gfxMaterialInstance->setUint(parameterName, data);
            }
        );
    }

    void MaterialInstance::setFloat(const char* parameterName, f32 data)
    {
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "MaterialInstance::setFloat",
            [this, parameterName, data](Frame& frame) {
                m_gfxMaterialInstance->setFloat(parameterName, data);
            }
        );
    }

    void MaterialInstance::setVec2(const char* parameterName, const glm::vec2& data)
    {
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "MaterialInstance::setVec2",
            [this, parameterName, data](Frame& frame) {
                m_gfxMaterialInstance->setVec2(parameterName, data);
            }
        );
    }

    void MaterialInstance::setVec3(const char* parameterName, const glm::vec3& data)
    {
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "MaterialInstance::setVec3",
            [this, parameterName, data](Frame& frame) {
                m_gfxMaterialInstance->setVec3(parameterName, data);
            }
        );
    }

    void MaterialInstance::setVec4(const char* parameterName, const glm::vec4& data)
    {
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "MaterialInstance::setVec4",
            [this, parameterName, data](Frame& frame) {
                m_gfxMaterialInstance->setVec4(parameterName, data);
            }
        );
    }

    void MaterialInstance::setTexture(const char* parameterName, Texture2D* texture)
    {
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "MaterialInstance::setTexture",
            [this, parameterName, texture](Frame& frame) {
                m_gfxMaterialInstance->setTexture(parameterName, texture->getGHTexture());
            }
        );
    }
}