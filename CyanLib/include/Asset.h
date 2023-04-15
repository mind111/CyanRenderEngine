#pragma once

#include <memory>
#include <atomic>

#include "Common.h"

namespace Cyan
{
    struct GUID
    {
        // todo: implement
        u64 id;
    };

    struct Asset
    {
        enum class State : u32
        {
            kUnloaded = 0,
            kLoading,
            kLoaded,
            kUninitialized,
            kInitialized,
            kInvalid
        };

        Asset(const char* inName)
            : name(inName), handle(-1), state(State::kUnloaded)
        {
        }

        virtual const char* getAssetTypeName() { return "Asset"; }

        virtual ~Asset() { }
        virtual void import() { }
        virtual void load() = 0;
        virtual void onLoaded() { }
        virtual void unload() = 0;

        bool isLoaded() { return state >= State::kLoaded; }
        bool isInited() { return state == State::kInitialized; }

        // unique name identifier
        std::string name;
        std::string path;
        // unique handle
        u64 handle;
        std::atomic<State> state = State::kInvalid;
    };
}
