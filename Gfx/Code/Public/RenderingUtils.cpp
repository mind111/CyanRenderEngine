#include "RenderingUtils.h"
#include "RenderPass.h"
#include "GfxModule.h"
#include "GfxStaticMesh.h"
#include "Geometry.h"

namespace Cyan
{
    RenderingUtils* RenderingUtils::s_instance = nullptr;
    GfxStaticSubMesh* RenderingUtils::s_unitQuadMesh = nullptr;
    GfxStaticSubMesh* RenderingUtils::s_unitCubeMesh = nullptr;

    RenderingUtils::RenderingUtils()
    {
    }

    RenderingUtils::~RenderingUtils()
    {

    }

    RenderingUtils* RenderingUtils::get()
    {
        static std::unique_ptr<RenderingUtils> instance(new RenderingUtils());
        if (s_instance == nullptr)
        {
            s_instance = instance.get();
        }
        return s_instance;
    }

    void RenderingUtils::initialize()
    {
        // init unit quad
        {
            std::vector<Triangles::Vertex> vertices(6);
            vertices[0].pos = glm::vec3(-1.f, -1.f, 0.f); vertices[0].texCoord0 = glm::vec2(0.f, 0.f);
            vertices[1].pos = glm::vec3( 1.f,  1.f, 0.f); vertices[1].texCoord0 = glm::vec2(1.f, 1.f);
            vertices[2].pos = glm::vec3(-1.f,  1.f, 0.f); vertices[2].texCoord0 = glm::vec2(0.f, 1.f);
            vertices[3].pos = glm::vec3(-1.f, -1.f, 0.f); vertices[3].texCoord0 = glm::vec2(0.f, 0.f);
            vertices[4].pos = glm::vec3( 1.f, -1.f, 0.f); vertices[4].texCoord0 = glm::vec2(1.f, 0.f);
            vertices[5].pos = glm::vec3( 1.f,  1.f, 0.f); vertices[5].texCoord0 = glm::vec2(1.f, 1.f);
            std::vector<u32> indices({ 0, 1, 2, 3, 4, 5 });
            std::unique_ptr<Geometry> unitQuad = std::make_unique<Triangles>(vertices, indices);
            s_unitQuadMesh = GfxStaticSubMesh::create(std::string("UnitQuad"), unitQuad.get());
        }

        // init unit cube
        {
            float positions[] = {
                -1.0f,  1.0f, -1.0f,
                -1.0f, -1.0f, -1.0f,
                 1.0f, -1.0f, -1.0f,
                 1.0f, -1.0f, -1.0f,
                 1.0f,  1.0f, -1.0f,
                -1.0f,  1.0f, -1.0f,

                -1.0f, -1.0f,  1.0f,
                -1.0f, -1.0f, -1.0f,
                -1.0f,  1.0f, -1.0f,
                -1.0f,  1.0f, -1.0f,
                -1.0f,  1.0f,  1.0f,
                -1.0f, -1.0f,  1.0f,

                 1.0f, -1.0f, -1.0f,
                 1.0f, -1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f, -1.0f,
                 1.0f, -1.0f, -1.0f,

                -1.0f, -1.0f,  1.0f,
                -1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f, -1.0f,  1.0f,
                -1.0f, -1.0f,  1.0f,

                -1.0f,  1.0f, -1.0f,
                 1.0f,  1.0f, -1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                -1.0f,  1.0f,  1.0f,
                -1.0f,  1.0f, -1.0f,

                -1.0f, -1.0f, -1.0f,
                -1.0f, -1.0f,  1.0f,
                 1.0f, -1.0f, -1.0f,
                 1.0f, -1.0f, -1.0f,
                -1.0f, -1.0f,  1.0f,
                 1.0f, -1.0f,  1.0f
            };

            u32 numVertices = sizeof(positions) / sizeof(glm::vec3);
            std::vector<Triangles::Vertex> vertices(numVertices);
            std::vector<u32> indices(numVertices);
            for (u32 v = 0; v < numVertices; ++v)
            {
                vertices[v].pos = glm::vec3(positions[v * 3 + 0], positions[v * 3 + 1], positions[v * 3 + 2]);
                indices[v] = v;
            }
            std::unique_ptr<Geometry> unitCube = std::make_unique<Triangles>(vertices, indices);
            s_unitCubeMesh = GfxStaticSubMesh::create(std::string("UnitCube"), unitCube.get());
        }
    }

    void RenderingUtils::deinitialize()
    {

    }

    void RenderingUtils::renderToBackBuffer(GHTexture2D* srcTexture)
    {
        GfxModule* gfxModule = GfxModule::get();
        glm::uvec2 backbufferSize = GfxModule::get()->getWindowSize();

        bool found = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", SHADER_TEXT_PATH "screen_pass_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "BlitTexturePS", SHADER_TEXT_PATH "blit_texture_p.glsl");
        auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

        RenderPass rp(backbufferSize.x, backbufferSize.y);
        rp.enableRenderToDefaultTarget();
        rp.setRenderFunc([gfxp, srcTexture](GfxContext* ctx) {
            gfxp->bind();
            gfxp->setUniform("mipLevel", 0);
            gfxp->setTexture("srcTexture", srcTexture);
            s_unitQuadMesh->draw();
            gfxp->unbind();
        });
        rp.disableDepthTest();
        rp.render(GfxContext::get());
    }
}