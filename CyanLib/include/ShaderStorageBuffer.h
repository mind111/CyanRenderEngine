#pragma once

#include "glew.h"

#include "Common.h"

namespace Cyan
{
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

        StaticSsboData()
            : constants() {

        }

        StaticSsboData(u32 numElements = 0) {

        }

        u32 getNumElements() { return 0; }

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

        u32 getNumElements() { return array.size(); }

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
            return StaticSsboData<StaticData>::getSizeInBytes();
        }

        virtual u32 getDynamicDataSizeInBytes() override
        {
            return DynamicSsboData<DynamicData>::getSizeInBytes();
        }

        virtual u32 getSizeInBytes() override
        {
            return getStaticDataSizeInBytes() + getDynamicDataSizeInBytes();
        }

        CompoundSsboData(u32 numElements = 0) 
            : StaticData(), DynamicData(numElements) {

        }
    };

    template <typename SsboData>
    struct ShaderStorageBuffer : public GpuResource
    {
        ShaderStorageBuffer(u32 numElements = 0)
            : data(numElements)
        {
            sizeInBytes = data.getSizeInBytes();

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

        void upload()
        {
            // if the dynamic array out grow the allocated buffer, need to release old resources and create new ones
            if (data.getSizeInBytes() > sizeInBytes)
            {
                sizeInBytes = data.getSizeInBytes();
                // remove old gpu resources
                glDeleteBuffers(1, &glResource);

                // create new gpu resources
                glCreateBuffers(1, &glResource);
                glNamedBufferData(glResource, sizeInBytes, nullptr, GL_DYNAMIC_DRAW);
            }

            if (data.getStaticDataSizeInBytes() > 0)
            {
                // upload static members
                glNamedBufferSubData(getGpuResource(), 0, data.getStaticDataSizeInBytes(), data.getStaticData());
            }
            if (data.getDynamicDataSizeInBytes() > 0) {
                // upload dynamic members
                glNamedBufferSubData(getGpuResource(), data.getStaticDataSizeInBytes(), data.getDynamicDataSizeInBytes(), data.getDynamicData());
            }
        }

        u32 getNumElements() { return data.getNumElements(); }

        template <typename T>
        void addElement(const T& element)
        {
            return data.addElement(element);
        }

        auto& operator[](u32 index) {
            return data[index];
        }

        /**
        * return a deep copy `this` which possesses same buffer content as `this` but uses
        */
        ShaderStorageBuffer<SsboData>* clone() {
            auto cloned = new ShaderStorageBuffer<SsboData>(data.getNumElements());
            if (void* staticData = data.getStaticData()) {
                memcpy(cloned->data.getStaticData(), staticData, data.getStaticDataSizeInBytes());
            }
            if (void* dynamicData = data.getDynamicData()) {
                memcpy(cloned->data.getDynamicData(), dynamicData, data.getDynamicDataSizeInBytes());
            }
            cloned->upload();
            return cloned;
        }

        SsboData data;
        u32 sizeInBytes = 0;
        i32 bufferBindingUnit = -1;
    };
}

