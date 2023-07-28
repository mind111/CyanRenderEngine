#pragma once

#include "Core.h"
#include "MathLibrary.h"

namespace Cyan
{
    /**
     * User inputs
     */
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

    struct ILowLevelInputEvent
    {
        enum class Type
        {
            kKeyEvent = 0,
            kMouseCursor,
            kMouseButton,
            kInvalid
        };

        Type getEventType() { return eventType; }

    protected:
        ILowLevelInputEvent(const Type& type)
            : eventType(type)
        {
        }

        Type eventType = Type::kInvalid;
    };

    struct LowLevelKeyEvent : public ILowLevelInputEvent
    {
        LowLevelKeyEvent(const char& inKey, const InputAction& inAction)
            : ILowLevelInputEvent(Type::kKeyEvent), key(inKey), action(inAction)
        {
        }

        void debugPrint()
        {
            switch (action)
            {
            case InputAction::kPress:
                cyanInfo("%c key is pressed", key);
                break;
            case InputAction::kRepeat:
                cyanInfo("%c key is repeated", key);
                break;
            case InputAction::kRelease:
                cyanInfo("%c key is released", key);
                break;
            default:
                break;
            }
        }

        char key;
        InputAction action;
    };

    struct LowLevelMouseCursorEvent : public ILowLevelInputEvent
    {
        LowLevelMouseCursorEvent(const glm::dvec2& inPos, const glm::dvec2& inDelta)
            : ILowLevelInputEvent(Type::kMouseCursor), position(inPos), delta(inDelta)
        {
        }

        glm::dvec2 position;
        glm::dvec2 delta;
    };

    enum class MouseButton
    {
        kMouseLeft = 0,
        kMouseRight,
        kCount
    };

    struct LowLevelMouseButtonEvent : public ILowLevelInputEvent
    {
        LowLevelMouseButtonEvent(const MouseButton& inButton, const InputAction& inAction)
            : ILowLevelInputEvent(Type::kMouseButton), button(inButton), action(inAction)
        {
        }

        MouseButton button;
        InputAction action;
    };

    using InputEventQueue = std::queue<std::unique_ptr<ILowLevelInputEvent>>;
    class InputEventHandler
    {
    public:
        InputEventHandler(const std::function<void(InputEventQueue&)>& func)
            : m_func(func)
        {

        }

        ~InputEventHandler() = default;

        void handle(InputEventQueue& eventQueue)
        {
            m_func(eventQueue);
        }
    private:
        std::function<void(InputEventQueue&)> m_func;
    };
}
