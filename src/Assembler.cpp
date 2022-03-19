#include <Assembler.hpp>

bool X64::Generator::CallNear(MappedMemory& out_memory, std::int32_t relative_offset)
{
    const auto address_size = 0x05;
    relative_offset -= address_size;

    out_memory.Write<std::uint8_t>(0xE8);
    return out_memory.Write(relative_offset);
}

bool X64::Generator::PushX32(MappedMemory& out_memory, const std::uint32_t value)
{
    out_memory.Write<std::uint8_t>(0x68);
    return out_memory.Write<std::uint32_t>(value);
}