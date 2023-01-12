#pragma once

#include "Common.h"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

namespace Cyan
{
    namespace UI
    {
        void initialize(GLFWwindow* window);
        void begin();
        void draw();
        void beginWindow(const char* label, ImGuiWindowFlags flags = 0);
        void endWindow();
        void comboBox(const char* items[], u32 numItems, const char* label, u32* currentIndex);
        bool button(const char* label);
        bool header(const char* label);

        extern ImFont* gFont;
    }
}