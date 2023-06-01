#pragma once

#include <vector>

#include "CyanCore.h"

namespace Cyan
{
    struct IndexBuffer : public GfxResource
    {
        IndexBuffer() = delete;
        IndexBuffer(const std::vector<u32>& indices);
        ~IndexBuffer();
    };
}
