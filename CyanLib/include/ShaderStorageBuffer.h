#pragma once

#include "glew.h"

#include "Common.h"

#define SET_SSBO_STATIC_MEMBER(ssboName, memberName, data) ssboName.ssboStruct.staticChunk.memberName = data;
#define SET_SSBO_DYNAMIC_ELEMENT(ssboName, index, data) ssboName.ssboStruct.dynamicArray[index] = data;
#define ADD_SSBO_DYNAMIC_ELEMENT(ssboName, data) ssboName.ssboStruct.dynamicArray.push_back(data);

namespace Cyan
{
    struct IShaderStorageBufferStruct
    {
        virtual void* getStaticChunkData() = 0;
        virtual void* getDynamicChunkData() = 0;
        virtual u32 getStaticChunkSizeInBytes() = 0;
        virtual u32 getDynamicChunkSizeInBytes() = 0;
        virtual u32 getSizeInBytes() = 0;
    };

    template <typename StaticType>
    struct StaticSsboStruct : public IShaderStorageBufferStruct
    {
        static StaticType staticChunk;

        virtual void* getStaticChunkData() override
        {
            return reinterpret_cast<void*>(&staticChunk);
        }

        virtual void* getDynamicChunkData() override
        {
            return nullptr;
        }

        virtual u32 getStaticChunkSizeInBytes() override
        {
            return sizeof(StaticType);
        }

        virtual u32 getDynamicChunkSizeInBytes() override
        {
            return 0;
        }

        virtual u32 getSizeInBytes() override
        {
            return getStaticChunkSizeInBytes() + getDynamicChunkSizeInBytes();
        }
    };

    template <typename DynamicType>
    struct DyanmicSsboStruct : public IShaderStorageBufferStruct
    {
        static StaticType staticChunk;

        virtual void* getStaticChunkData() override
        {
            return reinterpret_cast<void*>(&staticChunk);
        }

        virtual void* getDynamicChunkData() override
        {
            return nullptr;
        }

        virtual u32 getStaticChunkSizeInBytes() override
        {
            return sizeof(StaticType);
        }

        virtual u32 getDynamicChunkSizeInBytes() override
        {
            return 0;
        }

        virtual u32 getSizeInBytes() override
        {
            return getStaticChunkSizeInBytes() + getDynamicChunkSizeInBytes();
        }
    };

    template <typename SsboStructType, typename StaticType = void>
    struct ShaderStorageBuffer : public GpuResource
    {
        ShaderStorageBuffer(u32 numDynamicArrayElemenents = 0)
        {
            ssboStruct.dynamicArray.reserve(numDynamicArrayElemenents);
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
                // update dynamic members
                glNamedBufferSubData(getGpuResource(), ssboStruct.getStaticChunkSizeInBytes(), ssboStruct.getDynamicChunkSizeInBytes(), ssboStruct.getDynamicChunkData());
            }
            else
            {
                // update dynamic members
                glNamedBufferSubData(getGpuResource(), 0, ssboStruct.getDynamicChunkSizeInBytes(), ssboStruct.getDynamicChunkData());
            }
        }

        struct SsboStruct : public IShaderStorageBufferStruct
        {
            static StaticType staticChunk;
            static std::vector<DynamicType> dynamicArray;

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
        } ssboStruct;
        u32 sizeInBytes = 0;
        i32 bufferBindingUnit = -1;
    };
}
