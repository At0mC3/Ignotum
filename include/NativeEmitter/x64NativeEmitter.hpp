#ifndef __X64NATIVEEMITTER_H__
#define __X64NATIVEEMITTER_H__

#include <NativeEmitter/NativeEmitter.hpp>

class x64NativeEmitter : public NativeEmitter
{
public:
    bool EmitPush32Bit(std::uint32_t value, MappedMemory &mapped_memory)
    {
        if(!mapped_memory.Write<std::uint8_t>(0x68))
            return false;

        return mapped_memory.Write<std::uint32_t>(value);
    }

    bool EmitPush64Bit(std::uint64_t value, MappedMemory &mapped_memory)
    {
        if(!mapped_memory.Write<std::uint8_t>(0x68))
            return false;

        return mapped_memory.Write<std::uint64_t>(value);
    }

    bool EmitNearCall(std::int32_t offset, MappedMemory &mapped_memory)
    {
        const auto address_size = 0x05;
        offset -= address_size;

        if(!mapped_memory.Write<std::uint8_t>(0xE8))
            return false;

        return mapped_memory.Write(offset);
    }

    bool EmitNearJmp(std::int32_t offset, MappedMemory &mapped_memory)
    {
        const auto address_size = 0x05;
        offset -= address_size;

        if(!mapped_memory.Write<std::uint8_t>(0xE9))
            return false;

        return mapped_memory.Write(offset);
    }
};

#endif // __X64NATIVEEMITTER_H__