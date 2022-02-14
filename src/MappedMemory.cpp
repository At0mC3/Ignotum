//
// Created by Trinn on 2/13/2022.
//
#include <new>

#include <MappedMemory.hpp>

Result<MappedMemory, const char*> MappedMemory::Allocate(std::uintmax_t buffer_size)
{
    std::shared_ptr<std::byte[]> buffer(new(std::nothrow) std::byte[buffer_size]());
    if(!buffer)
        return Err("Failed to initialize the buffer");

    return Ok(MappedMemory(buffer, buffer_size));
}
