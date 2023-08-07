#include "Image.h"
#include "stb_image.h"

namespace Cyan
{
    Image::Image(const char* name)
        : Asset(name)
        , m_width(0)
        , m_height(0)
        , m_numChannels(0)
        , m_bitsPerChannel(0)
        , m_pixels(nullptr)
    {
    }

    Image::~Image()
    {

    }

    void Image::onLoaded()
    {
        std::lock_guard<std::mutex> lock(m_listenerMutex);

        bLoaded.store(true);

        // notify all the listeners
        for (auto& listener : m_listeners)
        {
            listener(this);
        }

        // clear pending listeners
        m_listeners.clear();
    }

    void Image::importFromFile(const char* filename)
    {
        i32 hdr = stbi_is_hdr(filename);
        if (hdr)
        {
            m_bitsPerChannel = 32;
            m_pixels.reset((u8*)stbi_loadf(filename, &m_width, &m_height, &m_numChannels, 0));
        }
        else
        {
            i32 is16Bit = stbi_is_16_bit(filename);
            if (is16Bit)
            {
                m_pixels.reset((u8*)stbi_load_16(filename, &m_width, &m_height, &m_numChannels, 0));
                m_bitsPerChannel = 16;
            }
            else
            {
                m_pixels.reset((u8*)stbi_load(filename, &m_width, &m_height, &m_numChannels, 0));
                m_bitsPerChannel = 8;
            }
        }

        onLoaded();
    }

    void Image::importFromMemory(u8* memory, u32 sizeInBytes)
    {
        i32 hdr = stbi_is_hdr_from_memory(memory, sizeInBytes);
        if (hdr)
        {
            m_bitsPerChannel = 32;
            m_pixels.reset((u8*)stbi_loadf_from_memory(memory, sizeInBytes, &m_width, &m_height, &m_numChannels, 0));
        }
        else
        {
            i32 is16Bit = stbi_is_16_bit_from_memory(memory, sizeInBytes);
            if (is16Bit)
            {
                m_bitsPerChannel = 16;
                m_pixels.reset((u8*)stbi_load_16_from_memory(memory, sizeInBytes, &m_width, &m_height, &m_numChannels, 0));
            }
            else
            {
                m_bitsPerChannel = 8;
                m_pixels.reset((u8*)stbi_load_from_memory(memory, sizeInBytes, &m_width, &m_height, &m_numChannels, 0));
            }
        }

        onLoaded();
    }

    void Image::addListener(const Listener& listener)
    {
        bool bIsLoaded = bLoaded.load();
        if (bIsLoaded)
        {
            listener(this);
        }
        else
        {
            std::lock_guard<std::mutex> lock(m_listenerMutex);
            m_listeners.push_back(listener);
        }
    }
}