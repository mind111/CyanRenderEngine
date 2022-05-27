#include "RenderableScene.h"

#include "Scene.h"
#include "CyanRenderer.h"

namespace Cyan
{
    RenderableScene::RenderableScene(Scene* scene, const SceneView& sceneView)
    {
        // build list of mesh instances and transforms
        for (auto entity : sceneView.entities)
        {
            visitEntity(entity, [this](SceneComponent* sceneComp) {
                    if (auto meshInst = sceneComp->getAttachedMesh())
                    {
                        meshInstances.push_back(meshInst);
                        worldTransformMatrices.push_back(sceneComp->getWorldTransformMatrix());
                    }
                });
        }

        // build lighting data
        skybox = scene->m_skybox;
        irradianceProbe = scene->m_irradianceProbe;
        reflectionProbe = scene->m_reflectionProbe;
        for (u32 i = 0; i < scene->dLights.size(); ++i)
        {
            scene->dLights[i].update();
            directionalLights.emplace_back(scene->dLights[i].getData());
        }
        for (u32 i = 0; i < scene->pointLights.size(); ++i)
        {
            scene->pointLights[i].update();
            pointLights.emplace_back(scene->pointLights[i].getData());
        }

        // build gpu buffers
        glCreateBuffers(1, &directionalLightSbo);
        glNamedBufferData(directionalLightSbo, sizeofVector(directionalLights), directionalLights.data(), GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &pointLightSbo);
        glNamedBufferData(pointLightSbo, sizeofVector(pointLights), pointLights.data(), GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &worldTransformMatrixSbo);
        glNamedBufferData(worldTransformMatrixSbo, sizeofVector(worldTransformMatrices), worldTransformMatrices.data(), GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &viewSsbo);
        glNamedBufferData(viewSsbo, sizeof(view), &view, GL_DYNAMIC_DRAW);
    }

    /**
    * Submit rendering data to global gpu buffers
    */
    void RenderableScene::submitSceneData(GfxContext* ctx)
    {
        // bind global ssbo
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)SceneBufferBindings::kViewData, viewSsbo);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)SceneBufferBindings::TransformMatrices, worldTransformMatrixSbo);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)SceneBufferBindings::DirLightData, directionalLightSbo);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)SceneBufferBindings::PointLightsData, pointLightSbo);

        // shared BRDF lookup texture used in split sum approximation for image based lighting
        if (Texture* BRDFLookupTexture = ReflectionProbe::getBRDFLookupTexture())
        {
            ctx->setPersistentTexture(BRDFLookupTexture, (u32)SceneTextureBindings::BRDFLookupTexture);
        }

        // skybox
        if (skybox)
        {
            ctx->setPersistentTexture(skybox->getDiffueTexture(), 0);
            ctx->setPersistentTexture(skybox->getSpecularTexture(), 1);
        }

        // additional light probes
        if (irradianceProbe)
        {
            ctx->setPersistentTexture(irradianceProbe->m_convolvedIrradianceTexture, 3);
        }
        if (reflectionProbe)
        {
            ctx->setPersistentTexture(reflectionProbe->m_convolvedReflectionTexture, 4);
        }
    }
}