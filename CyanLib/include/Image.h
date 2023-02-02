#pragma once

#include <memory>
#include <string>

#include "Common.h"

namespace Cyan
{
    struct Image
    {
        Image(const char* inName, const char* srcImageFile);
        Image(const char* inName, u8* mem, u32 sizeInBytes);

        static u32 count;
        const char* path = nullptr;
        std::string name;
        i32 width;
        i32 height;
        i32 numChannels;
        i32 bitsPerChannel;
        std::shared_ptr<u8> pixels = nullptr;
    };
}
