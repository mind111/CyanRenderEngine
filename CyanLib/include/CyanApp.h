#pragma once

#include "Common.h"
#include "CyanEngine.h" 

class CyanApp
{
public:
    CyanApp() { gEngine =  new CyanEngine(); }
    ~CyanApp() { }

    virtual void initialize(int appWindowWidth, int appWindowHeight, glm::vec2 sceneViewportPos, glm::vec2 renderSize) = 0;

    virtual void run() = 0;
    virtual void render() = 0;
    virtual void shutDown() = 0;

protected:
    CyanEngine* gEngine;
};