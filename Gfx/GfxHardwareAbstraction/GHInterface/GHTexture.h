#pragma once

namespace Cyan
{
    class GHTexture
    {
    public:
        static GHTexture* create();

        virtual ~GHTexture() { }

        virtual void bind() = 0;
        virtual void unbind() = 0;
    };

    class GHTexture2D : public GHTexture
    {
    public:
        static GHTexture2D* create();

        virtual ~GHTexture2D() { }
    };

    class GHTextureCube : public GHTexture
    {
    public:
        static GHTextureCube* create();

        virtual ~GHTextureCube() { }
    };
}
