#pragma once

#include <vector>
#include <queue>
#include <unordered_map>
#include <functional>
#include <memory>

#include "Common.h"
#include "Allocator.h"

namespace Cyan
{
    // todo: this design requries new() operator for creating IEvent instances, so probably need to use a
    // transient allocator with a placement new
    struct IEvent
    {
        virtual ~IEvent() { }
        virtual std::string getTypeDesc() { }
    };

    /*
    * A listener is essentially a wrapper of a function pointer. It subscribes to a type of event not a particular IEvent instance.
    * However, per IEvent instance data is required to notify a listener.
    */
    struct IListener
    {
        IListener() { }
        virtual ~IListener() { }
        virtual void notify(IEvent* event) { }
    };

    template <typename ... Args>
    struct TListener : public IListener
    {
        TListener(const std::function<void(Args...)>& func) 
            : callback(func)
        { 

        }
        
        virtual void notify(IEvent* event) override 
        { 

        }

        std::function<void(Args...)> callback;
    };

    // todo: how to manage the lifetime of an IEvent instance 
    struct EventDispatcher
    {
        EventDispatcher() 
        { 
            allocator = std::make_unique<LinearAllocator>(memoryPoolSize);
        }

        template<typename ConcreteEventType>
        void registerEventType()
        {
            // register an new type of event that should be managed by this EventDispatcher instance 
            std::string eventTypeDesc = ConcreteEventType::getTypeDescStatic();
            if (listenerMap.find(eventTypeDesc) == listenerMap.end())
            {
                listenerMap.insert({ eventTypeDesc, { } });
            }
        }

        template <typename ConcreteEventType>
        void addEventListener(const typename ConcreteEventType::Handler& handler) 
        { 
            // no need verify whether 'listener' is compatible with the event type 
            // as it's done in ConcreteEventType::Listener::notify()
            auto entry = listenerMap.find(ConcreteEventType::getTypeDescStatic());
            if (entry != listenerMap.end())
            {
                const auto& listenerList = *entry->second;
                listenerList.push_back(ConcreteEventType::createListener(handler));
            }
        }

        template <typename ConcreteEventType, typename ... Args>
        IEvent* createAndEnqueEvent(Args... args)
        {
            // won't create an instance of unregistered event type
            if (listenerMap.find(ConcreteEventType::getTypeDescStatic()) != listenerMap.end())
            {
                ConcreteEventType* newEvent = allocator->alloc<ConcreteEventType>(args...);
                eventQueue.push(newEvent);
                return newEvent;
            }
            return nullptr;
        }

        void enqueEvent(IEvent* event) 
        { 
            eventQueue.push(event);
        }

        void dispatch()
        { 
            while (!eventQueue.empty())
            {
                auto event = eventQueue.front();
                std::string typeDesc = event->getTypeDesc();
                auto entry = listenerMap.find(typeDesc);
                if (entry != listenerMap.end())
                {
                    const auto& listenerList = entry->second;
                    for (u32 i = 0; i < listenerList.size(); ++i)
                    {
                        listenerList[i]->notify(event);
                    }
                }
                eventQueue.pop();
                event->~IEvent();
            }

            // reset allocator as we processed for the event
            allocator->reset();
        }

    private:
        // 1 kb of memory for the linear allocator by default
        static constexpr u32 memoryPoolSize = 1024 * 1024;
        std::unique_ptr<LinearAllocator> allocator;
        // todo: if event queue stores pointers to actual event object then how to manage the lifetime of event instance that was passed in
        std::queue<IEvent*> eventQueue;
        std::unordered_map<std::string, std::vector<IListener*>> listenerMap;
    };
}
