#pragma once

#include "Core.h"
#include "Asset.h"

namespace Cyan
{
    class Image : public Asset
    {
    public:
        using Listener = std::function<void(Image*)>;

        Image(const char* name);
        virtual ~Image();

        static const char* getAssetTypeName() { return "Image"; }

        virtual void load() override { }
        virtual void onLoaded() override;
        virtual void unload() { }

        void importFromFile(const char* imageFilename);
        void importFromMemory(u8* memory, u32 sizeInBytes);
        void addListener(const Listener& listener);

        i32 m_width;
        i32 m_height;
        i32 m_numChannels;
        i32 m_bitsPerChannel;
        std::unique_ptr<u8> m_pixels = nullptr;
    };
}
