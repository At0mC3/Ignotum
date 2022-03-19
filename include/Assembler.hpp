#ifndef __ASSEMBLER_H__
#define __ASSEMBLER_H__
#include <cstdint>

#include <MappedMemory.hpp>

namespace X64
{
    namespace Generator
    {
        bool CallNear(MappedMemory& out_memory, const std::int32_t relative_address);
        bool PushX32(MappedMemory& out_memory, const std::uint32_t value);
    }
}

#endif // __ASSEMBLER_H__