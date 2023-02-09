#pragma once

namespace Cyan
{
    struct GUID
    {
        // todo: implement
    };

    struct Asset
    {
        struct InternalSource
        {
            // todo: implement
        };

        struct ExternalSource
        {
            enum State
            {
                kLoaded = 0,
                kUnloaded
            };

            const char* filename = nullptr;
            State state = kUnloaded;

            virtual void load() = 0;
            virtual void unload() = 0;
        };

        Asset(const char* inName) 
            : name(inName)
        {
        }

        Asset(std::shared_ptr<ExternalSource> inExternalSource, const char* inName) 
            : externalSource(inExternalSource), name(inName)
        {
        }

        virtual ~Asset() { }

        virtual void import() = 0;
        virtual void reimport() = 0;

        virtual void load() = 0;
        virtual void unload() = 0;
        virtual void save() = 0;

        std::shared_ptr<ExternalSource> externalSource = nullptr;
        std::shared_ptr<InternalSource> internalSource = nullptr;

        // unique name identifier
        std::string name;
        // unique handle
        GUID handle;
    };
}
