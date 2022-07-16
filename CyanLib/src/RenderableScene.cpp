#include "RenderableScene.h"

#include "Camera.h"
#include "Scene.h"
#include "CyanRenderer.h"
#include "LightComponent.h"
#include "LightRenderable.h"

namespace Cyan
{
    RenderableScene::RenderableScene(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator)
        : camera{ sceneView.camera->view(), sceneView.camera->projection() }
    {
        transformSsboPtr = std::make_shared<TransformSsbo>(256);
        TransformSsbo& transformSsbo = *(transformSsboPtr.get());

        // build list of mesh instances, transforms, and lights
        for (auto entity : sceneView.entities)
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

            // static meshes
            if (auto staticMesh = dynamic_cast<StaticMeshEntity*>(entity))
            {
                meshInstances.push_back(staticMesh->getMeshInstance());
                ADD_SSBO_DYNAMIC_ELEMENT(transformSsbo, staticMesh->getWorldTransformMatrix());
            }
        }

        // build view data
        viewSsboPtr = std::make_shared<ViewSsbo>();
        ViewSsbo& viewSsbo = *(viewSsboPtr.get());
        SET_SSBO_STATIC_MEMBER(viewSsbo, view, camera.view);
        SET_SSBO_STATIC_MEMBER(viewSsbo, projection, camera.projection);

        // build lighting data
        skybox = inScene->skybox;
        irradianceProbe = inScene->irradianceProbe;
        reflectionProbe = inScene->reflectionProbe;
    }

    /**
    * Submit rendering data to global gpu buffers
    */
    void RenderableScene::submitSceneData(GfxContext* ctx) const
    {
        // todo: avoid repetitively submit redundant data
        {

        }

        // bind global ssbo
        ViewSsbo& viewSsbo = *(viewSsboPtr.get());
        SET_SSBO_STATIC_MEMBER(viewSsbo, view, camera.view);
        SET_SSBO_STATIC_MEMBER(viewSsbo, projection, camera.projection);
        viewSsboPtr->update();
        transformSsboPtr->update();
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