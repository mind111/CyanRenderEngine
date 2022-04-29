#pragma once

#include "Common.h"

namespace Cyan
{
    struct Asset
    {
        static std::string typeIdentifier;
        virtual std::string getAssetTypeIdentifier() = 0;

        // todo: implement the following
        virtual void serialize() { }
        virtual void deserialize() { }
    };

    class AssetFactory
    {
        virtual Asset* create() = 0;
    };
}

