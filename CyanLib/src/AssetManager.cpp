#include <fstream>
#include <sstream>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "AssetManager.h"
#include "AssetImporter.h"
#include "Texture.h"
#include "GraphicsSystem.h"

namespace Cyan
{
    AssetManager* Singleton<AssetManager>::singleton = nullptr;
    AssetManager::AssetManager()
    {
        m_assetImporter = std::make_unique<AssetImporter>(this);

        const u32 sizeInPixels = 16 * 1024;
        for (i32 i = 0; i < (i32)Texture2DAtlas::Format::kCount; ++i)
        { switch ((Texture2DAtlas::Format)(i)) 
            {
            case Texture2DAtlas::Format::kR8:
                atlases[i] = new Texture2DAtlas(sizeInPixels, PF_R8);
                break;
            case Texture2DAtlas::Format::kRGB8:
                atlases[i] = new Texture2DAtlas(sizeInPixels, PF_RGB8);
                break;
            case Texture2DAtlas::Format::kRGBA8:
                atlases[i] = new Texture2DAtlas(sizeInPixels, PF_RGBA8);
                break;
#if 0
            case Format::kRGB16F:
                assert(0);
                break;
            case Format::kRGBA16F:
                assert(0);
                break;
#endif
            default: 
                assert(0);
                break;
            }
        }
    }

    AssetManager::~AssetManager()
    {

    }

    void AssetManager::initialize()
    {
        /**
            initialize default geometries and shapes
        */ 
        // line

        // shared global quad mesh
        {
            float quadVerts[24] = {
                -1.f, -1.f, 0.f, 0.f,
                 1.f,  1.f, 1.f, 1.f,
                -1.f,  1.f, 0.f, 1.f,

                -1.f, -1.f, 0.f, 0.f,
                 1.f, -1.f, 1.f, 0.f,
                 1.f,  1.f, 1.f, 1.f
            };

            m_defaultShapes.fullscreenQuad = createStaticMesh("FullScreenQuadMesh", 1);
            std::vector<Triangles::Vertex> vertices(6);
            vertices[0].pos = glm::vec3(-1.f, -1.f, 0.f); vertices[0].texCoord0 = glm::vec2(0.f, 0.f);
            vertices[1].pos = glm::vec3( 1.f,  1.f, 0.f); vertices[1].texCoord0 = glm::vec2(1.f, 1.f);
            vertices[2].pos = glm::vec3(-1.f,  1.f, 0.f); vertices[2].texCoord0 = glm::vec2(0.f, 1.f);
            vertices[3].pos = glm::vec3(-1.f, -1.f, 0.f); vertices[3].texCoord0 = glm::vec2(0.f, 0.f);
            vertices[4].pos = glm::vec3( 1.f, -1.f, 0.f); vertices[4].texCoord0 = glm::vec2(1.f, 0.f);
            vertices[5].pos = glm::vec3( 1.f,  1.f, 0.f); vertices[5].texCoord0 = glm::vec2(1.f, 1.f);
            std::vector<u32> indices({ 0, 1, 2, 3, 4, 5 });
            auto t = std::make_unique<Triangles>(vertices, indices);
            m_defaultShapes.fullscreenQuad->createSubmesh(0, std::move(t));
        }

        // cube
        {
            m_defaultShapes.unitCubeMesh = createStaticMesh("UnitCubeMesh", 1);
            u32 numVertices = sizeof(cubeVertices) / sizeof(glm::vec3);
            std::vector<Triangles::Vertex> vertices(numVertices);
            std::vector<u32> indices(numVertices);
            for (u32 v = 0; v < numVertices; ++v)
            {
                vertices[v].pos = glm::vec3(cubeVertices[v * 3 + 0], cubeVertices[v * 3 + 1], cubeVertices[v * 3 + 2]);
                indices[v] = v;
            }
            auto t = std::make_unique<Triangles>(vertices, indices);
            m_defaultShapes.unitCubeMesh->createSubmesh(0, std::move(t));
        }

#if 0
        // quad
        m_defaultShapes.quad = m_assetImporter->importWavefrontObj("Quad", ASSET_PATH "mesh/default/quad.obj");
        // sphere
        m_defaultShapes.sphere = m_assetImporter->importWavefrontObj("Sphere", ASSET_PATH "mesh/default/sphere.obj");
        // icosphere
        m_defaultShapes.icosphere = m_assetImporter->importWavefrontObj("IcoSphere", ASSET_PATH "mesh/default/icosphere.obj");
        // bounding sphere
        // todo: line mesh doesn't work
        m_defaultShapes.boundingSphere = m_assetImporter->importWavefrontObj("BoundingSphere", ASSET_PATH "mesh/default/bounding_sphere.obj");
        // disk
        m_defaultShapes.disk = m_assetImporter->importWavefrontObj("Disk", ASSET_PATH "mesh/default/disk.obj");
        // todo: cylinder
#endif
 
        /**
        *   initialize default textures
        */ 
        {
            Sampler2D sampler = { };
            sampler.minFilter = FM_TRILINEAR;
            sampler.magFilter = FM_BILINEAR;
            sampler.wrapS = Sampler::Addressing::kWrap;
            sampler.wrapT = Sampler::Addressing::kWrap;

            auto checkerDarkImage = AssetImporter::importImage("default_checker_dark", ASSET_PATH "textures/defaults/checker_dark.png");
            m_defaultTextures.checkerDark = createTexture2D("default_checker_dark", checkerDarkImage, sampler);
            auto gridDarkImage = AssetImporter::importImage("default_grid_dark", ASSET_PATH "textures/defaults/grid_dark.png");
            m_defaultTextures.gridDark = createTexture2D("default_grid_dark", gridDarkImage, sampler);
        }
        {
            Sampler2D sampler = { };
            sampler.minFilter = FM_POINT;
            sampler.magFilter = FM_POINT;
            sampler.wrapS = WM_WRAP;
            sampler.wrapT = WM_WRAP;

            auto blueNoiseImage_128x128 = AssetImporter::importImage("BlueNoise_128x128_R", ASSET_PATH "textures/noise/BN_128x128_R.png");
            m_defaultTextures.blueNoise_128x128_R = createTexture2D("BlueNoise_128x128_R", blueNoiseImage_128x128, sampler);

            auto blueNoiseImage_1024x1024 = AssetImporter::importImage("BlueNoise_1024x1024_RGBA", ASSET_PATH "textures/noise/BN_1024x1024_RGBA.png");
            m_defaultTextures.blueNoise_1024x1024_RGBA = createTexture2D("BlueNoise_1024x1024_RGBA", blueNoiseImage_1024x1024, sampler);
        }

#undef DEFAULT_TEXTURE_FOLDER

        /**
            initialize the default material 
        */ 
        createMaterial("DefaultMaterial");

        stbi_set_flip_vertically_on_load(1);
    }

