#include "stbi/stb_image.h"

#include "World.h"
#include "StaticMeshEntity.h"
#include "StaticMeshComponent.h"
#include "gltf.h"

#include "AssetManager.h"
#include "StaticMesh.h"
#include "Image.h"
#include "Texture.h"
#include "GfxHardwareAbstraction/GHInterface/GHTexture.h"
#include "Material.h"

namespace Cyan
{
    AssetManager::AssetManager()
    {

    }

    AssetManager::~AssetManager()
    {

    }

    std::unique_ptr<AssetManager> AssetManager::s_instance = nullptr;
    AssetManager* AssetManager::get()
    {
        if (s_instance == nullptr)
        {
            s_instance = std::unique_ptr<AssetManager>(new AssetManager());
        }
        return s_instance.get();
    }

    void AssetManager::initialize()
    {
        importGltf("C:/dev/cyanRenderEngine/Engine/Resources/Shapes.glb");
        stbi_set_flip_vertically_on_load(1);
    }

    void AssetManager::import(World* world, const char* filename)
    {
        // parse file extension
        std::string path(filename);
        u32 found = static_cast<u32>(path.find_last_of('.'));
        std::string extension = path.substr(found, found + 1);
        if (extension == ".gltf" || extension == ".glb")
        {
            s_instance->importGltf(world, filename);
        }
        else if (extension == ".obj")
        {
        }
    }

    static void importGltfNode(World* world, gltf::Gltf& gltf, Entity* parent, const gltf::Node& node)
    {
        // @name
        std::string m_name = node.m_name;

        // @transform
        Transform localTransform;
        if (node.hasMatrix >= 0)
        {
            const std::array<f32, 16>& m = node.matrix;
            glm::mat4 mat = {
                glm::vec4(m[0],  m[1],  m[2],  m[3]),     // column 0
                glm::vec4(m[4],  m[5],  m[6],  m[7]),     // column 1
                glm::vec4(m[8],  m[9],  m[10], m[11]),    // column 2
                glm::vec4(m[12], m[13], m[14], m[15])     // column 3
            };
            localTransform.fromMatrix(mat);
        }
        else
        {
            glm::vec3 scale(1.f); glm::vec4 rotation(0.f, 0.f, 0.f, 1.f); glm::vec3 translation(0.f);
            // @scale
            if (node.hasScale >= 0)
            {
                scale = node.scale;
            }
            // @rotation
            if (node.hasRotation >= 0)
            {
                rotation = node.rotation;
            }
            // @translation
            if (node.hasTranslation >= 0)
            {
                translation = node.translation;
            }
            localTransform.scale = scale;
            localTransform.rotation = glm::quat(rotation.w, glm::vec3(rotation.x, rotation.y, rotation.z));
            localTransform.translation = translation;
        }
        // @mesh
        std::shared_ptr<Entity> e = nullptr;
        if (node.mesh >= 0)
        {
           const gltf::Mesh& gltfMesh = gltf.m_meshes[node.mesh];
           auto mesh = AssetManager::findAsset<StaticMesh>(gltfMesh.m_name.c_str());
           std::shared_ptr<StaticMeshEntity> staticMeshEntity = world->createEntity<StaticMeshEntity>(m_name.c_str(), localTransform);
           // setup mesh
           StaticMeshComponent* c = staticMeshEntity->getStaticMeshComponent();
           c->setStaticMesh(mesh);
           e = staticMeshEntity;
           // todo: setup parent
           if (parent != nullptr)
           {
               parent->attachChild(e);
           }
           // setup materials
           for (i32 p = 0; p < gltfMesh.primitives.size(); ++p)
           {
               const gltf::Primitive& primitive = gltfMesh.primitives[p];
               if (primitive.material >= 0)
               {
                   const gltf::Material& gltfMatl = gltf.m_materials[primitive.material];
                   auto mi = AssetManager::findAsset<MaterialInstance>(gltfMatl.m_name.c_str());
                   staticMeshEntity->getStaticMeshComponent()->setMaterial(p, mi);
               }
           }
        }
        else
        {
           e = world->createEntity<Entity>(node.m_name.c_str(), localTransform);
        }

        if (parent != nullptr)
        {
            parent->attachChild(e);
        }

        // recurse into children nodes
        for (auto child : node.children)
        {
            importGltfNode(world, gltf, e.get(), gltf.m_nodes[child]);
        }
    }

