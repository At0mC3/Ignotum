//
// Created by Trinn on 2/13/2022.
//
#include <new>

#include <MappedMemory.hpp>

std::optional<MappedMemory> MappedMemory::Allocate(std::uintmax_t buffer_size)
{
    auto mem_ptr = new(std::nothrow) std::uint8_t[buffer_size]();
    if(!mem_ptr) {
        return {};
    }

    std::shared_ptr<std::uint8_t[]> buffer(mem_ptr);

    return MappedMemory(buffer, buffer_size);
}
