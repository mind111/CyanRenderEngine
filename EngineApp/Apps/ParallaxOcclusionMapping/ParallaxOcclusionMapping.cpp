#pragma once

#include "ParallaxOcclusionMapping.h"
#include "World.h"
#include "SceneCameraEntity.h"
#include "SceneCameraComponent.h"
#include "CameraControllerComponent.h"
#include "LightEntities.h"
#include "AssetManager.h"
#include "GfxHardwareAbstraction/GHInterface/GHTexture.h"
#include "SkyboxEntity.h"
#include "SkyboxComponent.h"
#include "LightComponents.h"
#include "Material.h"
#include "StaticMeshEntity.h"
#include "StaticMeshComponent.h"

namespace Cyan
{

#define APP_ASSET_PATH "C:/dev/cyanRenderEngine/EngineApp/Apps/ParallaxOcclusionMapping/Resources/"

    ParallaxOcclusionMapping::ParallaxOcclusionMapping(u32 windowWidth, u32 windowHeight)
        : App("ParallaxOcclusionMapping", windowWidth, windowHeight)
    {

    }

    ParallaxOcclusionMapping::~ParallaxOcclusionMapping()
    {

    }

    void ParallaxOcclusionMapping::update(World* world)
    {

    }

    void ParallaxOcclusionMapping::render()
    {

    }

    // todo: 
    // [x] skybox
    // [x] skylight
    // [x] basic POM
    //     [] intersection & aliasing improvements
    //     [] performance improvements
    //     [] writing pixel depth of the intersection to the depth buffer
    // [x] ao
    // [] restir ssgi
    //     [x] hiz tracing
    //     [x] naive diffuse 
    //     [x] naive SSDO 
    //     [x] restir SSDO 
    //         [x] hemisphere sampling pass (temporal pass)
    //         [x] spatial reuse
    //     [] restir diffuse indirect 

    // [] better tone mapper
    // [] auto exposure
    // [] taa
    void ParallaxOcclusionMapping::customInitialize(World* world)
    {
        const char* sceneFilePath = APP_ASSET_PATH "Scene.glb";
        world->import(sceneFilePath);

        GHSampler2D sampler; 
        sampler.setAddressingModeX(SamplerAddressingMode::Wrap);
        sampler.setAddressingModeY(SamplerAddressingMode::Wrap);
        sampler.setFilteringModeMin(Sampler2DFilteringMode::Trilinear);
        sampler.setFilteringModeMag(Sampler2DFilteringMode::Bilinear);

        // todo: manually load textures and setup materials
        {
            auto brickAlbedoTex = AssetManager::createTexture2DFromImage(APP_ASSET_PATH "brick_albedo.jpg", "Brick_Albedo", sampler);
            auto brickNormalTex = AssetManager::createTexture2DFromImage(APP_ASSET_PATH "brick_normal.jpg", "Brick_Normal", sampler);
            auto brickRoughnessTex = AssetManager::createTexture2DFromImage(APP_ASSET_PATH "brick_roughness.jpg", "Brick_Roughness", sampler);
            auto brickOcclusionTex = AssetManager::createTexture2DFromImage(APP_ASSET_PATH "brick_ao.jpg", "Brick_Occlusion", sampler);

            GHSampler2D heightSampler;
            heightSampler.setAddressingModeX(SamplerAddressingMode::Wrap);
            heightSampler.setAddressingModeY(SamplerAddressingMode::Wrap);
            heightSampler.setFilteringModeMin(Sampler2DFilteringMode::Trilinear);
            heightSampler.setFilteringModeMag(Sampler2DFilteringMode::Bilinear);
            auto brickHeightTex = AssetManager::createTexture2DFromImage(APP_ASSET_PATH "brick_height.jpg", "Brick_Height", heightSampler);

            auto POMMateiral = AssetManager::createMaterial(
                "M_POM", 
                APP_ASSET_PATH "Materials/M_POM_p.glsl", 
                [brickAlbedoTex, brickNormalTex, brickRoughnessTex, brickOcclusionTex, brickHeightTex](Material* m) {
                    m->setTexture("mp_albedoMap", brickAlbedoTex);
                    m->setTexture("mp_normalMap", brickNormalTex);
                    m->setTexture("mp_roughnessMap", brickRoughnessTex);
                    m->setTexture("mp_occlusionMap", brickOcclusionTex);
                    m->setTexture("mp_heightMap", brickHeightTex);
                }
            );

            auto brickWallMatl = AssetManager::createMaterial(
                "M_BrickWall", 
                APP_ASSET_PATH "Materials/M_BrickWall_p.glsl", 
                [brickAlbedoTex, brickNormalTex, brickRoughnessTex, brickOcclusionTex](Material* m) {
                    m->setTexture("mp_albedoMap", brickAlbedoTex);
                    m->setTexture("mp_normalMap", brickNormalTex);
                    m->setTexture("mp_roughnessMap", brickRoughnessTex);
                    m->setTexture("mp_occlusionMap", brickOcclusionTex);
                }
            );

            auto shaderball_0 = world->findEntity<StaticMeshEntity>("shaderBall");
            shaderball_0->getStaticMeshComponent()->setMaterial(0, brickWallMatl->getDefaultInstance());
            // shaderball_0->getStaticMeshComponent()->setMaterial(4, brickWallMatl->getDefaultInstance());
            shaderball_0->getStaticMeshComponent()->setMaterial(4, POMMateiral->getDefaultInstance());

            auto plane = world->findEntity<StaticMeshEntity>("Plane.001");
            plane->getStaticMeshComponent()->setMaterial(-1, POMMateiral->getDefaultInstance());
        }

        {
            auto albedo = AssetManager::createTexture2DFromImage(APP_ASSET_PATH "tile_albedo.jpg", "SwimmingTile_Albedo", sampler);
            auto normal = AssetManager::createTexture2DFromImage(APP_ASSET_PATH "tile_normal.jpg", "SwimmingTile_Normal", sampler);
            auto roughness = AssetManager::createTexture2DFromImage(APP_ASSET_PATH "tile_roughness.jpg", "SwimmingTile_Roughness", sampler);
            auto occlusion = AssetManager::createTexture2DFromImage(APP_ASSET_PATH "tile_ao.jpg", "SwimmingTile_Occlusion", sampler);

            auto matl = AssetManager::createMaterial(
                "M_SwimmingTile", 
                APP_ASSET_PATH "Materials/M_SwimmingTile_p.glsl", 
                [albedo, normal, roughness, occlusion](Material* m) {
                    m->setTexture("mp_albedoMap", albedo);
                    m->setTexture("mp_normalMap", normal);
                    m->setTexture("mp_roughnessMap", roughness);
                    m->setTexture("mp_occlusionMap", occlusion);
                }
            );
            auto shaderball_1 = world->findEntity<StaticMeshEntity>("shaderBall.001");
            shaderball_1->getStaticMeshComponent()->setMaterial(0, matl->getDefaultInstance());
            shaderball_1->getStaticMeshComponent()->setMaterial(4, matl->getDefaultInstance());

            auto plane = world->findEntity<StaticMeshEntity>("Plane.003");
            plane->getStaticMeshComponent()->setMaterial(-1, matl->getDefaultInstance());
        }
    }
}
