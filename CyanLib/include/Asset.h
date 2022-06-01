#pragma once

namespace Cyan
{
    struct Asset
    {
        virtual std::string getAssetObjectTypeDesc() = 0;
        static std::string getAssetClassTypeDesc() { return std::string("BaseAsset"); }

        // todo: implement the following
        virtual void serialize() { }
        virtual void deserialize() { }
    };
}
