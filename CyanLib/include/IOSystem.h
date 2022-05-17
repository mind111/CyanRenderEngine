#pragma once

#include <vector>
#include <queue>
#include <functional>

#include "Common.h"
#include "System.h"
#include "Event.h"

namespace Cyan
{
    typedef void MouseCursorListener(f64, f64, f64, f64);
    typedef void MouseButtonListener(i32, i32);
    typedef void MouseWheelListener(f64, f64);
    typedef void KeyListener(i32, i32);

#if 0
    struct IOEvent
    {
        enum class Type
        {
            kMouseCursor = 0,
            kMouseButton,
            kMouseWheel,
            kKey,
            kCount
        } type = Type::kCount;

        i32 key = -1;
        i32 button = -1;
        i32 action = -1;
        f64 cursorX = 0.0;
        f64 cursorY = 0.0;
        f64 xOffset = 0.0;
        f64 yOffset = 0.0;
    };
#endif
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
                if (event->getTypeDesc().compare("MouseButtontEvent") == 0)
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

    class IOSystem : public System
    {
    public:
        IOSystem();

        virtual void initialize() override;
        virtual void update() override;
        virtual void finalize() override;

        static IOSystem* get() { return singleton; }

#if 0
        void enqueIOEvent(const IOEvent& event);
#else
        template <typename IOEventType, typename ... Args>
        void enqueIOEvent(Args... args)
        {
            m_ioEventDispatcher->createAndEnqueEvent<IOEventType>(args...);
        }
#endif

    private:
        struct MouseCursor
        {
            f64 x = 0.0;
            f64 y = 0.0;
            f64 dx = 0.0;
            f64 dy = 0.0;
        } m_mouseCursorState;

        EventDispatcher* m_ioEventDispatcher = nullptr;
        static IOSystem* singleton;
    };
}
