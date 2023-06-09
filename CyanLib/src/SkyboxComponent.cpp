#include "SkyboxComponent.h"
#include "SkyBox.h"
#include "AssetImporter.h"
#include "World.h"
#include "Scene.h"

namespace Cyan
{
    SkyboxComponent::SkyboxComponent(const char* name, const Transform& local)
        : SceneComponent(name, local)
    {
        std::string imageName = name + std::string("HDRIImage");
        auto HDRIImage = AssetImporter::importImage(imageName.c_str(), defaultHDRIPath);
        Sampler2D sampler = { };
        sampler.magFilter = FM_BILINEAR;
        sampler.minFilter = FM_TRILINEAR;
        sampler.wrapS = WM_CLAMP;
        sampler.wrapT = WM_CLAMP;
        m_HDRI = AssetManager::createTexture2D("SkyboxHDRI", HDRIImage, sampler);
        m_skybox.reset(new Skybox(this));
    }

    SkyboxComponent::~SkyboxComponent()
    {

    }

    void SkyboxComponent::setOwner(Entity* owner)
    {
        Scene* scene = owner->getWorld()->getScene();
        scene->addSkybox(m_skybox.get());
    }
}