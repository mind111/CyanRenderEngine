#pragma once

/**
 * note: this file includes all the interfaces exposed by the Gfx module
 */

#include <memory>

#include "Gfx.h"

namespace Cyan
{
    class IScene
    {
    public:
        virtual ~IScene() { }

        // factory method
        GFX_API static std::unique_ptr<IScene> create();
    };

    class ISceneRenderer
    {
    public:
        virtual ~ISceneRenderer() { }

        // factory method
        GFX_API static std::unique_ptr<ISceneRenderer> create();
    };
}