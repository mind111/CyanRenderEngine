#pragma once

#include <vector>
#include <queue>
#include <functional>

#include "mathUtils.h"

#include "Common.h"
#include "System.h"
#include "Event.h"

#define CYAN_PRESS GLFW_PRESS
#define CYAN_RELEASE GLFW_RELEASE
#define CYAN_MOUSE_BUTTON_RIGHT GLFW_MOUSE_BUTTON_RIGHT
#define CYAN_MOUSE_BUTTON_LEFT GLFW_MOUSE_BUTTON_LEFT

namespace Cyan
{
    struct IOEvent : public IEvent
    {
        virtual std::string getTypeDesc() override { return "IOEvent"; }
        static std::string  getTypeDescStatic() { return std::string("IOEvent"); }
    };

    struct MouseButtonEvent : public IOEvent
    {
        using Handler = std::function<void(i32, i32)>;

        MouseButtonEvent(i32 but, i32 act) : button(but), action(act) { }

        virtual std::string getTypeDesc() { return std::string("MouseButtonEvent"); }
        static std::string  getTypeDescStatic() { return std::string("MouseButtonEvent"); }

        struct Listener : public TListener<i32, i32>
        {
            Listener(const std::function<void(i32, i32)>& func)
                : TListener<i32, i32>(func)
            { }

            virtual void notify(IEvent* event)
            {
                // todo: would this string comparison be too slow ...?
                if (event->getTypeDesc().compare("MouseButtonEvent") == 0)
                {
                    MouseButtonEvent* mouseButtonEvent = static_cast<MouseButtonEvent*>(event);
                    callback(mouseButtonEvent->button, mouseButtonEvent->action);
                }
            }
        };

        static Listener* createListener(const Handler& func)
        {
            return new Listener(func);
        }

        i32 button = -1;
        i32 action = -1;
    };

    struct MouseWheelEvent : public IOEvent
    {
        using Handler = std::function<void(f64, f64)>;

        MouseWheelEvent(f64 x, f64 y) : xOffset(x), yOffset(y) { }

        virtual std::string getTypeDesc() { return std::string("MouseWheelEvent"); }
        static std::string getTypeDescStatic() { return std::string("MouseWheelEvent"); }

        struct Listener : public TListener<f64, f64>
        {
            Listener(const Handler& func)
                : TListener<f64, f64>(func)
            { }

            virtual void notify(IEvent* event)
            {
                // todo: would this string comparison be too slow ...?
                if (event->getTypeDesc().compare("MouseWheelEvent") == 0)
                {
                    MouseWheelEvent* mouseWheelEvent = static_cast<MouseWheelEvent*>(event);
                    callback(mouseWheelEvent->xOffset, mouseWheelEvent->yOffset);
                }
            }
        };

        static Listener* createListener(const Handler& func)
        {
            return new Listener(func);
        }

        f64 xOffset;
        f64 yOffset;
    };

    struct MouseCursorEvent : public IOEvent
    {
        using Handler = std::function<void(f64, f64)>;

        MouseCursorEvent(f64 x, f64 y) : xPos(x), yPos(y) { }

        virtual std::string getTypeDesc() { return std::string("MouseCursorEvent"); }
        static std::string getTypeDescStatic() { return std::string("MouseCursorEvent"); }

        struct Listener : public TListener<f64, f64>
        {
            Listener(const Handler& func)
                : TListener<f64, f64>(func)
            { }

            virtual void notify(IEvent* event)
            {
                if (event->getTypeDesc().compare("MouseCursorEvent") == 0)
                {
                    MouseCursorEvent* mouseCursorEvent = static_cast<MouseCursorEvent*>(event);
                    callback(mouseCursorEvent->xPos, mouseCursorEvent->yPos);
                }
            }
        };

        static Listener* createListener(const Handler& func)
        {
            return new Listener(func);
        }

        f64 xPos;
        f64 yPos;
    };

    struct KeyEvent : public IOEvent
    {
        using Handler = std::function<void(i32, i32)>;

        KeyEvent(i32 k, i32 act) : key(k), action(act) { }

        virtual std::string getTypeDesc() { return std::string("KeyEvent"); }
        static std::string getTypeDescStatic() { return std::string("KeyEvent"); }

        struct Listener : public TListener<i32, i32>
        {
            Listener(const Handler& func)
                : TListener<i32, i32>(func)
            { }

            virtual void notify(IEvent* event)
            {
                if (event->getTypeDesc().compare("MouseCursorEvent") == 0)
                {
                    KeyEvent* keyEvent = static_cast<KeyEvent*>(event);
                    callback(keyEvent->key, keyEvent->action);
                }
            }
        };

        static Listener* createListener(const Handler& func)
        {
            return new Listener(func);
        }

        i32 key;
        i32 action;
    };

    class IOSystem : public System, public Singleton<IOSystem>
    {
    public:

        IOSystem();

        virtual void initialize() override;
        virtual void update() override;
        virtual void finalize() override;

        // getters
        glm::dvec2 getMouseCursorChange() { return glm::dvec2(m_mouseCursorState.dx, m_mouseCursorState.dy); }
        bool isMouseLeftButtonDown() { return m_mouseButtonState.isLeftButtonDown; }
        bool isMouseRightButtonDown() { return m_mouseButtonState.isRightButtonDown; }

        // setters
        void mouseRightButtonDown() { m_mouseButtonState.isRightButtonDown = true; }
        void mouseRightButtonUp() { m_mouseButtonState.isRightButtonDown = false; }

        template <typename IOEventType>
        void addIOEventListener(const typename IOEventType::Handler& handler)
        {
            m_IOEventDispatcher->addEventListener<IOEventType>(handler);
        }

        template <typename IOEventType, typename ... Args>
        void enqueIOEvent(Args... args)
        {
            m_IOEventDispatcher->createAndEnqueEvent<IOEventType>(args...);
        }

    private:
        struct MouseCursor
        {
            f64 x = -1.0;
            f64 y = -1.0;
            f64 dx = 0.0;
            f64 dy = 0.0;
        } m_mouseCursorState;

        struct MouseButtonState
        {
            bool isLeftButtonDown = false;
            bool isRightButtonDown = false;
        } m_mouseButtonState;

        EventDispatcher* m_IOEventDispatcher = nullptr;
    };
}
