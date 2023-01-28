#include "GfxContext.h"
#include "CyanRenderer.h"
#include "CyanAPI.h"

namespace Cyan {
    GfxContext* Singleton<GfxContext>::singleton = nullptr;
    void GfxContext::setShaderInternal(Shader* shader)
    {
        auto bindTextures = [this](Shader* shader) {
            u32 numUsedBindings = 0u;
            for (const auto& bindingState : shader->m_samplerBindingMap) {
                std::string samplerName = bindingState.first;
                auto texture = bindingState.second;
                if (texture) {
                    u32 bindingUnit = m_nextTextureBindingUnit;
                    if (!m_textureBindingMap.empty())
                    {
                        auto entry = m_textureBindingMap.find(texture->name);
                        if (entry != m_textureBindingMap.end())
                        {
                            bindingUnit = entry->second;
                        }
                        else {
                            bindingUnit = m_nextTextureBindingUnit;
                            m_textureBindingMap.insert({ std::string(texture->name),  bindingUnit });
                            m_nextTextureBindingUnit = (m_nextTextureBindingUnit + 1) % kMaxNumTextureUnits;
                            numUsedBindings++;
                        }
                    }
                    else
                    {
                        m_textureBindingMap.insert({ std::string(texture->name),  bindingUnit });
                        m_nextTextureBindingUnit = (m_nextTextureBindingUnit + 1) % kMaxNumTextureUnits;
                        numUsedBindings++;
                    }
                    shader->setUniform(samplerName.c_str(), static_cast<i32>(bindingUnit));
                    setTexture(texture, bindingUnit);
                }
            }
            return numUsedBindings;
        };

        auto bindShaderStorageBuffers = [this](Shader* shader) {
            for (const auto& shaderStorageBlock : shader->m_shaderStorageBlockMap) 
            {
                std::string blockName = shaderStorageBlock.first;
                u32 blockIndex = shaderStorageBlock.second;
                if (!m_shaderStorageBindingMap.empty())
                {
                    auto entry = m_shaderStorageBindingMap.find(blockName);
                    if (entry == m_shaderStorageBindingMap.end()) 
                    {
                        cyanError("ShaderStorageBuffer %s is not currently bound", blockName.c_str());
                    }
                    else 
                    {
                        u32 binding = entry->second;
                        glShaderStorageBlockBinding(shader->getProgram(), blockIndex, binding);
                    }
                }
            }
        };

        if (shader) 
        {
            bindTextures(shader);
            bindShaderStorageBuffers(shader);
        }
    }


    void GfxContext::setPixelPipeline(PixelPipeline* pixelPipelineObject, const std::function<void(VertexShader*, PixelShader*)>& setupShaders) 
    {
        VertexShader* vertexShader = pixelPipelineObject->m_vertexShader;
        PixelShader* pixelShader = pixelPipelineObject->m_pixelShader;
        setupShaders(vertexShader, pixelShader);
        setShaderInternal(vertexShader);
        setShaderInternal(pixelShader);
        pixelPipelineObject->bind();
    }

    void GfxContext::setGeometryPipeline(GeometryPipeline* geometryPipelineObject, const std::function<void(VertexShader*, GeometryShader*, PixelShader*)>& setupShaders) 
    {
        VertexShader* vertexShader = geometryPipelineObject->m_vertexShader;
        GeometryShader* geometryShader = geometryPipelineObject->m_geometryShader;
        PixelShader* pixelShader = geometryPipelineObject->m_pixelShader;
        setupShaders(vertexShader, geometryShader, pixelShader);
        setShaderInternal(vertexShader);
        setShaderInternal(geometryShader);
        setShaderInternal(pixelShader);
        geometryPipelineObject->bind();
    }

    void GfxContext::setComputePipeline(ComputePipeline* computePipelineObject, const std::function<void(ComputeShader*)>& setupShaders) 
    {
        ComputeShader* computeShader = computePipelineObject->m_computeShader;
        setupShaders(computeShader);
        setShaderInternal(computeShader);
        computePipelineObject->bind();
    }

