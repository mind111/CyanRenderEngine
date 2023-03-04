#pragma once

#include <glew/glew.h>

#include "Common.h"
#include "CyanCore.h"

namespace Cyan
{
#if 0
    struct IShaderStorageBufferData
    {
        virtual void reserveDynamicData(u32 numElements) { }
        virtual bool hasDynamicData() { return false; }
        virtual void* getStaticData() { return nullptr; }
        virtual void* getDynamicData() { return nullptr; }
        virtual u32 getStaticDataSizeInBytes() { return 0; }
        virtual u32 getDynamicDataSizeInBytes() { return 0; };
        virtual u32 getSizeInBytes() { return getStaticDataSizeInBytes() + getDynamicDataSizeInBytes(); }
    };

    template <typename StaticData>
    struct StaticSsboData : public IShaderStorageBufferData
    {
        virtual void* getStaticData() override
        {
            return reinterpret_cast<void*>(&constants);
        }

        virtual u32 getStaticDataSizeInBytes() override
        {
            return sizeof(StaticData);
        }

        StaticSsboData(u32 numElements)
            : constants() {

        }

        i32 getNumElements() { return 0; }

        StaticData constants;
    };

    template <typename DynamicData>
    struct DynamicSsboData : public IShaderStorageBufferData
    {
        virtual bool hasDynamicData() override
        {
            return true;
        }

        virtual void* getDynamicData() override
        {
            return array.data();
        }

        virtual u32 getDynamicDataSizeInBytes() override
        {
            return sizeOfVector(array);
        }

        DynamicSsboData(u32 numElements) 
            : array(numElements) {

        }

        i32 getNumElements() { return array.size(); }

        void addElement(const DynamicData& element)
        {
            array.push_back(element);
        }

        DynamicData& operator[](u32 index)
        {
            return array[index];
        }

        std::vector<DynamicData> array;
    };

    template <typename StaticData, typename DynamicData>
    struct CompoundSsboData : public StaticSsboData<StaticData>, public DynamicSsboData<DynamicData>
    {
        virtual bool hasDynamicData() override
        {
            return DynamicSsboData<DynamicData>::hasDynamicData();
        }

        virtual void* getStaticData() override
        {
            return StaticSsboData<StaticData>::getStaticData();
        }

        virtual void* getDynamicData() override
        {
            return DynamicSsboData<DynamicData>::getDynamicData();
        }

        virtual u32 getStaticDataSizeInBytes() override
        {
            return StaticSsboData<StaticData>::getStaticDataSizeInBytes();
        }

        virtual u32 getDynamicDataSizeInBytes() override
        {
            return DynamicSsboData<DynamicData>::getDynamicDataSizeInBytes();
        }

        virtual u32 getSizeInBytes() override
        {
            return getStaticDataSizeInBytes() + getDynamicDataSizeInBytes();
        }

        CompoundSsboData(u32 numElements = 0) 
            : StaticSsboData<StaticData>(numElements), DynamicSsboData<DynamicData>(numElements) 
        {

        }
    };

    template <typename SsboData>
    struct ShaderStorageBuffer : public GpuResource {
        ShaderStorageBuffer(const char* bufferBlockName, u32 numElements = 0)
            : name(bufferBlockName), data(numElements) 
        {
            sizeInBytes = data.getSizeInBytes();

            glCreateBuffers(1, &glObject);
            glNamedBufferData(glObject, sizeInBytes, nullptr, GL_DYNAMIC_DRAW);
        }

        ~ShaderStorageBuffer() 
        {
            glDeleteBuffers(1, &glObject);
        }

        void bind(u32 inBufferBindingUnit) 
        {
            bufferBindingUnit = (i32)inBufferBindingUnit;
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (u32)bufferBindingUnit, glObject);
        }

        void unbind() 
        {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bufferBindingUnit, 0);
            bufferBindingUnit = -1;
        }

        void upload() 
        {
            // if the dynamic array out grow the allocated buffer, need to release old resources and create new ones
            if (data.getSizeInBytes() > sizeInBytes)
            {
                sizeInBytes = data.getSizeInBytes();
                // remove old gpu resources
                glDeleteBuffers(1, &glObject);

                // create new gpu resources
                glCreateBuffers(1, &glObject);
                glNamedBufferData(glObject, sizeInBytes, nullptr, GL_DYNAMIC_DRAW);
            }

            if (data.getStaticDataSizeInBytes() > 0)
            {
                // upload static members
                glNamedBufferSubData(getGpuResource(), 0, data.getStaticDataSizeInBytes(), data.getStaticData());
            }
            if (data.getDynamicDataSizeInBytes() > 0) 
            {
                // upload dynamic members
                glNamedBufferSubData(getGpuResource(), data.getStaticDataSizeInBytes(), data.getDynamicDataSizeInBytes(), data.getDynamicData());
            }
        }

        i32 getNumElements() { return data.getNumElements(); }

        template <typename T>
        void addElement(const T& element)
        {
            return data.addElement(element);
        }

        auto& operator[](u32 index) 
        {
            return data[index];
        }

        /**
        * return a deep copy `this` which possesses same buffer content as `this` but uses
        */
        ShaderStorageBuffer<SsboData>* clone() 
        {
            auto cloned = new ShaderStorageBuffer<SsboData>(this->name.c_str(), data.getNumElements());
            if (void* staticData = data.getStaticData()) {
                memcpy(cloned->data.getStaticData(), staticData, data.getStaticDataSizeInBytes());
            }
            if (void* dynamicData = data.getDynamicData()) {
                memcpy(cloned->data.getDynamicData(), dynamicData, data.getDynamicDataSizeInBytes());
            }
            cloned->upload();
            return cloned;
        }

        std::string name = std::string("Invalid");
        SsboData data;
        u32 sizeInBytes = 0;
        i32 bufferBindingUnit = -1;
    };
#else
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

        ~ShaderStorageBuffer() 
        { 
            GLuint buffers[] = { getGpuResource() };
            glDeleteBuffers(1, buffers);
        }

        const char* getBlockName() { return blockName.c_str(); }

        void bind(u32 inBinding) 
        { 
            binding = inBinding;
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, getGpuResource());
        }

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
#endif
}

