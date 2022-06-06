#include "RenderableScene.h"

#include "Scene.h"
#include "CyanRenderer.h"
#include "LightComponent.h"

namespace Cyan
{
    SceneRenderable::SceneRenderable(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator)
        : scene(inScene)
    {
        viewSsboPtr = std::make_unique<ViewSsbo>();
        transformSsboPtr = std::make_unique<TransformSsbo>(256);

        // build list of mesh instances and transforms
        for (auto entity : sceneView.entities)
        {
            TransformSsbo& transformSsbo = *(transformSsboPtr.get());

            entity->visit([this, &transformSsbo](SceneComponent* sceneComp) {
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

        // build lighting data
        skybox = scene->skybox;
        irradianceProbe = scene->irradianceProbe;
        reflectionProbe = scene->reflectionProbe;

        // todo: experiment with the following design
        // build a list of lights in the scene
        for (auto entity : scene->entities)
        {
            auto lightComponents = entity->getComponent<ILightComponent>();
            if (!lightComponents.empty())
            {
                for (auto lightComponent : lightComponents)
                {
                    lights.push_back(lightComponent->buildRenderableLight(allocator));
                }
            }
        }
#if 0
        for (u32 i = 0; i < scene->pointLights.size(); ++i)
        {
            scene->pointLights[i].update();
            SET_SSBO_DYNAMIC_ELEMENT(pointLightSsbo, i, scene->pointLights[i].getData());
        }
#endif
        viewSsbo.update();
    }

    /**
    * Submit rendering data to global gpu buffers
    */
    void SceneRenderable::submitSceneData(GfxContext* ctx)
    {

        // bind global ssbo
        viewSsboPtr->bind((u32)SceneSsboBindings::kViewData);
        transformSsboPtr->bind((u32)SceneSsboBindings::TransformMatrices);

        // shared BRDF lookup texture used in split sum approximation for image based lighting
        if (Texture2DRenderable* BRDFLookupTexture = ReflectionProbe::getBRDFLookupTexture())
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