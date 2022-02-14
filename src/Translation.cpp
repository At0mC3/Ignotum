#include <Translation.hpp>

HOT_PATH FORCE_INLINE void SubInstLogic(
        const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
        MappedMemory& mapped_memory
        )
{
    switch(operands[0].type)
    {
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_REGISTER:
            break;
        default:
            break;
    }
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