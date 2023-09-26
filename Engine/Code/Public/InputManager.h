#pragma once

#include "LowLeveInputs.h"

namespace Cyan
{
    class InputManager
    {
    public:
        using KeyEventListener = std::function<void(LowLevelKeyEvent*)>;
        using MouseCursorEventListener = std::function<void(LowLevelMouseCursorEvent*)>;
        using MouseButtonEventListener = std::function<void(LowLevelMouseButtonEvent*)>;

        static InputManager* get();

        void registerKeyEventListener(char key, const KeyEventListener& listener);
        void registerMouseCursorEventListener(const MouseCursorEventListener& listener);
        void registerMouseButtonEventListener(const MouseButtonEventListener& listener);

        void processKeyEvent(LowLevelKeyEvent* keyEvent);
        void processMouseCursorEvent(LowLevelMouseCursorEvent* cursorEvent);
        void processMouseButtonEvent(LowLevelMouseButtonEvent* buttonEvent);
    private:
        InputManager() = default;

        std::unordered_map<char, std::vector<KeyEventListener>> m_keyEventListeners;
        std::vector<MouseCursorEventListener> m_mouseCursorListeners;
        std::vector<MouseButtonEventListener> m_mouseButtonListeners;

        static InputManager* s_instance;
    };
}
