#include "LightRenderable.h"
#include "Scene.h"
#include "CyanRenderer.h"

namespace Cyan
{
    void DirectionalLightRenderable::renderShadow(const Scene& scene, Renderer& renderer)
    {
        shadowmapPtr->render(scene, scene.aabb, renderer);
    }
}