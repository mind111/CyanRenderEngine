#include "VertexBuffer.h"

u32 VertexAttrib::getSize()
{
    switch (m_type)
    {
        case VertexAttrib::DataType::Float:
            return 4;
        case VertexAttrib::DataType::Int:
            return 4;
        default:
            return 0;
            break;
    }
}