#include "CyanUI.h"

namespace Cyan
{
    namespace UI
    {
        ImFont* gFont = nullptr;

        void initialize(GLFWwindow* window)
        {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGui::StyleColorsDark();
            ImGui::GetStyle().WindowRounding = 0.0f;
            ImGui::GetStyle().ChildRounding = 0.0f;
            ImGui::GetStyle().FrameRounding = 0.0f;
            ImGui::GetStyle().GrabRounding = 0.0f;
            ImGui::GetStyle().PopupRounding = 0.0f;
            ImGui::GetStyle().ScrollbarRounding = 0.0f;

            ImGui_ImplGlfw_InitForOpenGL(window, true);
            ImGui_ImplOpenGL3_Init(nullptr);

            // font
            ImGuiIO& io = ImGui::GetIO();
            gFont = io.Fonts->AddFontFromFileTTF("C:\\dev\\cyanRenderEngine\\asset\\fonts\\Roboto-Medium.ttf", 14.f);
        }

        void begin()
        {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
        }
        
        void draw()
        {

        }

        void beginWindow(const char* label, ImGuiWindowFlags flags)
        {
            ImGui::Begin(label, 0, flags);
        }

        void endWindow()
        {
            ImGui::End();
        }

        void comboBox(const char* items[], u32 numItems, const char* label, u32* currentIndex)
        {
            /*
                Note: For some reasons, @label has to be passed in or else having 
                multiple combo boxes will not work 
                Guide on how to work around widgets' labels: https://github.com/ocornut/imgui/blob/master/docs/FAQ.md#q-why-is-my-widget-not-reacting-when-i-click-on-it
            */
            ImGui::Text(label);
            ImGui::SameLine();
            u32 labelLength = strlen(label);
            char* hiddenLabel = (char*)_alloca(labelLength + 3);
            strcpy(hiddenLabel, "##");
            strcat(hiddenLabel, label);
            if (ImGui::BeginCombo(hiddenLabel, items[*currentIndex], 0))
            {
                for (int n = 0; n < numItems; n++)
                {
                    const bool isSelected = (*currentIndex == n);
                    if (ImGui::Selectable(items[n], isSelected))
                    {
                        *currentIndex = n;
                    }
                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        bool button(const char* label)
        {
            return ImGui::Button(label);
        }

        bool header(const char* label)
        {
            return ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
        }

    }
}