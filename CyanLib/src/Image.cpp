#include <cassert>

#include "Image.h"
#include "stbi/stb_image.h"

namespace Cyan
{
    u32 Image::count = 0;
    Image::Image(const char* srcImageFile, const char* inName)
        : path(srcImageFile)
    {
        if (inName)
        {
            name = inName;
        }
        else
        {
            name = "image_" + std::to_string(count);
        }
        i32 hdr = stbi_is_hdr(srcImageFile);
        if (hdr)
        {
            bitsPerChannel = 32;
            pixels = std::shared_ptr<u8>((u8*)stbi_loadf(srcImageFile, &width, &height, &numChannels, 0));
            assert(0);
        }
        else
        {
            i32 is16Bit = stbi_is_16_bit(srcImageFile);
            if (is16Bit)
            {
                pixels = std::shared_ptr<u8>((u8*)stbi_load_16(srcImageFile, &width, &height, &numChannels, 0));
                bitsPerChannel = 16;
            }
            else
            {
                pixels = std::shared_ptr<u8>(stbi_load(srcImageFile, &width, &height, &numChannels, 0));
                bitsPerChannel = 8;
            }
        }
        count++;
    }

    Image::Image(u8* mem, u32 sizeInBytes, const char* inName)
    {
        if (inName)
        {
            name = inName;
        }
        else
        {
            name = "image_" + std::to_string(count);
        }
        i32 hdr = stbi_is_hdr_from_memory(mem, sizeInBytes);
        if (hdr)
        {
            bitsPerChannel = 32;
            pixels = std::shared_ptr<u8>(((u8*)stbi_loadf_from_memory(mem, sizeInBytes, &width, &height, &numChannels, 0)));
        }
        else
        {
            i32 is16Bit = stbi_is_16_bit_from_memory(mem, sizeInBytes);
            if (is16Bit)
            {
                bitsPerChannel = 16;
                pixels = std::shared_ptr<u8>((u8*)stbi_load_16_from_memory(mem, sizeInBytes, &width, &height, &numChannels, 0));
            }
            else
            {
                bitsPerChannel = 8;
                pixels = std::shared_ptr<u8>((u8*)stbi_load_from_memory(mem, sizeInBytes, &width, &height, &numChannels, 0));
            }
        }
        count++;
    }
}