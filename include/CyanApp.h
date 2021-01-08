#pragma once

#include "Common.h"
#include "CyanEngine.h" 

class CyanApp
{
public:
    CyanApp() { gEngine =  new CyanEngine(); }
    ~CyanApp() { }

    virtual void init(int appWindowWidth, int appWindowHeight) = 0;

    virtual void beginFrame() = 0;
    virtual void render() = 0;
    virtual void endFrame() = 0;

    virtual void run() = 0;
    virtual void shutDown() = 0;

protected:
    CyanEngine* gEngine;
};