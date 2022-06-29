#include "RenderableScene.h"

#include "Scene.h"
#include "CyanRenderer.h"
#include "LightComponent.h"

namespace Cyan
{
    SceneRenderable::SceneRenderable(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator)
        : scene(inScene)
    {
#if 1
        viewSsboPtr = std::make_unique<ViewSsbo>();
        transformSsboPtr = std::make_unique<TransformSsbo>(256);
#endif
        // build list of mesh instances and transforms
        for (auto entity : sceneView.entities)
        {
            TransformSsbo& transformSsbo = *(transformSsboPtr.get());
#if 1
            if (auto staticMeshEntity = dynamic_cast<StaticMeshEntity*>(entity))
            {
                meshInstances.push_back(staticMeshEntity->getMeshInstance());
                ADD_SSBO_DYNAMIC_ELEMENT(transformSsbo, staticMeshEntity->getWorldTransformMatrix());
            }
#else
            entity->visit([this, &transformSsbo](SceneComponent* sceneComp) {
                    if (auto meshInst = sceneComp->getAttachedMesh())
                    {
                        meshInstances.push_back(meshInst);
#if 1
                        ADD_SSBO_DYNAMIC_ELEMENT(transformSsbo, sceneComp->getWorldTransformMatrix());
#endif
                    }
                });
#endif
        }

#if 1
        // build view data
        ViewSsbo& viewSsbo = *(viewSsboPtr.get());
        SET_SSBO_STATIC_MEMBER(viewSsbo, view, sceneView.camera.view);
        SET_SSBO_STATIC_MEMBER(viewSsbo, projection, sceneView.camera.projection);
#endif

        // build lighting data
        skybox = scene->skybox;
        irradianceProbe = scene->irradianceProbe;
        reflectionProbe = scene->reflectionProbe;

        // todo: this following code is causing memory leak
        // build a list of lights in the scene
#if 1
        for (auto entity : scene->entities)
        {
            auto lightComponents = entity->getComponent<ILightComponent>();
            if (!lightComponents.empty())
            {
                for (auto lightComponent : lightComponents)
                {
                    /* note:
                        using a custom deleter since `allocator` used in this case is using placement new, need to explicitly invoke
                        destructor to avoid memory leaking as the object being constructed using placement new might heap allocate its members.
                        Thus, need to invoke destructor to make sure those heap allocated members are released, no need to free memory since the allocator's
                        memory will be reset and reused each frame.
                    */
                    lights.push_back(std::shared_ptr<ILightRenderable>(lightComponent->buildRenderableLight(allocator), [](ILightRenderable* lightToDelete) { 
                            lightToDelete->~ILightRenderable();
                        })
                    );
                }
            }
        }
#endif
        viewSsboPtr->update();
        transformSsboPtr->update();
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