#pragma once

#include <memory>
#include <string>

#include "Common.h"

namespace Cyan
{
    struct Image
    {
        Image(const char* srcImageFile, const char* inName = nullptr);
        Image(u8* mem, u32 sizeInBytes, const char* inName = nullptr);

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
