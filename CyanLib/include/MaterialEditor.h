#pragma once

#include "Material.h"
#include "AssetManager.h"

namespace Cyan
{
    class IMaterialEditor
    {
    public:
        virtual ~IMaterialEditor() {}
        virtual void renderUI() = 0;
    };

    // todo: how to do this in a more flexible way
    class MaterialEditor : public IMaterialEditor
    {
    public:
        MaterialEditor(PBRMaterial* inMaterial)
            : material(inMaterial)
        { }

        virtual void renderUI() override 
        { 
        }

    private:
        PBRMaterial* material = nullptr;
    };
}
