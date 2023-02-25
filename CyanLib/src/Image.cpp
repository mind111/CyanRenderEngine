#include <cassert>

#include "Image.h"
#include "stbi/stb_image.h"
#include "AssetImporter.h"

namespace Cyan
{
    Image::Image(const char* inName)
        : Asset(inName)
    {

    }

    void Image::import()
    {
        if (state != State::kLoaded && state != State::kLoading) 
        {
            state = State::kLoading;
            AssetImporter::import(this);
        }
    }

    void Image::onLoaded()
    {
        std::lock_guard<std::mutex> lock(listenersMutex);
        // todo: should listeners be removed once the event is fired? probably not
        for (auto listener : listeners)
        {
            listener->onImageLoaded();
        }
        state = State::kLoaded;
    }

    void Image::addListener(Listener* listener) 
    { 
        std::lock_guard<std::mutex> lock(listenersMutex);
        listeners.push_back(listener); 
    }

    void Image::removeListener(Listener* toRemove) 
    { 
        std::lock_guard<std::mutex> lock(listenersMutex);
        for (i32 i = 0; i < listeners.size(); ++i)
        {
            if (listeners[i] == toRemove)
            {
                listeners.erase(listeners.begin() + i);
                break;
            }
        }
    }
}