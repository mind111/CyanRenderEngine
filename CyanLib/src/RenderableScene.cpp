#include "RenderableScene.h"

#include "Scene.h"
#include "CyanRenderer.h"

namespace Cyan
{
    RenderableScene::RenderableScene(Scene* scene, const SceneView& sceneView)
    {
        viewSsboPtr = std::make_unique<ViewSsbo>();
        transformSsboPtr = std::make_unique<TransformSsbo>();
        directionalLightSsboPtr = std::make_unique<DirectionalLightSsbo>(1);
        pointLightSsboPtr = std::make_unique<PointLightSsbo>(20);

        // build list of mesh instances and transforms
        for (auto entity : sceneView.entities)
        {
            TransformSsbo& transformSsbo = *(transformSsboPtr.get());

            visitEntity(entity, [this, &transformSsbo](SceneComponent* sceneComp) {
                    if (auto meshInst = sceneComp->getAttachedMesh())
                    {
                        meshInstances.push_back(meshInst);
                        ADD_SSBO_DYNAMIC_ELEMENT(transformSsbo, sceneComp->getWorldTransformMatrix());
                    }
                });
        }

        // build view data
        ViewSsbo& viewSsbo = *(viewSsboPtr.get());
        SET_SSBO_STATIC_MEMBER(viewSsbo, view, scene->camera.view);
        SET_SSBO_STATIC_MEMBER(viewSsbo, projection, scene->camera.projection);
        SET_SSBO_STATIC_MEMBER(viewSsbo, numDirLights, scene->dLights.size());
        SET_SSBO_STATIC_MEMBER(viewSsbo, numPointLights, scene->pointLights.size());

        // build lighting data
        skybox = scene->m_skybox;
        irradianceProbe = scene->m_irradianceProbe;
        reflectionProbe = scene->m_reflectionProbe;

        DirectionalLightSsbo directionalLightSsbo = *(directionalLightSsboPtr.get());
        PointLightSsbo pointLightSsbo = *(pointLightSsboPtr.get());

        for (u32 i = 0; i < scene->dLights.size(); ++i)
        {
            scene->dLights[i].update();
            SET_SSBO_DYNAMIC_ELEMENT(directionalLightSsbo, i, scene->dLights[i].getData());
        }
        for (u32 i = 0; i < scene->pointLights.size(); ++i)
        {
            scene->pointLights[i].update();
            SET_SSBO_DYNAMIC_ELEMENT(pointLightSsbo, i, scene->pointLights[i].getData());
        }

        viewSsbo.update();
        directionalLightSsbo.update();
        pointLightSsbo.update();
    }

    /**
    * Submit rendering data to global gpu buffers
    */
    void RenderableScene::submitSceneData(GfxContext* ctx)
    {
        // bind global ssbo
        viewSsboPtr->bind((u32)SceneBufferBindings::kViewData);
        transformSsboPtr->bind((u32)SceneBufferBindings::TransformMatrices);
        directionalLightSsboPtr->bind((u32)SceneBufferBindings::DirLightData);
        pointLightSsboPtr->bind((u32)SceneBufferBindings::PointLightsData);

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