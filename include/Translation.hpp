#ifndef INCLUDE_TRANSLATION_
#define INCLUDE_TRANSLATION_

#include <iostream>
#include <functional>
#include <cstdint>
#include <variant>

// 3rd party Library
#include <Zydis/Zydis.h>

namespace Translation
{
    namespace
    {
        typedef std::function<std::size_t(const ZydisDecodedInstruction &instruction, std::byte *out_buffer, const std::size_t &out_buffer_size)> TranslationFn;

        static const std::unordered_map<ZydisMnemonic, TranslationFn> vm_translation_map;
    }
    
    enum TranslationError { INSTRUCTION_NOT_FOUND };
    std::variant<std::size_t, TranslationError> TranslateInstruction(const ZydisDecodedInstruction &instruction, std::byte *out_buffer, const std::size_t &out_buffer_size);
}

#endif