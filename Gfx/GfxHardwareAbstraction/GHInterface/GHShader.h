#pragma once

namespace Cyan 
{
    class Shader
    {
    public:
        virtual ~Shader() { }
    };

    class GHVertexShader : public Shader
    {
    };

    class GHPixelShader : public Shader
    {
    };

    class GHComputeShader : public Shader
    {
    };
}
