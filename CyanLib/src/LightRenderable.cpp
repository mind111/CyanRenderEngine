#include "LightRenderable.h"
#include "Scene.h"
#include "CyanRenderer.h"
#include "RenderableScene.h"

namespace Cyan
{
    void DirectionalLightRenderable::renderShadow(const Scene& scene, const RenderableScene& renderableScene, Renderer& renderer)
    {
        /**
        * This is a bit clunky but I want to make CSM and vanilla DirectionalShadowMap to support same interface
        */
        BoundingBox3D lightSpaceAABB = calcLightSpaceAABB(direction, scene.aabb);
        OrthographicCamera camera(
            glm::vec3(0.f),
            -direction,
            glm::vec3(0.f, 1.f, 0.f),
            lightSpaceAABB
        );

        shadowmapPtr->render(&camera, scene, renderableScene, renderer);
    }
}