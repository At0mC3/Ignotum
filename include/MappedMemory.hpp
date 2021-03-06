//
// Created by Trinn on 2/13/2022.
//

#ifndef IGNOTUM_MAPPEDMEMORY_HPP
#define IGNOTUM_MAPPEDMEMORY_HPP

#include <memory>
#include <cstdint>
#include <cstring>
#include <utility>
#include <optional>

#include <result.h>

class MappedMemory
{
private:
    std::shared_ptr<std::uint8_t[]> m_buffer;
    std::uintmax_t m_size; // The size of the buffer
    std::uintmax_t m_cursor_i{ 0 }; // This holds a cursor index for the buffer
public:
    MappedMemory(std::shared_ptr<std::uint8_t[]>& buffer, std::uintmax_t size) : m_buffer(std::move(buffer)), m_size(size) {}
    static std::optional<MappedMemory> Allocate(std::uintmax_t buffer_size);
public:
    // Gives access to the internal buffer
    [[nodiscard]] std::shared_ptr<std::uint8_t[]> InnerPtr() const { return m_buffer; }
    [[nodiscard]] std::uint8_t* InnerPtrRaw() const { return m_buffer.get(); }
    [[nodiscard]] std::uintmax_t Size() const { return m_size; }
    [[nodiscard]] std::uintmax_t CursorPos() const { return m_cursor_i; }
public:
    template<class T>
    [[nodiscard]] bool Write(T value) 
    {
        if(m_size - m_cursor_i < sizeof(value))
            return false;

        // Get a pointer to the buffer and cast it to the size of the given value
        auto* ptr = m_buffer.get() + m_cursor_i;
        *std::bit_cast<T*>(ptr) = value;

        // Increment the cursor
        m_cursor_i += sizeof(value);
        return true;
    }

    [[nodiscard]] bool Write(std::uint8_t* source, std::size_t size) 
    {
        if(m_size - m_cursor_i < size)
            return false;

        // Get a pointer to the buffer and cast it to the size of the given value
        auto* ptr = m_buffer.get() + m_cursor_i;
        std::memcpy(ptr, source, size);

        // Increment the cursor
        m_cursor_i += size;
        return true;
    }
};

#endif //IGNOTUM_MAPPEDMEMORY_HPP
