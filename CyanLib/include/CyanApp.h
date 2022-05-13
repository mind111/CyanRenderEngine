#pragma once

#include "Common.h"
#include "CyanEngine.h" 
#include "Scene.h"

namespace Cyan
{
    class CyanApp
    {
    public:
        CyanApp();
        virtual ~CyanApp() { }

        void initialize();
        void finalize();
        void update();
        void render();
        void run();

        virtual void initialize(int appWindowWidth, int appWindowHeight, glm::vec2 sceneViewportPos, glm::vec2 renderSize) = 0;

        virtual void run() = 0;
        virtual void render() = 0;
        virtual void shutDown() = 0;

    protected:
        CyanEngine* gEngine;
    };
}