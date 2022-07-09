#ifndef INCLUDE_CRYPTOGRAPHY_HPP_
#define INCLUDE_CRYPTOGRAPHY_HPP_

#include <cstdint>
#include <random>
#include <bit>

namespace cryptography
{
    /**
     * @brief
     * Like it's name indicates, the function generates a pseudo random 16 bit number
     * using the std engine.
     *
     * @return std::uint16_t The value generated.
     */
    static inline std::uint16_t Generate16BitKey()
    {
        static std::random_device rd;
        static std::independent_bits_engine<std::mt19937, 16, std::uint16_t> key_engine(rd());

        return key_engine();
    }

    /**
     * @brief
     * This function is used as a really basic way of obfuscating the entry point
     * for the virtual instructions.
     *
     * It simply does a xor with the key and the original virtual instruction pointer.
     *
     * @param original_vip The original value which is mean't to be used by the vm to find the instructions.
     * @param key_value A key that was possibly randomly generated to xor the original vip.
     * @return std::uint32_t The virtual instruction pointer that was obfuscated by the provided key.
     */
    static inline std::uint32_t EncodeVIPEntry(std::uint32_t original_vip, std::uint16_t key_value)
    {
        const auto key_part1 = static_cast<std::uint8_t>(key_value);
        const auto key_part2 = static_cast<std::uint8_t>(key_value >> 8);

        auto enc_vip = original_vip ^ static_cast<std::uint32_t>(key_part1) << 8;
        enc_vip = enc_vip ^ static_cast<std::uint32_t>(key_part2);

        return (enc_vip << 16) | key_value;
    }
}

#endif