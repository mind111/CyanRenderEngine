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

#if 0
    class GeometryPipeline : public Pipeline
    {
    };
#endif
}
