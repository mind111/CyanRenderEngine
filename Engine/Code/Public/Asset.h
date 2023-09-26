#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "Core.h"
#include "Engine.h"

namespace Cyan
{
    struct GUID
    {
        // todo: implement
        u64 id;
    };

    // todo:
    class ENGINE_API Asset
    {
    public:
        enum class State : u32
        {
            kUnloaded = 0,
            kLoading,
            kLoaded,
            kInitialized,
            kInvalid
        };

        Asset(const char* name)
            : m_name(name), m_handle(-1), m_state(State::kUnloaded)
        {
        }
        virtual ~Asset() { }

        virtual void load() { }
        virtual void onLoaded() { }
        virtual void unload() { }
        virtual void save() { }

        static const char* getAssetTypeName() { return "Asset"; }

        const std::string& getName() { return m_name; }
        bool isLoaded() { return m_state >= State::kLoaded; }
        bool isInited() { return m_state == State::kInitialized; }

    private:
        // unique name identifier
        std::string m_name;
        std::string m_path;
        // unique handle
        u64 m_handle;
        std::atomic<State> m_state = State::kInvalid;
    };
}
