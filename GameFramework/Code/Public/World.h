#pragma once

#include <string>
#include <memory>

#include "Core.h"
#include "GameFramework.h"

namespace Cyan
{
    class IScene;

    class GAMEFRAMEWORK_API World
    {
    public:
        World(const char* name);
        ~World();

        void update();

        std::string m_name;
        std::unique_ptr<IScene> m_scene = nullptr;
        i32 m_frameCount = 0;
        f32 m_elapsedTime = 0.f;
        f32 m_deltaTime = 0.f;
    };
}
