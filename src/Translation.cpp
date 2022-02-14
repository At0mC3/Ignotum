#include <type_traits>

#include <Translation.hpp>

using Virtual::Parameter;

HOT_PATH FORCE_INLINE void Ldr(const ZydisRegister& reg, MappedMemory& mapped_memory)
{
    const auto inst = Virtual::Instruction(Virtual::Parameter(static_cast<std::uint16_t>(reg)), Virtual::Command::kLdr);
    mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
}

HOT_PATH FORCE_INLINE void Ldm(const ZydisDecodedOperand::ZydisDecodedOperandImm_& imm, MappedMemory& mapped_memory)
{
    // Construct the instruction and write it to the memory
    const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kLdImm);
    mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());

    assert(!imm.is_signed && "Signed value not supported in Ldm");

    auto unsigned_imm = imm.value.u;
    // Write the immediate in the mapped memory
    mapped_memory.Write<decltype(unsigned_imm)>(unsigned_imm);
}

HOT_PATH FORCE_INLINE void SubInstLogic(
        const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
        MappedMemory& mapped_memory
)
{
    const auto first_operand = operands[0];
    switch(first_operand.type)
    {
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_REGISTER:
            Ldr(first_operand.reg.value, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_MEMORY:
            Ldm(first_operand.imm, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_POINTER:
            // LdPtr(operands[0].ptr.segment, mapped_memory)
            break;
        default:
            break;
    }

    const auto second_operand = operands[1];
    switch(second_operand.type)
    {
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_REGISTER:
            Ldr(first_operand.reg.value, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_MEMORY:
            Ldm(first_operand.imm, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_POINTER:
            // LdPtr(operands[0].ptr.segment, mapped_memory)
            break;
        default:
            break;
    }

    const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVAdd);
    mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
}

HOT_PATH FORCE_INLINE Result<bool, Translation::TranslationError> Translation::TranslateInstruction(
    const ZydisDecodedInstruction &instruction,
    const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
    MappedMemory& mapped_memory
)
{
    switch(instruction.mnemonic)
    {
        case ZydisMnemonic::ZYDIS_MNEMONIC_SUB:
            SubInstLogic(operands, mapped_memory);
            break;
        default:
            return Err(TranslationError::INSTRUCTION_NOT_FOUND);
    }
    
    return Ok(true);
}

HOT_PATH MappedMemory Translation::TranslateInstructionBlock(const MappedMemory& instruction_block)
{
    const auto inner_buffer_size = instruction_block.Size();
    const auto buffer = instruction_block.InnerPtr().get();
    // Initialize formatter. Only required when you actually plan to do instruction
    // formatting ("disassembling"), like we do here
    ZydisFormatter formatter;
    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

    // Initialize decoder context
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

    std::size_t offset = 0;
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE];

    auto virtual_inst_memory = MappedMemory::Allocate(inner_buffer_size * 5)
            .expect("Failed to allocate a buffer for the virtual instructions");

    while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, buffer + offset, inner_buffer_size - offset,
        &instruction, operands, ZYDIS_MAX_OPERAND_COUNT_VISIBLE, 
        ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY)))
    {
        // Format & print the binary instruction structure to human-readable format
        char text_buffer[256] = { 0 };
        ZydisFormatterFormatInstruction(&formatter, &instruction, operands,
            instruction.operand_count_visible, text_buffer, sizeof(text_buffer), 0);
        // std::puts(text_buffer);

        const auto translation_result = Translation::TranslateInstruction(
            instruction,
            operands,
            virtual_inst_memory
        );

        if(translation_result.isErr())
        {
            std::cout << "[UNSUPPORTED]: " << text_buffer << "\n";
        }
        else
        {
            std::cout << "[SUPPORTED]: " << text_buffer << "\n";
        }


        offset += instruction.length;
    }

    return virtual_inst_memory;
}