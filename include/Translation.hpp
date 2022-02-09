#ifndef INCLUDE_TRANSLATION_
#define INCLUDE_TRANSLATION_

#include <iostream>
#include <functional>
#include <bit>
#include <variant>
#include <cstdint>

#include <Virtual.hpp>
#include <Parameter.hpp>

// 3rd party Library
#include <Zydis/Zydis.h>

namespace Translation
{
    namespace
    {
        typedef std::function<std::size_t(
            const ZydisDecodedInstruction& instruction, 
            const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE], 
            std::byte* out_buffer, 
            const std::size_t& out_buffer_size)> TranslationFn;

        inline Virtual::InstructionLength WriteInstruction(const Virtual::Instruction& inst, auto* out_buffer, const auto& out_buffer_size)
        {
            // If the remaining space in the buffer is smaller than the size of a instruction, ret 0
            if(out_buffer_size < sizeof(Virtual::InstructionLength))
                return 0;

            *std::bit_cast<std::uint32_t*>(out_buffer) = inst.AssembleInstruction();
            return sizeof(Virtual::InstructionLength);
        }

        static const std::unordered_map<ZydisMnemonic, TranslationFn> vm_translation_map = {
            {ZydisMnemonic::ZYDIS_MNEMONIC_ADD, [](const ZydisDecodedInstruction& instruction, const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE], auto* out_buffer, const auto out_buffer_size) {
                const auto operands_count = instruction.operand_count_visible;

                // The buffer might have the size of 3 bytes, and it won't be enough for the size of the instructions
                if(out_buffer_size < sizeof(std::uint32_t))
                    return 0;

                auto buffer_size = out_buffer_size / sizeof(std::uint32_t);
                auto buffer_offset = 0;

                if(operands[0].type == ZydisOperandType::ZYDIS_OPERAND_TYPE_REGISTER) {
                    // If the first argument of the add instruction is a register, run the ldr instruction
                    // Which will load the content of the register in the virtual stack
                    const auto reg = operands[0].reg.value;
                    const auto virtual_instruction = Virtual::Instruction(
                            Virtual::Parameter(static_cast<std::uint16_t>(reg)),
                            Virtual::Command::kLdr
                            );

                    buffer_offset += WriteInstruction(virtual_instruction, out_buffer + buffer_offset, out_buffer_size - buffer_offset);
                    std::cout << virtual_instruction.AssembleInstruction() << '\n';
                }
                else if(operands[0].type == ZydisOperandType::ZYDIS_OPERAND_TYPE_MEMORY) {
                    const auto index = operands[0].mem.index;
                    const auto reg = operands[0].mem.base;
                    const auto scale = operands[0].mem.scale;
                    const auto disp = operands[0].mem.disp.value;
//                    std::cout << static_cast<std::uint32_t>(reg) << "\n";
//                    std::cout << static_cast<std::uint32_t>(index) << "\n";
//                    std::cout << std::hex << (unsigned int)scale << "\n";
//                    std::cout << std::hex << disp << "\n";

                    const auto register_index = static_cast<std::uint16_t>(reg);
                    const auto virtual_instruction = Virtual::Instruction(Virtual::Parameter(register_index), Virtual::Command::kLdImm);

                    // buffer_offset += WriteInstruction(virtual_instruction, out_buffer + buffer_offset, out_buffer_size - buffer_offset);
                    // std::cout << virtual_instruction.AssembleInstruction() << '\n';
                }


                return buffer_offset;
            }}
        };
    }

    enum TranslationError
    {
        INSTRUCTION_NOT_FOUND
    };
    std::variant<std::size_t, TranslationError> TranslateInstruction(
        const ZydisDecodedInstruction &instruction,
        const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
        std::byte *out_buffer, 
        const std::size_t &out_buffer_size
    );
}

#endif