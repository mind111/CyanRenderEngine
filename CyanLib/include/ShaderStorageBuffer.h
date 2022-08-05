#pragma once

#include "glew.h"

#include "Common.h"

#define SET_SSBO_MEMBER(ssboName, memberName, data) ssboName.ssboStruct.staticChunk.memberName = data
#define SET_SSBO_ELEMENT(ssboName, index, data) ssboName.ssboStruct.dynamicArray[index] = data
#define ADD_SSBO_ELEMENT(ssboName, data) ssboName.ssboStruct.dynamicArray.push_back(data)
#define GET_SSBO_ELEMENT(ssboName, index) ssboName.ssboStruct.dynamicArray[index]

namespace Cyan
{
    struct IShaderStorageBufferStruct
    {
        virtual void reserveDynamicChunk(u32 numElements) { }
        virtual bool hasDynamicChunk() { return false; }
        virtual void* getStaticChunkData() { return nullptr; }
        virtual void* getDynamicChunkData() { return nullptr; }
        virtual u32 getStaticChunkSizeInBytes() { return 0; }
        virtual u32 getDynamicChunkSizeInBytes() { return 0; };
        virtual u32 getSizeInBytes() { return getStaticChunkSizeInBytes() + getDynamicChunkSizeInBytes(); }
    };

    template <typename StaticType>
    struct StaticSsboStruct : public IShaderStorageBufferStruct
    {
        virtual void* getStaticChunkData() override
        {
            return reinterpret_cast<void*>(&staticChunk);
        }

        virtual u32 getStaticChunkSizeInBytes() override
        {
            return sizeof(StaticType);
        }

        StaticType staticChunk;
    };

    template <typename DynamicType>
    struct DynamicSsboStruct : public IShaderStorageBufferStruct
    {
        virtual bool hasDynamicChunk() override
        {
            return true;
        }

        virtual void* getDynamicChunkData() override
        {
            return dynamicArray.data();
        }

        virtual u32 getDynamicChunkSizeInBytes() override
        {
            return sizeOfVector(dynamicArray);
        }

        u32 getNumElements() { return dynamicArray.size(); }

        void addElement(const DynamicType& element)
        {
            dynamicArray.push_back(element);
        }

        DynamicType& operator[](u32 index)
        {
            return dynamicArray[index];
        }

        std::vector<DynamicType> dynamicArray;
    };

#if 0
    template <typename StaticType, typename DynamicType>
    struct CompoundSsboStruct : public IShaderStorageBufferStruct
    {       
        virtual void reserveDynamicChunk(u32 numElements) 
        { 
            dynamicArray.reserve(numElements);
        }

        virtual bool hasDynamicChunk() override
        {
            return true;
        }

        virtual void* getStaticChunkData() override
        {
            return reinterpret_cast<void*>(&staticChunk);
        }

        virtual void* getDynamicChunkData() override
        {
            return dynamicArray.data();
        }

        virtual u32 getStaticChunkSizeInBytes() override
        {
            return sizeof(StaticType);
        }

        virtual u32 getDynamicChunkSizeInBytes() override
        {
            return sizeOfVector(dynamicArray);
        }

        virtual u32 getSizeInBytes() override
        {
            return getStaticChunkSizeInBytes() + getDynamicChunkSizeInBytes();
        }

        static StaticType staticChunk;
        static std::vector<DynamicType> dynamicArray;
    };
#else
    template <typename StaticType, typename DynamicType>
    struct CompoundSsboStruct : public StaticSsboStruct<StaticType>, public DynamicSsboStruct<DynamicType>
    {
        virtual bool hasDynamicChunk() override
        {
            return DynamicSsboStruct<DynamicType>::hasDynamicChunk();
        }

        virtual void* getStaticChunkData() override
        {
            return StaticSsboStruct<StaticType>::getStaticChunkData();
        }

        virtual void* getDynamicChunkData() override
        {
            return DynamicSsboStruct<DynamicType>::getDynamicChunkData();
        }

        virtual u32 getStaticChunkSizeInBytes() override
        {
            return StaticSsboStruct<StaticType>::getSizeInBytes();
        }

        virtual u32 getDynamicChunkSizeInBytes() override
        {
            return DynamicSsboStruct<DynamicType>::getSizeInBytes();
        }

        virtual u32 getSizeInBytes() override
        {
            return getStaticChunkSizeInBytes() + getDynamicChunkSizeInBytes();
        }
    };
#endif

    template <typename SsboStructType>
    struct ShaderStorageBuffer : public GpuResource
    {
        ShaderStorageBuffer(u32 numElements = 0)
        {
            sizeInBytes = ssboStruct.getSizeInBytes();

            glCreateBuffers(1, &glResource);
            // todo: the hint flag need to be configurable
            glNamedBufferData(glResource, sizeInBytes, nullptr, GL_DYNAMIC_DRAW);
        }

        ~ShaderStorageBuffer()
        {
            glDeleteBuffers(1, &glResource);
        }

        void bind(u32 inBufferBindingUnit)
        {
            bufferBindingUnit = (i32)inBufferBindingUnit;
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)bufferBindingUnit, glResource);
        }

        void unbind()
        {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bufferBindingUnit, 0);
            bufferBindingUnit = -1;
        }

        void update()
        {
            // if the dynamic array out grow the allocated buffer, need to release old resources and create new ones
            if (ssboStruct.getSizeInBytes() > sizeInBytes)
            {
                sizeInBytes = ssboStruct.getSizeInBytes();
                // remove old gpu resources
                glDeleteBuffers(1, &glResource);

                // create new gpu resources
                glCreateBuffers(1, &glResource);
                glNamedBufferData(glResource, sizeInBytes, nullptr, GL_DYNAMIC_DRAW);
            }

            if (ssboStruct.getStaticChunkSizeInBytes() > 0)
            {
                // update static members
                glNamedBufferSubData(getGpuResource(), 0, ssboStruct.getStaticChunkSizeInBytes(), ssboStruct.getStaticChunkData());
                if (ssboStruct.getDynamicChunkSizeInBytes() > 0)
                    // update dynamic members
                    glNamedBufferSubData(getGpuResource(), ssboStruct.getStaticChunkSizeInBytes(), ssboStruct.getDynamicChunkSizeInBytes(), ssboStruct.getDynamicChunkData());
            }
            else
            {
                u32 dynamicSize = ssboStruct.getDynamicChunkSizeInBytes();
                // update dynamic members
                glNamedBufferSubData(getGpuResource(), 0, ssboStruct.getDynamicChunkSizeInBytes(), ssboStruct.getDynamicChunkData());
            }
        }

        u32 getNumElements() { return ssboStruct.getNumElements(); }

        template <typename T>
        void addElement(const T& element)
        {
            return ssboStruct.addElement(element);
        }

        auto& operator[](u32 index) {
            return ssboStruct[index];
        }

        SsboStructType ssboStruct;
        u32 sizeInBytes = 0;
        i32 bufferBindingUnit = -1;
    };
}
