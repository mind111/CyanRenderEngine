#include "GfxContext.h"
#include "CyanRenderer.h"
#include "CyanAPI.h"

namespace Cyan 
{
    GfxContext* Singleton<GfxContext>::singleton = nullptr;
    i32 GfxContext::kMaxCombinedTextureUnits = -1;
    i32 GfxContext::kMaxCombinedShaderStorageBlocks = -1;

    void GfxContext::initialize()
    {
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &kMaxCombinedTextureUnits);
        glGetIntegerv(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS, &kMaxCombinedShaderStorageBlocks);
        m_textureBindings.resize(kMaxCombinedTextureUnits);
    }

    glm::uvec2 GfxContext::getDefaultFramebufferSize()
    { 
        i32 width, height;
        glfwGetWindowSize(singleton->m_glfwWindow, &width, &height);
        return glm::uvec2(width, height);
    }

    Framebuffer* GfxContext::createFramebuffer(u32 width, u32 height)
    {
        static std::unordered_map<std::string, std::shared_ptr<Framebuffer>> framebufferMap;

        std::string key = std::to_string(width) + 'x' + std::to_string(height);
        auto entry = framebufferMap.find(key);
        if (entry == framebufferMap.end())
        {
            Framebuffer* framebuffer = Framebuffer::create(width, height);
            framebufferMap.insert({ key, std::shared_ptr<Framebuffer>(framebuffer)});
            return framebuffer;
        }
        else
        {
            return entry->second.get();
        }
    }

    void GfxContext::setShaderInternal(Shader* shader)
    {
        auto bindTextures = [this](Shader* shader) {
            for (const auto& textureBinding : shader->m_samplerBindingMap) 
            {
                std::string samplerName = textureBinding.first;
                auto texture = textureBinding.second;
                if (texture) 
                {
                    u32 textureUnit = allocTextureUnit();
                    shader->setUniform(samplerName.c_str(), static_cast<i32>(textureUnit));
                    texture->bind(this, textureUnit);
                }
            }
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

    u32 GfxContext::allocTextureUnit()
    {
        assert(numUsedTextureUnits < kMaxCombinedTextureUnits);
        u32 outTextureUnit = numUsedTextureUnits;
        numUsedTextureUnits++;
        return outTextureUnit;
    }

    void GfxContext::resetTextureBindingState()
    {
        // reset all previously used texture units to cleanup texture binding state
        for (u32 i = 0; i < numUsedTextureUnits; ++i)
        {
            assert(m_textureBindings[i] != nullptr);
            m_textureBindings[i]->unbind(this);
        }
        numUsedTextureUnits = 0;
    }

    void GfxContext::setProgramPipelineInternal()
    {
        resetTextureBindingState();
    }

    void GfxContext::setPixelPipeline(PixelPipeline* pixelPipelineObject, const std::function<void(VertexShader*, PixelShader*)>& setupShaders) 
    {
        setProgramPipelineInternal();

        VertexShader* vertexShader = pixelPipelineObject->m_vertexShader;
        PixelShader* pixelShader = pixelPipelineObject->m_pixelShader;
        setupShaders(vertexShader, pixelShader);
        setShaderInternal(vertexShader);
        setShaderInternal(pixelShader);
        pixelPipelineObject->bind();
    }

    void GfxContext::setGeometryPipeline(GeometryPipeline* geometryPipelineObject, const std::function<void(VertexShader*, GeometryShader*, PixelShader*)>& setupShaders) 
    {
        setProgramPipelineInternal();

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
        setProgramPipelineInternal();

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

    void GfxContext::setTexture(GfxTexture* texture, u32 textureUnit) 
    {
        GLuint textureObject = texture == nullptr ? 0 : texture->getGpuResource();
        glBindTextureUnit(textureUnit, textureObject);
        m_textureBindings[textureUnit] = texture;
    }

    void GfxContext::setVertexArray(VertexArray* va) 
    {
        if (va == nullptr) 
        {
            m_vertexArray = nullptr;
            glBindVertexArray(0);
        }
        else
        {
            m_vertexArray = va;
            glBindVertexArray(m_vertexArray->getGpuResource());
        }
    }

    static GLenum translatePrimitiveMode(const PrimitiveMode& inType)
    {
        switch (inType)
        {
            case PrimitiveMode::TriangleList:
                return GL_TRIANGLES;
            case PrimitiveMode::LineStrip:
                return GL_LINE_STRIP;
            case PrimitiveMode::Line:
                return GL_LINES;
            case PrimitiveMode::Points:
                return GL_POINTS;
            default:
                assert(0);
        }
    }

    void GfxContext::setPrimitiveType(PrimitiveMode mode)
    {
        m_gfxPipelineState.primitiveMode = mode;
    }

    void GfxContext::setViewport(Viewport viewport)
    {
        glViewport(viewport.x, viewport.y, viewport.width, viewport.height);

        m_viewport = viewport;
    }

    void GfxContext::setFramebuffer(Framebuffer* framebuffer) 
    {
        if (!framebuffer)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }
        framebuffer->validate();
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->getGpuResource());

        m_framebuffer = framebuffer;
    }

    void GfxContext::setClearColor(glm::vec4 albedo)
    {
        glClearColor(albedo.r, albedo.g, albedo.b, albedo.a);

        m_clearColor = albedo;
    }

    void GfxContext::drawIndexAuto(u32 numVerts, u32 offset) 
    {
        glDrawArrays(translatePrimitiveMode(m_gfxPipelineState.primitiveMode), offset, numVerts);
    }

    void GfxContext::drawIndex(u32 numIndices) 
    {
        glDrawElements(translatePrimitiveMode(m_gfxPipelineState.primitiveMode), numIndices, GL_UNSIGNED_INT, reinterpret_cast<const void*>(0));
    }

    /** todo:
     * This call requires a vertex array object being bound, since I'm using programmable vertex pulling, I'm not using vertex array object
     for multiDrawIndirect, need to use a proper vertex array object to pass the vertex data instead later. For now can bind a dummy vao to 
     suppress the API error.
     */
    void GfxContext::multiDrawArrayIndirect(IndirectDrawBuffer* indirectDrawBuffer)
    {
        indirectDrawBuffer->bind();
        // todo: the offset and stride parameter should be configurable at some point
        glMultiDrawArraysIndirect(translatePrimitiveMode(m_gfxPipelineState.primitiveMode), 0, indirectDrawBuffer->numDrawcalls(), 0);
    }

    void GfxContext::clear()
    {
        glClearColor(.2f, .2f, .2f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void GfxContext::reset()
    {
        if (m_framebuffer)
        {
            m_framebuffer->reset();
            m_framebuffer->unbind();
            m_framebuffer = nullptr;
        }

        m_viewport = {};
        m_pixelPipeline->unbind();
        m_pixelPipeline = nullptr;
        m_nextShaderStorageBinding = 0u;

        m_gfxPipelineState = GfxPipelineState { };

        if (m_vertexArray)
        {
            m_vertexArray->unbind();
            m_vertexArray = nullptr;
        }
    }

    void GfxContext::flip()
    {
        glfwSwapBuffers(m_glfwWindow);
    }
}