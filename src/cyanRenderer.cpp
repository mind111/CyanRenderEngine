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
        u_model = createUniform("s_model", Uniform::Type::u_mat4);
        u_cameraView = createUniform("s_view", Uniform::Type::u_mat4);
        u_cameraProjection = createUniform("s_projection", Uniform::Type::u_mat4);
        m_frame = new Frame;
    }

    void Renderer::drawMeshInstance(MeshInstance* meshInstance, glm::mat4* modelMatrix)
    {
        Mesh* mesh = meshInstance->m_mesh;

        for (u32 i = 0; i < mesh->m_subMeshes.size(); ++i)
        {
            auto ctx = Cyan::getCurrentGfxCtx();
            MaterialInstance* matl = meshInstance->m_matls[i]; 
            Material* matlClass = matl->m_template;
            Shader* shader = matlClass->m_shader;
            // TODO: this is obviously redundant
            ctx->setShader(matlClass->m_shader);
            if (modelMatrix)
            {
                // TODO: this is obviously redundant
                Cyan::setUniform(u_model, &(*modelMatrix)[0][0]);
                ctx->setUniform(u_model);
            }
            ctx->setUniform(u_cameraView);
            ctx->setUniform(u_cameraProjection);
            // for (auto uniform : shader->m_uniforms)
            // {
            //     ctx->setUniform(uniform);
            // }
            for (auto buffer : shader->m_buffers)
            {
                ctx->setBuffer(buffer);
            }
            u32 usedTextureUnits = matl->bind();
            Mesh::SubMesh* sm = mesh->m_subMeshes[i];
            ctx->setVertexArray(sm->m_vertexArray);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            ctx->drawIndex(sm->m_numVerts);
            // NOTES: reset texture units because texture unit bindings are managed by gl context 
            // it won't change when binding different shaders
            for (u32 t = 0; t < usedTextureUnits; ++t)
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
    #if 0
        auto ctx = Cyan::getCurrentGfxCtx();
        for (auto draw : m_frame->m_renderQueue)
        {
            auto sm = draw.m_mesh->m_subMeshes[draw.m_index];
            MaterialInstance* material = sm->m_matl;
            Material* matlClass = material->m_template;
            auto shader = matlClass->m_shader;

            ctx->setShader(shader);
            // TODO: This is dumb
            Cyan::setUniform(u_model, &draw.m_modelTransform[0][0]);
            ctx->setUniform(u_model);
            // for (auto uniform : shader->m_uniforms)
            // {
            //     ctx->setUniform(uniform);
            // }
            for (auto buffer : shader->m_buffers)
            {
                ctx->setBuffer(buffer);
            }

            u32 usedTextureUnit = material->bind();

            ctx->setVertexArray(sm->m_vertexArray);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            ctx->drawIndex(sm->m_numVerts);
            // NOTES: reset texture units because texture unit bindings are managed by gl context 
            // it won't change when binding different shaders
            for (int t = 0; t < usedTextureUnit; ++t)
            {
                ctx->setTexture(nullptr, t);
            }
        }
    #endif
    }

    void Renderer::drawEntity(Entity* entity) 
    {
        auto computeModelMatrix = [&]() {
            glm::mat4 model(1.f);
            // model = trans * rot * scale
            model = glm::translate(model, entity->m_xform->translation);
            glm::mat4 rotation = glm::toMat4(entity->m_xform->qRot);
            model *= rotation;
            model = glm::scale(model, entity->m_xform->scale);
            return model;
        };
        // glm::mat4 modelTransform = computeModelMatrix();
        // submitMesh(_e->m_mesh, modelTransform);
        glm::mat4 model;
        if (entity->m_xform)
        {
            model = computeModelMatrix();
            drawMeshInstance(entity->m_meshInstance, &model);
        }
        else
        {
            drawMeshInstance(entity->m_meshInstance, nullptr);
        }

        // for (auto sm : mesh->m_subMeshes)
        // {
        //     // TODO: (Optimize) This could be slow as well
        //     Shader* shader = sm->m_matl->m_template->m_shader;
        //     ctx->setShader(shader);
        //     ctx->setUniform(u_model);
        // }
    }

    void Renderer::endFrame()
    {
        m_frame->m_renderQueue.clear();
        Cyan::getCurrentGfxCtx()->flip();
    }

    void Renderer::renderScene(Scene* scene)
    {
        //
        // Draw all the entities
        for (auto entity : scene->entities)
        {
            drawEntity(entity);
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