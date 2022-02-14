#ifndef INCLUDE_TRANSLATION_
#define INCLUDE_TRANSLATION_

#include <iostream>
#include <functional>
#include <bit>
#include <variant>
#include <cstdint>

#include <Virtual.hpp>
#include <Parameter.hpp>
#include <MappedMemory.hpp>
#include <utl/Utl.hpp>

// 3rd party Library
#include <Zydis/Zydis.h>
#include <result.h>

namespace Translation
{
    namespace
    {

    }

    enum TranslationError
    {
        INSTRUCTION_NOT_FOUND,
        OUT_OF_MEMORY
    };

    HOT_PATH FORCE_INLINE Result<bool, TranslationError> TranslateInstruction(
        const ZydisDecodedInstruction &instruction,
        const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
        MappedMemory& mapped_memory
    );

    HOT_PATH MappedMemory TranslateInstructionBlock(const MappedMemory& instruction_block);
}

#endif