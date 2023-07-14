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

#if 0
    class ComputePipeline : public Pipeline
    {

    };

    class GeometryPipeline : public Pipeline
    {
    };
#endif
}