    void GfxContext::setCullFace(FrontFace frontFace, FaceCull faceToCull)
    {
        glEnable(GL_CULL_FACE);
        GLenum gl_frontFace, gl_faceToCull;
        switch(frontFace)
        {
            case FrontFace::ClockWise:
                gl_frontFace = GL_CW;
                break;
            case FrontFace::CounterClockWise:
                gl_frontFace = GL_CCW;
                break;
        }
        switch(faceToCull)
        {
            case FaceCull::Front:
                gl_faceToCull = GL_FRONT;
                break;
            case FaceCull::Back:
                gl_faceToCull = GL_BACK;
                break;
        }
        glFrontFace(gl_frontFace);
        glCullFace(gl_faceToCull);
    }

    void GfxContext::setDepthControl(DepthControl ctrl) {
        switch(ctrl)
        {
            case DepthControl::kEnable:
                glEnable(GL_DEPTH_TEST);
                break;
            case DepthControl::kDisable:
                glDisable(GL_DEPTH_TEST);
                break;
            default:
                break;
        }
    }

    void GfxContext::setTexture(ITexture* texture, u32 binding) 
    {
        // reset a texture unit
        if (!texture)
        {
            glBindTextureUnit(binding, 0);
        }
        else
        {
            glBindTextureUnit(binding, texture->getGpuObject());
        }
    }

    void GfxContext::setVertexArray(VertexArray* va) 
    {
        if (!va) 
        {
            m_va = nullptr;
            glBindVertexArray(0);
            return;
        }
        m_va = va;
        glBindVertexArray(m_va->getGLObject());
    }

    void GfxContext::setPrimitiveType(PrimitiveMode _type)
    {
        switch (_type)
        {
            case PrimitiveMode::TriangleList:
                m_primitiveType = GL_TRIANGLES;
                break;
            case PrimitiveMode::LineStrip:
                m_primitiveType = GL_LINE_STRIP;
                break;
            case PrimitiveMode::Line:
                m_primitiveType = GL_LINES;
                break;
            case PrimitiveMode::Points:
                m_primitiveType = GL_POINTS;
                break;
            default:
                break;
        }
    }

    void GfxContext::setViewport(Viewport viewport)
    {
        m_viewport = viewport;
        glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    }

    void GfxContext::setRenderTarget(RenderTarget* renderTarget) 
    {
        if (!renderTarget)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->fbo);
    }

    void GfxContext::setRenderTarget(RenderTarget* renderTarget, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers)
    {
        if (!renderTarget)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }
        GLenum* buffers = static_cast<GLenum*>(_alloca(drawBuffers.size() * sizeof(GLenum)));
        i32 numBuffers = drawBuffers.size();
        for (i32 i = 0; i < numBuffers; ++i)
        {
            const RenderTargetDrawBuffer& drawBuffer = *(drawBuffers.begin() + i);
            if (drawBuffer.binding > -1)
            {
                buffers[i] = GL_COLOR_ATTACHMENT0 + drawBuffer.binding;
            }
            else
            {
                buffers[i] = GL_NONE;
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->fbo);
        glDrawBuffers(numBuffers, buffers);
    }

    void GfxContext::setClearColor(glm::vec4 albedo)
    {
        glClearColor(albedo.r, albedo.g, albedo.b, albedo.a);
    }

    void GfxContext::drawIndexAuto(u32 numVerts, u32 offset) 
    {
        glDrawArrays(m_primitiveType, offset, numVerts);
    }

    void GfxContext::drawIndex(u32 numIndices) 
    {
        glDrawElements(m_primitiveType, numIndices, GL_UNSIGNED_INT, reinterpret_cast<const void*>(0));
    }

    void GfxContext::clear()
    {
        glClearColor(.2f, .2f, .2f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void GfxContext::flip()
    {
        glfwSwapBuffers(m_glfwWindow);
    }
}