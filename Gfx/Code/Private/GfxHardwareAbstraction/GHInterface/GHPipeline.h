#pragma once

namespace Cyan
{
    class GHGfxPipeline
    {
    public:
        virtual ~GHGfxPipeline() { }
        virtual void bind() = 0;
        virtual void unbind() = 0;
    };

    class GHComputePipeline
    {
    public:
        virtual ~GHComputePipeline() { }
        virtual void bind() = 0;
        virtual void unbind() = 0;
    };

    class GHGeometryPipeline
    {
    public:
        virtual ~GHGeometryPipeline() { }
        virtual void bind() = 0;
        virtual void unbind() = 0;
    };
}
