#pragma once

namespace Cyan
{
    class Texture
    {
    public:
        static Texture* create();

        virtual ~Texture() { }

        virtual void bind() = 0;
        virtual void unbind() = 0;
    };

    class Texture2D : public Texture
    {
    public:
        static Texture2D* create();

        virtual ~Texture2D() { }
    };

    class TextureCube : public Texture
    {
    public:
        static TextureCube* create();

        virtual ~TextureCube() { }
    };
}
