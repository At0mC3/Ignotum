#ifndef INCLUDE_TRANSLATION_
#define INCLUDE_TRANSLATION_

// Std libraries
#include <iostream>
#include <functional>
#include <bit>
#include <variant>
#include <cstdint>

// Project libraries
#include <Virtual.hpp>
#include <Parameter.hpp>
#include <MappedMemory.hpp>
#include <utl/Utl.hpp>

// 3rd party Library
#include <Zydis/Zydis.h>
#include <result.h>

namespace Translation
{
    enum TranslationError
    {
        INSTRUCTION_NOT_FOUND, // The equivalent virtual instruction doesn't exist. A switch is needed
        OUT_OF_MEMORY // The mapped memory object ran out of space with it's internal buffer
    };

    /**
     * @brief 
     * Given a x86_64 instruction, it will translate it to the proper virtal instruction.
     * 
     * @param instruction The native instruction to be translated 
     * @param operands The operands that are within the given instruction
     * @param mapped_memory 
     * Mapped memory which will be used to write the virtual instruction to.
     * @return Result<bool, TranslationError>
     * The Ok value can be ignored. For the error, see above for the enum definition. 
     */
    HOT_PATH FORCE_INLINE Result<bool, TranslationError> TranslateInstruction(
        const ZydisDecodedInstruction &instruction,
        const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
        MappedMemory& mapped_memory
    );

    /**
     * @brief 
     * Given a buffer containing x86_64 instructions, it will go over the buffer and disassemble the instructions.
     * It will then call routines to translate these instructions to the virtual architecture.
     * 
     * @param instruction_block 
     * The mapped memory block which contains x86_64 instructions
     * @return MappedMemory
     * A mapped memory object containing all of the translated instructions
     */
    HOT_PATH Result<MappedMemory, int> TranslateInstructionBlock(const MappedMemory& instruction_block);
}

#endif