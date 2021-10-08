#pragma once

#include "Scene.h"
#include "Texture.h"
#include "Asset.h"
#include "CyanRenderer.h"

namespace Cyan
{
    class GraphicsSystem
    {
    public:

        GraphicsSystem(glm::vec2& windowSize);
        ~GraphicsSystem() { }

        static GraphicsSystem* getSingletonPtr();
        Renderer* getRenderer();
        TextureManager* getTextureManager();
        AssetManager* getAssetManager();
        void initialize();

    private:
        glm::vec2 m_windowSize; 
        SceneManager* m_sceneManager;
        TextureManager* m_textureManager;
        AssetManager* m_assetManager;
        Renderer*     m_renderer;
        static GraphicsSystem* m_singleton;
    };
}