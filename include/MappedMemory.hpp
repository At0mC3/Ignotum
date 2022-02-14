//
// Created by Trinn on 2/13/2022.
//

#ifndef IGNOTUM_MAPPEDMEMORY_HPP
#define IGNOTUM_MAPPEDMEMORY_HPP

#include <memory>
#include <cstdint>

#include <result.h>

class MappedMemory
{
private:
    std::shared_ptr<std::byte[]> m_buffer;
    std::uintmax_t m_size; // The size of the buffer
    std::uintmax_t m_cursor_i{ 0 }; // This holds a cursor index for the buffer
public:
    MappedMemory(std::shared_ptr<std::byte[]> buffer, std::uintmax_t size) : m_buffer(buffer), m_size(size) {}
    static Result<MappedMemory, const char*> Allocate(std::uintmax_t buffer_size);
public:
    // Gives access to the internal buffer
    [[nodiscard]] std::shared_ptr<std::byte[]> Get() const { return m_buffer; }
    [[nodiscard]] std::uintmax_t Size() const { return m_size; }
    [[nodiscard]] std::uintmax_t CursorPos() const { return m_cursor_i; }
};

#endif //IGNOTUM_MAPPEDMEMORY_HPP
