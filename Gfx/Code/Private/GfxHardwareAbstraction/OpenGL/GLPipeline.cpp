#include "GLPipeline.h"

namespace Cyan
{
    GLGfxPipeline::GLGfxPipeline(std::shared_ptr<GLVertexShader> vs, std::shared_ptr<GLPixelShader> ps)
        : GLObject(), m_vertexShader(vs), m_pixelShader(ps)
    {
        assert(m_vertexShader != nullptr && m_pixelShader != nullptr);

        glCreateProgramPipelines(1, &m_name);
        glUseProgramStages(m_name, GL_VERTEX_SHADER_BIT, m_vertexShader->getName());
        glUseProgramStages(m_name, GL_FRAGMENT_SHADER_BIT, m_pixelShader->getName());
    }

    GLGfxPipeline::~GLGfxPipeline()
    {
        GLuint pipelines[1] = { m_name };
        glDeleteProgramPipelines(1, pipelines);
    }

    void GLGfxPipeline::bind()
    {
        glBindProgramPipeline(m_name);
    }

    void GLGfxPipeline::unbind()
    {
        glBindProgramPipeline(0);
    }
}