    void AssetManager::importGltf(const char* gltfFilename)
    {
        std::string path(gltfFilename);
        u32 found = static_cast<u32>(path.find_last_of('.'));
        std::string extension = path.substr(found, found + 1);
        if (extension == ".gltf")
        {
        }
        else if (extension == ".glb")
        {
            // glb needs to persist until all async tasks are finished
            auto glb = std::make_shared<gltf::Glb>(gltfFilename);
            glb->load();

            // async import meshes
            for (i32 m = 0; m < glb->m_meshes.size(); ++m)
            {
                const auto& gltfMesh = glb->m_meshes[m];
                std::string meshName = gltfMesh.m_name; // meshName cannot be empty here since it's handled in glb->load()
                u32 numSubMeshes = static_cast<u32>(gltfMesh.primitives.size());
                // todo: this call needs to preallocate all the subMeshes
                auto mesh = AssetManager::createStaticMesh(meshName.c_str(), numSubMeshes);

                // todo: this can be run on a worker thread
                for (u32 sm = 0; sm < numSubMeshes; ++sm)
                {
                    const gltf::Primitive& p = glb->m_meshes[m].primitives[sm];
                    switch ((gltf::Primitive::Mode)p.mode)
                    {
                    case gltf::Primitive::Mode::kTriangles:
                    {
                        auto triangles = std::make_unique<Triangles>();
                        glb->importTriangles(p, *triangles);
                        auto subMesh = (*mesh)[sm];
                        subMesh->setGeometry(std::move(triangles));
                    } break;
                    case gltf::Primitive::Mode::kLines:
                    case gltf::Primitive::Mode::kPoints:
                    default:
                        assert(0);
                    }
                }
            }

            for (i32 i = 0; i < glb->m_images.size(); ++i)
            {
                const auto& gltfImage = glb->m_images[i];
                std::string imageName = gltfImage.m_name;
                auto image = AssetManager::createImage(imageName.c_str());
                glb->importImage(gltfImage, *image);
            }

            for (i32 i = 0; i < glb->m_textures.size(); ++i)
            {
                const gltf::Texture& gltfTexture = glb->m_textures[i];
                const gltf::Image& gltfImage = glb->m_images[gltfTexture.source];
                const gltf::Sampler& gltfSampler = glb->m_samplers[gltfTexture.sampler];
                std::string textureName = gltfTexture.m_name;
                auto image = AssetManager::findAsset<Image>(gltfImage.m_name.c_str());

                GHSampler2D sampler;
                bool bGenerateMipmap = false;
                gltf::translateSampler2D(gltfSampler, sampler, bGenerateMipmap);
                auto texture = AssetManager::createTexture2D(textureName.c_str(), sampler, image, bGenerateMipmap);
            }

            glb->importMaterials();
        }
    }

    void AssetManager::importGltf(World* world, const char* gltfFilename)
    {
        std::string path(gltfFilename);
        u32 found = static_cast<u32>(path.find_last_of('.'));
        std::string extension = path.substr(found, found + 1);
        if (extension == ".gltf")
        {
        }
        else if (extension == ".glb")
        {
            // glb needs to persist until all async tasks are finished
            auto glb = std::make_shared<gltf::Glb>(gltfFilename);
            glb->load();

            // async import meshes
            for (i32 m = 0; m < glb->m_meshes.size(); ++m)
            {
                const auto& gltfMesh = glb->m_meshes[m];
                std::string meshName = gltfMesh.m_name; // meshName cannot be empty here since it's handled in glb->load()
                u32 numSubMeshes = static_cast<u32>(gltfMesh.primitives.size());
                // todo: this call needs to preallocate all the subMeshes
                auto mesh = AssetManager::createStaticMesh(meshName.c_str(), numSubMeshes);

                // todo: this can be run on a worker thread
                for (u32 sm = 0; sm < numSubMeshes; ++sm)
                {
                    const gltf::Primitive& p = glb->m_meshes[m].primitives[sm];
                    switch ((gltf::Primitive::Mode)p.mode)
                    {
                    case gltf::Primitive::Mode::kTriangles:
                    {
                        auto triangles = std::make_unique<Triangles>();
                        glb->importTriangles(p, *triangles);
                        auto subMesh = (*mesh)[sm];
                        subMesh->setGeometry(std::move(triangles));
                    } break;
                    case gltf::Primitive::Mode::kLines:
                    case gltf::Primitive::Mode::kPoints:
                    default:
                        assert(0);
                    }
                }
            }

            for (i32 i = 0; i < glb->m_images.size(); ++i)
            {
                const auto& gltfImage = glb->m_images[i];
                std::string imageName = gltfImage.m_name;
                auto image = AssetManager::createImage(imageName.c_str());
                glb->importImage(gltfImage, *image);
            }

            for (i32 i = 0; i < glb->m_textures.size(); ++i)
            {
                const gltf::Texture& gltfTexture = glb->m_textures[i];
                const gltf::Image& gltfImage = glb->m_images[gltfTexture.source];
                const gltf::Sampler& gltfSampler = glb->m_samplers[gltfTexture.sampler];
                std::string textureName = gltfTexture.m_name;
                auto image = AssetManager::findAsset<Image>(gltfImage.m_name.c_str());

                GHSampler2D sampler;
                bool bGenerateMipmap = false;
                gltf::translateSampler2D(gltfSampler, sampler, bGenerateMipmap);
                auto texture = AssetManager::createTexture2D(textureName.c_str(), sampler, image, bGenerateMipmap);
            }

            glb->importMaterials();

            // sync import scene nodes
            if (glb->m_defaultScene >= 0)
            {
                const gltf::Scene& gltfScene = glb->m_scenes[glb->m_defaultScene];
                for (i32 i = 0; i < gltfScene.m_nodes.size(); ++i)
                {
                    const gltf::Node& node = glb->m_nodes[gltfScene.m_nodes[i]];
                    importGltfNode(world, *glb, nullptr, node);
                }
            }
        }
    }

