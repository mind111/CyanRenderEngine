#pragma once

#include <unordered_map>
#include <stack>

#include "Common.h"

namespace Cyan
{
    /*
    * allocator using built-in heap allocation through "new" operator
    */
    template <typename T>
    class DefaultHeapAllocator
    {
    public:

        DefaultHeapAllocator() = default;
        ~DefaultHeapAllocator() { }

        // todo: this should be able to take in arguments to passed to T's constructor
        T* alloc()
        {
            return new T();
        }
    private:
    };

    // todo: this should take in / forward the arguments for constructing an object
    template <typename T, u32 kMaxNumObjects>
    class ObjectPool
    {
    public:

        ObjectPool()
        {
            m_objects.resize(kMaxNumAllocations);
        }

        T* alloc()
        {
            // todo: figure out what to do if we reach the amount of objects that we can allocate
            if (numAllocated >= kMaxNumAllocations)
            {
                return nullptr;
            }
            if (!freeObjectList.empty())
            {
                u32 reused = freeObjectList.top();
                freeObjectList.pop();
                T* newObject = &m_objects[reused];
                numAllocated += 1;
                return newObject;
            }
            u32 allocatedIndex = numAllocated;
            T* newObject = &m_objects[numAllocated++];
            allocationMap.insert({ newObject, allocatedIndex });
            return newObject;
        }

        void free(T* allocated)
        {
            auto entry = allocationMap.find(allocated);
            if (entry != allocationMap.end())
            {
                freeObjectList.push(entry->second);
                allocationMap.erase(allocated);
                numAllocated -= 1;
            }

            // error:
            cyanError("Trying to free an object that is not allocated")
        }

        T& getObject(u32 index)
        {
            return m_objects[index];
        }

        i32 getObjectIndex(T* objectPtr)
        {
            auto entry = allocationMap.find(objectPtr);
            if (entry == allocationMap.end())
            {
                return -1;
            }
            return entry->second;
        }

        const std::vector<T>& getObjects()
        {
            return m_objects;
        }

    private:
        static const u32 kMaxNumAllocations = kMaxNumObjects;
        std::unordered_map<T*, u32> allocationMap;
        std::stack<u32> freeObjectList;
        u32 numAllocated = 0;
        std::vector<T> m_objects;
    };

    /*
    * Linear allocator that uses placement new
    */
    class LinearAllocator
    {
    public:
        LinearAllocator(u32 inSize)
            : sizeInBytes(inSize)
        {
            memory = new u8[sizeInBytes];
        }

        ~LinearAllocator() 
        { 
            delete[] memory;
        }

        template <typename T, typename ... Args>
        T* alloc(Args... args)
        {
            if (pos + sizeof(T) <= sizeInBytes)
            {
                // placement new
                T* newObject = new (&memory[pos]) T(args...);
                pos += sizeof(T);
                return newObject;
            }

            cyanError("LinearAllocator out of memory")
            return nullptr;
        }

        void reset()
        {
            pos = 0u;
        }

    private:
        u32 sizeInBytes = 0u;
        u32 pos = 0u;
        u8* memory = nullptr;
    };
}

// no fragmentation within the memory managed by a StackAllocator
struct StackAllocator
{
    StackAllocator(u32 nBytes)
    {
        m_pos = 0u;
        m_size = nBytes;
        // TODO: alloc from memory pool instead
        m_bytes = new u8[nBytes];
        // clear to 0
        memset(m_bytes, 0x0, m_size);
    }

    ~StackAllocator()
    {
        delete[] m_bytes;
    }

    void* alloc(u32 size)
    {
        // 4 additional bytes for position marker stored at the end of each allocated region
        if (m_pos + (size + 4u) <= m_size)
        {
            m_numAllocations += 1;
            // cache the size of this allocation at the end of allocated region
            u32* markerPtr = reinterpret_cast<u32*>(&m_bytes[m_pos + size]);
            *markerPtr = m_pos;
            void* address = reinterpret_cast<void*>(m_bytes + m_pos); 
            m_pos += (size + 4u);
            return address;
        }
        CYAN_ASSERT(0, "StackAllocator out of memory");
        return nullptr;
    }

    // don't need to think about free for now
    void free()
    {
        if (m_pos > 0u)
        {
            m_numAllocations -= 1;
            u32* markerPtr = reinterpret_cast<u32*>(&m_bytes[m_pos - 4u]);
            u32 size = *markerPtr;
            m_pos -= (size + 4u);
        }
    }

    void reset()
    {
        m_pos = 0u;
    }

    u32 m_pos;
    u32 m_numAllocations;
    u32 m_size;
    u8* m_bytes;
};