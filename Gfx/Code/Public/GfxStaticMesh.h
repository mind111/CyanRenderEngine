#pragma once

namespace Cyan
{
    class GfxStaticSubMesh
    {
    public:
        virtual ~GfxStaticSubMesh() { };
        virtual void draw() = 0;
    protected:
        GfxStaticSubMesh();
    };
}
