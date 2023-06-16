#pragma once

#include <vector>
#include <queue>
#include <functional>
#include <unordered_map>

#include "Singleton.h"
#include "Common.h"
#include "MathUtils.h"
#include "System.h"

namespace Cyan
{
    enum class InputAction
    {
        kPress = 0,
        kRepeat,
        kRelease,
        kCount
    };

#define KEY_PRESSED InputAction::kPress
#define KEY_REPEAT InputAction::kRepeat
#define KEY_RELEASE InputAction::kRelease

    struct KeyEvent
    {
        char key;
        InputAction action;
    };

    struct MouseEvent
    {
        bool bInited = false;
        glm::vec2 cursorPosition;
        glm::vec2 cursorDelta;
        using MouseButtonState = InputAction;
        MouseButtonState mouseLeftButtonState = MouseButtonState::kRelease;
        MouseButtonState mouseRightButtonState = MouseButtonState::kRelease;
    };

    class InputSystem : public Singleton<InputSystem>, public System
    {
    public:
        InputSystem();
        ~InputSystem() { };

        virtual void initialize() override;
        virtual void update() override;
        virtual void deinitialize() override;

        using KeyEventListener = std::function<void(const KeyEvent&)>;
        using MouseCursorEventListener = std::function<void(const MouseEvent&)>;
        using MouseButtonEventListener = std::function<void(const MouseEvent&)>;

        void registerKeyEventListener(char key, const KeyEventListener& listener);
        void registerMouseCursorEventListener(const MouseCursorEventListener& listener);
        void registerMouseButtonEventListener(const MouseButtonEventListener& listener);

        std::unordered_map<char, std::vector<KeyEventListener>> m_keyEventListeners;
        std::vector<MouseCursorEventListener> m_mouseCursorListeners;
        std::vector<MouseButtonEventListener> m_mouseButtonListeners;

        using MouseState = MouseEvent;
        MouseState m_mouseState;
    };
}