    StaticMesh* AssetManager::createStaticMesh(const char* name, u32 numSubMeshes)
    {
        StaticMesh* found = findAsset<StaticMesh>(name);
        // assert(found == nullptr);

        StaticMesh* outMesh = new StaticMesh(name, numSubMeshes);
        s_instance->m_residentAssetMap.insert({ outMesh->getName(), outMesh });
        return outMesh;
    }

    Image* AssetManager::createImage(const char* name)
    {
        Image* found = findAsset<Image>(name);
        // assert(found == nullptr);

        Image* outImage = new Image(name);
        s_instance->m_residentAssetMap.insert({ outImage->getName(), outImage });
        return outImage;
    }

    Texture2D* AssetManager::createTexture2D(const char* name, const GHSampler2D& sampler2D, Image* image, bool bGenerateMipmap)
    {
        Texture2D* found = findAsset<Texture2D>(name);
        assert(found == nullptr);

        Texture2D* outTex2D = new Texture2D(name, sampler2D, image, bGenerateMipmap);
        s_instance->m_residentAssetMap.insert({ outTex2D->getName(), outTex2D });
        return outTex2D;
    }

    Texture2D* AssetManager::createTexture2D(const char* name, std::unique_ptr<GHTexture2D> GHTexture)
    {
        Texture2D* found = findAsset<Texture2D>(name);
        assert(found == nullptr);

        Texture2D* outTex2D = new Texture2D(name, std::move(GHTexture));
        s_instance->m_residentAssetMap.insert({ outTex2D->getName(), outTex2D });
        return outTex2D;
    }

    Texture2D* AssetManager::createTexture2DFromImage(const char* imageFilePath, const char* textureName, const GHSampler2D& sampler2D)
    {
        auto image = createImage(imageFilePath);
        image->importFromFile(imageFilePath);
        bool bGenerateMipmap = false;
        SamplerFilteringMode minFM = sampler2D.getFilteringModeMin();
        switch (minFM)
        {
        case SamplerFilteringMode::BilinearMipmapPoint:
        case SamplerFilteringMode::PointMipmapPoint:
        case SamplerFilteringMode::Trilinear: bGenerateMipmap = true; break;
        default: break;
        }
        return createTexture2D(textureName, sampler2D, image, bGenerateMipmap);
    }

    Material* AssetManager::createMaterial(const char* name, const char* materialSourcePath, const MaterialSetupFunc& setupFunc)
    {
        Material* found = findAsset<Material>(name);
        assert(found == nullptr);

        Material* outMaterial = new Material(name, materialSourcePath, setupFunc);
        s_instance->m_residentAssetMap.insert({ outMaterial->getName(), outMaterial });
        return outMaterial;
    }

    MaterialInstance* AssetManager::createMaterialInstance(const char* name, Material* parent)
    {
        Material* m = findAsset<Material>(parent->getName());
        assert(m != nullptr);
        MaterialInstance* mi = findAsset<MaterialInstance>(name);
        assert(mi == nullptr);

        mi = m->createInstance(name);
        s_instance->m_residentAssetMap.insert({ mi->getName(), mi });
        return mi;
    }
}