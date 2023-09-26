#pragma once

#include "SkyboxComponent.h"
#include "Engine.h"
#include "GfxModule.h"
#include "Texture.h"
#include "Skybox.h"
#include "Entity.h"
#include "World.h"
#include "Scene.h"

namespace Cyan
{
    SkyboxComponent::SkyboxComponent(const char* name, const Transform& localTransform)
        : SceneComponent(name, localTransform)
    {
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "CreateSkybox",
            [this](Frame& frame) {
                m_skybox = std::make_unique<Skybox>(1024);
            }
        );
    }

    SkyboxComponent::~SkyboxComponent() { }

    void SkyboxComponent::setOwner(Entity* owner)
    {
        // update owner
        SceneComponent::setOwner(owner);

        World* world = owner->getWorld();
        if (world != nullptr)
        {
            Scene* scene = world->getScene();
            if (scene != nullptr)
            {
                Engine::get()->enqueueFrameGfxTask(
                    RenderingStage::kPreSceneRendering,
                    "SetSkybox", 
                    [scene, this](Frame& frame) {
                        if (m_skybox != nullptr)
                        {
                            scene->setSkybox(m_skybox.get());
                        }
                    }
                );
            }
        }
    }

    void SkyboxComponent::setHDRI(Texture2D* HDRI)
    {
        if (m_HDRI == nullptr)
        {
            if (HDRI != nullptr)
            {
                m_HDRI = HDRI;

                Engine::get()->enqueueFrameGfxTask(
                    RenderingStage::kPreSceneRendering,
                    "BuildSkyboxCubemapFromHDRI",
                    [this](Frame& frame) {
                        assert(m_skybox != nullptr);
                        m_skybox->buildFromHDRI(m_HDRI->getGHTexture());
                    }
                );
            }
        }
        else
        {
            if (HDRI != nullptr && (HDRI->getName() != m_HDRI->getName()))
            {
                m_HDRI = HDRI;

                Engine::get()->enqueueFrameGfxTask(
                    RenderingStage::kPreSceneRendering,
                    "BuildSkyboxCubemapFromHDRI",
                    [this](Frame& frame) {
                        assert(m_skybox != nullptr);
                        m_skybox->buildFromHDRI(m_HDRI->getGHTexture());
                    }
                );
            }
        }
    }

    GHTextureCube* SkyboxComponent::getSkyboxCubemap()
    {
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "BuildSkyboxCubemapFromHDRI",
            [this](Frame& frame) {
                assert(m_skybox != nullptr);
                m_skybox->buildFromHDRI(m_HDRI->getGHTexture());
            }
        );
        if (m_skybox != nullptr)
        {
            return m_skybox->getCubemapTexture();
        }
    }
}
