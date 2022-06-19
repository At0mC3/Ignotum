#ifndef __NATIVEEMITTER_H__
#define __NATIVEEMITTER_H__

#include <MappedMemory.hpp>

class NativeEmitter
{
public:
    virtual bool EmitPush32Bit(std::uint32_t value, MappedMemory& mapped_memory) = 0;
    virtual bool EmitPush64Bit(std::uint64_t value, MappedMemory& mapped_memory) = 0;
    virtual bool EmitNearCall(std::int32_t offset, MappedMemory& mapped_memory) = 0;
    virtual bool EmitNearJmp(std::int32_t offset, MappedMemory& mapped_memory) = 0;
};

#endif // __NATIVEEMITTER_H__