#include "InputManager.h"

namespace Cyan
{
    InputManager* InputManager::s_instance = nullptr;
    InputManager* InputManager::get()
    {
        static std::unique_ptr<InputManager> instance(new InputManager());
        if (s_instance == nullptr)
        {
            s_instance = instance.get();
        }
        return s_instance;
    }

    void InputManager::registerKeyEventListener(char key, const KeyEventListener& listener)
    {
        auto entry = m_keyEventListeners.find(key);
        if (entry != m_keyEventListeners.end())
        {
            std::vector<KeyEventListener>& listeners = entry->second;
            listeners.push_back(listener);
        }
        else
        {
            m_keyEventListeners[key] = std::vector<KeyEventListener>({ listener });
        }
    }

    void InputManager::registerMouseCursorEventListener(const MouseCursorEventListener& listener)
    {
        m_mouseCursorListeners.push_back(listener);
    }

    void InputManager::registerMouseButtonEventListener(const MouseButtonEventListener& listener)
    {
        m_mouseButtonListeners.push_back(listener);
    }

    void InputManager::processKeyEvent(LowLevelKeyEvent* keyEvent)
    {
        std::vector<KeyEventListener>& listeners = m_keyEventListeners[keyEvent->key];
        for (auto& listener : listeners)
        {
            listener(keyEvent);
        }
    }

    void InputManager::processMouseCursorEvent(LowLevelMouseCursorEvent* cursorEvent)
    {
        for (auto& listener : m_mouseCursorListeners)
        {
            listener(cursorEvent);
        }
    }

    void InputManager::processMouseButtonEvent(LowLevelMouseButtonEvent* buttonEvent)
    {
        for (auto& listener : m_mouseButtonListeners)
        {
            listener(buttonEvent);
        }
    }
}