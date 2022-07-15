#include "RenderableScene.h"

#include "Scene.h"
#include "CyanRenderer.h"
#include "LightComponent.h"

namespace Cyan
{
    RenderableScene::RenderableScene(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator)
        : scene(inScene)
    {
        viewSsboPtr = std::make_unique<ViewSsbo>();
        transformSsboPtr = std::make_unique<TransformSsbo>(256);
        TransformSsbo& transformSsbo = *(transformSsboPtr.get());

        // build list of mesh instances and transforms
        for (auto entity : sceneView.entities)
        {
            if (auto camera = dynamic_cast<CameraEntity*>(entity))
            {
                this->camera = camera->getCamera();
            }
            // static meshes
            else if (auto staticMesh = dynamic_cast<StaticMeshEntity*>(entity))
            {
                meshInstances.push_back(staticMesh->getMeshInstance());
                ADD_SSBO_DYNAMIC_ELEMENT(transformSsbo, staticMesh->getWorldTransformMatrix());
            }
        }

        // build view data
        ViewSsbo& viewSsbo = *(viewSsboPtr.get());
        SET_SSBO_STATIC_MEMBER(viewSsbo, view, camera->view());
        SET_SSBO_STATIC_MEMBER(viewSsbo, projection, camera->projection());

        // build lighting data
        skybox = scene->skybox;
        irradianceProbe = scene->irradianceProbe;
        reflectionProbe = scene->reflectionProbe;

        // todo: this following code is causing memory leak
        // build a list of lights in the scene
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
        viewSsboPtr->update();
        transformSsboPtr->update();
    }

    /**
    * Submit rendering data to global gpu buffers
    */
    void RenderableScene::submitSceneData(GfxContext* ctx)
    {
        // todo: avoid repetively submit redundant data
        {

        }
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