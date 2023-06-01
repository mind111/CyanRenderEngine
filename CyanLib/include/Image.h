#pragma once

#include <memory>
#include <string>
#include <Mutex>

#include "Asset.h"
#include "Common.h"

namespace Cyan
{
    class Image : public Asset
    {
    public:
        class Listener
        {
        public:
            Listener(Image* topic) 
                : m_topic(topic)
            { }

            virtual ~Listener() 
            {
                if (m_topic != nullptr)
                {
                    m_topic->removeListener(this);
                    m_topic = nullptr;
                }
            }

            virtual bool operator==(const Listener& rhs) = 0;
            virtual void onImageLoaded() { }
            virtual void subscribe() 
            {
                m_topic->addListener(this);
            }

            Image* m_topic = nullptr;
        };

        Image(const char* name);

        static const char* getAssetTypeName() { return "Image"; }

        virtual ~Image() { }
        virtual void load() override { }
        virtual void onLoaded() override;
        virtual void unload() { }

        void addListener(Listener* listener);
        void removeListener(Listener* toRemove);

        i32 m_width;
        i32 m_height;
        i32 m_numChannels;
        i32 m_bitsPerChannel;
        std::shared_ptr<u8> m_pixels = nullptr;
        std::mutex m_listenersMutex;
        std::vector<Listener*> m_listeners;
    };
}
