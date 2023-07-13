#include "GfxInterface.h"

// private implementations
#include "Scene.h"
#include "SceneRenderer.h"

namespace Cyan
{
    std::unique_ptr<IScene> IScene::create()
    {
        return std::move(std::make_unique<Scene>());
    }

    std::unique_ptr<ISceneRenderer> ISceneRenderer::create()
    {
        return std::move(std::make_unique<SceneRenderer>());
    }
}