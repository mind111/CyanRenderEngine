#pragma once

#include "App.h"
#include "SparseVoxelOctreeDemo.h"

#define APP_ASSET_PATH "C:/dev/cyanRenderEngine/EngineApp/Apps/VoxelBasedGI/Resources/"
#define APP_SHADER_PATH "C:/dev/cyanRenderEngine/EngineApp/Apps/VoxelBasedGI/Shader/"

namespace Cyan
{
    class VoxelBasedGI : public App
    {
    public:
        VoxelBasedGI(u32 windowWidth, u32 windowHeight);
        virtual ~VoxelBasedGI();

        virtual void update(World* world) override;
        virtual void render() override;
    protected:
        virtual void customInitialize(World* world) override;
    private:
        void experiementA();
        void experiementB();
        void experiementC();
        void experiementD();
        void experiementE();
        void experiementF();
        void experiementG();

        std::unique_ptr<SparseVoxelOctreeDemo> m_sparseVoxelOctreeDemo = nullptr;
    };
}

