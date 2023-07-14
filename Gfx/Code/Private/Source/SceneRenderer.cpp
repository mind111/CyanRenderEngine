#include "SceneRender.h"
#include "SceneRenderer.h"

namespace Cyan
{

    SceneRenderer::SceneRenderer()
    {

    }

    SceneRenderer::~SceneRenderer() { }

    void SceneRenderer::render(ISceneRender* outRender, IScene* scene, const SceneViewInfo& viewInfo)
    {
        // render depth prepass
    }

    // todo: implement this
    // 1. get Shader to work
    void SceneRenderer::renderSceneDepth(GHTexture2D* outDepthTex, Scene* scene, const SceneViewInfo& viewInfo)
    {
        if (outDepthTex != nullptr)
        {
        }
    }
}