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

    void Image::onLoaded()
    {
        std::lock_guard<std::mutex> lock(m_listenersMutex);
        // todo: should listeners be removed once the event is fired? probably not
        for (auto listener : m_listeners)
        {
            listener->onImageLoaded();
        }
        m_listeners.clear();
        m_state = State::kLoaded;
    }

    void Image::addListener(Listener* listener) 
    { 
        std::lock_guard<std::mutex> lock(m_listenersMutex);
        if (isLoaded())
        {
            listener->onImageLoaded();
        }
        else
        {
            m_listeners.push_back(listener); 
        }
    }

    void Image::removeListener(Listener* toRemove) 
    { 
        std::lock_guard<std::mutex> lock(m_listenersMutex);
        for (i32 i = 0; i < m_listeners.size(); ++i)
        {
            if (m_listeners[i] == toRemove)
            {
                m_listeners.erase(m_listeners.begin() + i);
                break;
            }
        }
    }
}