    template <>
    std::shared_ptr<StaticMesh> AssetManager::findAsset(const char* name)
    {
        std::shared_ptr<StaticMesh> out = nullptr;
        auto entry = singleton->m_meshMap.find(name);
        if (entry != singleton->m_meshMap.end())
        {
            out = entry->second;
        }
        return out;
    }

    template <>
    std::shared_ptr<Texture2D> AssetManager::findAsset(const char* name)
    {
        std::shared_ptr<Texture2D> out = nullptr;
        auto entry = singleton->m_textureMap.find(name);
        if (entry != singleton->m_textureMap.end())
        {
            out = entry->second;
        }
        return out;
    }

    template <>
    std::shared_ptr<Image> AssetManager::findAsset(const char* name)
    {
        std::shared_ptr<Image> out = nullptr;
        auto entry = singleton->m_imageMap.find(name);
        if (entry != singleton->m_imageMap.end())
        {
            out = entry->second;
        }
        return out;
    }

    template <>
    std::shared_ptr<Material> AssetManager::findAsset(const char* name)
    {
        std::shared_ptr<Material> out = nullptr;
        auto entry = singleton->m_materialMap.find(name);
        if (entry != singleton->m_materialMap.end())
        {
            out = entry->second;
        }
        return out;
    }

    std::shared_ptr<StaticMesh> AssetManager::createStaticMesh(const char* name, u32 numSubmeshes)
    {
        std::shared_ptr<StaticMesh> outMesh = nullptr;
        auto entry = singleton->m_meshMap.find(name);
        if (entry == singleton->m_meshMap.end())
        {
            outMesh = std::make_shared<StaticMesh>(name, numSubmeshes);
            singleton->m_meshMap.insert({ name, outMesh });
        }
        else
        {
            outMesh = entry->second;
        }
        return outMesh;
    }

    std::shared_ptr<Image> AssetManager::createImage(const char* name)
    {
        assert(name);
        std::shared_ptr<Image> outImage = nullptr;
        auto entry = singleton->m_imageMap.find(name);
        if (entry == singleton->m_imageMap.end())
        {
            outImage = std::make_shared<Image>(name);
            singleton->m_imageMap.insert({ outImage->m_name, outImage});
        }
        else
        {
            outImage = entry->second;
        }
        return outImage;
    }

    std::shared_ptr<Texture2D> AssetManager::createTexture2D(const char* name, std::shared_ptr<Image> srcImage, const Sampler2D& inSampler)
    {
        std::shared_ptr<Texture2D> outTexture = nullptr;
        auto entry = singleton->m_textureMap.find(name);
        if (entry == singleton->m_textureMap.end())
        {
            outTexture = std::make_shared<Texture2D>(name, srcImage, inSampler);
            singleton->m_textureMap.insert({ outTexture->m_name, outTexture });
        }
        else
        {
            outTexture = entry->second;
        }
        return outTexture;
    }

    std::shared_ptr<Material> AssetManager::createMaterial(const char* name) 
    {
        std::shared_ptr<Material> outMaterial = nullptr;
        auto entry = singleton->m_materialMap.find(name);
        if (entry == singleton->m_materialMap.end()) 
        {
            outMaterial = std::make_shared<Material>(name);
            singleton->m_materialMap.insert({ name, outMaterial });
        }
        else
        {
            entry->second;
        }
        return outMaterial;
    }
}