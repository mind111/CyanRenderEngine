// TODO: Deal with name conflicts with windows header
#include <iostream>
#include <fstream>

#include "glfw3.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "gtx/quaternion.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "json.hpp"
#include "glm.hpp"

#include "Common.h"
#include "CyanRenderer.h"
#include "Material.h"
#include "MathUtils.h"

bool fileWasModified(const char* fileName, FILETIME* writeTime)
{
    FILETIME lastWriteTime;
    HANDLE hFile = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
    // If the file is being written to then CreatFile will return a invalid handle.
    if (hFile != INVALID_HANDLE_VALUE)
    {
        GetFileTime(hFile, 0, 0, &lastWriteTime);
        CloseHandle(hFile);
        if (CompareFileTime(&lastWriteTime, writeTime) > 0)
        {
            *writeTime = lastWriteTime;
            return true;
        }
    }
    return false;
}

namespace Cyan
{
    void Renderer::init()
    {
        Cyan::init();
        u_model = createUniform("model", Uniform::Type::u_mat4);
        m_frame = new Frame;
    }

    void Renderer::drawMesh(Mesh* _mesh)
    {
        for (auto subMesh : _mesh->m_subMeshes)
        {
            auto ctx = Cyan::getCurrentGfxCtx();
            Material* matl = subMesh->m_matl; 
            Shader* shader = matl->m_shader;
            ctx->setShader(matl->m_shader);
            for (auto uniform : shader->m_uniforms)
            {
                ctx->setUniform(uniform);
            }
            for (auto buffer : shader->m_buffers)
            {
                ctx->setBuffer(buffer);
            }
            for (auto uniform : matl->m_uniforms)
            {
                ctx->setUniform(uniform);
            }
            u32 textureUnit = 0;
            for (auto binding : matl->m_bindings)
            {
                ctx->setSampler(binding.m_sampler, textureUnit);
                ctx->setTexture(binding.m_tex, textureUnit++);
            }
            ctx->setVertexArray(subMesh->m_vertexArray);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            ctx->drawIndex(subMesh->m_numVerts);
            // NOTES: reset texture units because texture unit bindings are managed by gl context 
            // it won't change when binding different shaders
            for (int t = 0; t < textureUnit; ++t)
            {
                ctx->setTexture(nullptr, t);
            }
        }
    }

    // TODO:
    void Renderer::submitMesh(Mesh* mesh, glm::mat4 modelTransform)
    {
        for (u32 index = 0; index < mesh->m_subMeshes.size(); ++index)
        {
            DrawCall draw = {};
            // draw.m_uniformBegin = m_frame->m_uniformBuffer.m_pos;
            // MaterialInstance* material = mesh->m_subMeshes[index]->m_matl;
            // for ( : material)
            // {
            //     Cyan::setUniform();
            // }
            // draw.m_uniformEnd = m_frame->m_uniformBuffer.m_pos;

            draw.m_mesh = mesh;
            draw.m_index = index;
            draw.m_modelTransform = modelTransform;
            m_frame->m_renderQueue.push_back(draw);
        }
    }

    void Renderer::renderFrame()
    {
        auto ctx = Cyan::getCurrentGfxCtx();
        for (auto draw : m_frame->m_renderQueue)
        {
            auto sm = draw.m_mesh->m_subMeshes[draw.m_index];
            auto material = sm->m_matl;
            auto shader = material->m_shader;

            ctx->setShader(shader);
            // TODO: This is dumb
            Cyan::setUniform(u_model, &draw.m_modelTransform[0][0]);
            ctx->setUniform(u_model);
            for (auto uniform : shader->m_uniforms)
            {
                ctx->setUniform(uniform);
            }
            for (auto buffer : shader->m_buffers)
            {
                ctx->setBuffer(buffer);
            }
            for (auto uniform : material->m_uniforms)
            {
                ctx->setUniform(uniform);
            }
            u32 textureUnit = 0;
            for (auto binding : material->m_bindings)
            {
                ctx->setSampler(binding.m_sampler, textureUnit);
                ctx->setTexture(binding.m_tex, textureUnit++);
            }
            ctx->setVertexArray(sm->m_vertexArray);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            ctx->drawIndex(sm->m_numVerts);
            // NOTES: reset texture units because texture unit bindings are managed by gl context 
            // it won't change when binding different shaders
            for (int t = 0; t < textureUnit; ++t)
            {
                ctx->setTexture(nullptr, t);
            }
        }
    }

    void Renderer::drawEntity(Entity* _e) 
    {
        auto computeModelMatrix = [&]() {
            glm::mat4 model(1.f);
            // model = trans * rot * scale
            model = glm::translate(model, _e->m_xform.translation);
            glm::mat4 rotation = glm::toMat4(_e->m_xform.qRot);
            model *= rotation;
            model = glm::scale(model, _e->m_xform.scale);
            return model;
        };
        // glm::mat4 modelTransform = computeModelMatrix();
        // submitMesh(_e->m_mesh, modelTransform);

        Cyan::setUniform(u_model, &computeModelMatrix()[0][0]);

        Mesh* mesh = _e->m_mesh;
        auto ctx = Cyan::getCurrentGfxCtx();
        for (auto sm : mesh->m_subMeshes)
        {
            // TODO: (Optimize) This could be slow as well
            Shader* shader = sm->m_matl->m_shader;
            ctx->setShader(shader);
            ctx->setUniform(u_model);
        }
        drawMesh(mesh);
    }

    void Renderer::endFrame()
    {
        m_frame->m_renderQueue.clear();
        Cyan::getCurrentGfxCtx()->flip();
    }

    void Renderer::render(Scene* scene)
    {
        //
        // Draw all the entities
        for (auto entity : scene->entities)
        {
            drawEntity(&entity);
        }

        // Post-processing
        {

        }

        //renderFrame();
    }

    float quadVerts[24] = {
        -1.f, -1.f, 0.f, 0.f,
        1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f,

        -1.f, -1.f, 0.f, 0.f,
        1.f, -1.f, 1.f, 0.f,
        1.f,  1.f, 1.f, 1.f
    };
}