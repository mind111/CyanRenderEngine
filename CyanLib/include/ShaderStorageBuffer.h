#pragma once

#include <cassert>
#include <glew/glew.h>

#include "Common.h"
#include "CyanCore.h"

namespace Cyan
{
    class GfxContext;

    class ShaderStorageBuffer : public GpuResource     
    {
    public:
        ShaderStorageBuffer(const char* bufferBlockName, u32 inSizeInBytes)
            : blockName(bufferBlockName), sizeInBytes(inSizeInBytes)
        {
            glCreateBuffers(1, &glObject);
            // todo: the usage hint should be configurable
            glNamedBufferData(getGpuResource(), sizeInBytes, nullptr, GL_DYNAMIC_COPY);
        }

        // deep copy of buffer data
        ShaderStorageBuffer(const ShaderStorageBuffer& src)
        {
            glCreateBuffers(1, &glObject);
            glNamedBufferData(getGpuResource(), src.sizeInBytes, nullptr, GL_DYNAMIC_COPY);
            glCopyNamedBufferSubData(src.getGpuResource(), getGpuResource(), 0, 0, src.sizeInBytes);
        }

        ~ShaderStorageBuffer();

        const char* getBlockName() { return blockName.c_str(); }

        void bind(GfxContext* ctx, u32 inBinding);
        void unbind(GfxContext* ctx);
        bool isBound();

        template <typename T>
        void read(T& data)
        {

        }

        template <typename T>
        void read(std::vector<T>& data, u32 offset, u32 bytesToRead)
        {
            assert(bytesToRead < sizeInBytes);
            assert(bytesToRead % sizeof(T) == 0);
            u32 numElements = bytesToRead / sizeof(T);
            if (data.size() < numElements)
            {
                data.resize(numElements);
            }
            glGetNamedBufferSubData(getGpuResource(), offset, bytesToRead, data.data());
        }

        template <typename T>
        void write(const std::vector<T>& data, u32 offset, u32 bytesToWrite)
        {
            if (bytesToWrite > sizeInBytes)
            {
                // need to resize
                sizeInBytes = bytesToWrite;
                GLuint buffers[] = { getGpuResource() };
                glDeleteBuffers(1, buffers);
                glCreateBuffers(1, &glObject);
                glNamedBufferData(getGpuResource(), sizeInBytes, data.data(), GL_DYNAMIC_COPY);
            }
            else
            {
                glNamedBufferSubData(getGpuResource(), offset, sizeInBytes, data.data());
            }
        }

        template <typename T>
        void write(const std::vector<T>& data, u32 offset)
        {
            write(data, offset, sizeOfVector(data));
        }

        template <typename T>
        void write(const T& data, u32 offset)
        {
            glNamedBufferSubData(getGpuResource(), offset, sizeof(T), &data);
        }
    private:
        std::string blockName;
        i32 binding = -1;
        i32 sizeInBytes = -1;
    };
}

