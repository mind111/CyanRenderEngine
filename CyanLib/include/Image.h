#pragma once

#include <memory>
#include <string>
#include <Mutex>

#include "Asset.h"
#include "Common.h"

namespace Cyan
{
    struct Image : public Asset
    {
        struct Listener
        {
            Listener(Image* inBroadcaster)
                : broadcaster(inBroadcaster)
            { 
                if (broadcaster)
                {
                    broadcaster->addListener(this);
                }
            }

            virtual ~Listener() 
            {
                if (broadcaster)
                {
                    broadcaster->removeListener(this);
                }
            }

            virtual bool operator==(const Listener& rhs) = 0;
            virtual void onImageLoaded() { };

            Image* broadcaster = nullptr;
        };

        Image(const char* name);

        /* Asset Interface */
        virtual const char* getAssetTypeName() { return "Image"; }

        virtual ~Image() { }
        virtual void import();
        virtual void load() override { }
        virtual void onLoaded() override;
        virtual void unload() { }

        void addListener(Listener* listener);
        void removeListener(Listener* toRemove);

        i32 width;
        i32 height;
        i32 numChannels;
        i32 bitsPerChannel;
        std::shared_ptr<u8> pixels = nullptr;
        std::mutex listenersMutex;
        std::vector<Listener*> listeners;
    };
}
