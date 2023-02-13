#pragma once

#include "Asset.h"

namespace Cyan
{
    /** 
    * Encapsulation of an external asset file such as .gltf, .glb, and .obj and so on ...
    */
    struct ExternalAssetFile
    {
        enum class State
        {
            kLoaded = 0,
            kUnloaded
        };

        ExternalAssetFile(const char* inFilename)
            : filename(inFilename), state(State::kUnloaded)
        {}

        const char* filename = nullptr;
        State state = State::kUnloaded;

        virtual void load() = 0;
        virtual void unload() = 0;
    };
}
