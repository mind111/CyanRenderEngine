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
    }

    void Image::addListener(const Listener& listener)
    {

    }
}