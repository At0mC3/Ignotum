#include <type_traits>

#include <Translation.hpp>

HOT_PATH FORCE_INLINE Translation::RetResult Translation::TranslateInstruction(
    const ZydisDecodedInstruction &instruction,
    const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
    MappedMemory& mapped_memory,
    bool isProbing
)
{
    switch(instruction.mnemonic)
    {
        case ZydisMnemonic::ZYDIS_MNEMONIC_SUB:
            if(isProbing)
                return RetResult::OK;
            SubInstLogic(operands, mapped_memory);
            break;
        case ZydisMnemonic::ZYDIS_MNEMONIC_ADD:
            if(isProbing)
                return RetResult::OK;
            AddInstLogic(operands, mapped_memory);
            break;
        case ZydisMnemonic::ZYDIS_MNEMONIC_MOV:
            if(isProbing)
                return RetResult::OK;
            MovInstLogic(operands, mapped_memory);
            break;
        default: // Instruction was not found
            return RetResult::INSTRUCTION_NOT_SUPPORTED;
            break;
    }
    
    return RetResult::OK;
}

HOT_PATH Result<MappedMemory, int> 
Translation::TranslateInstructionBlock(
    const MappedMemory& instruction_block, 
    NativeEmitter* native_emitter
)
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

    const auto virtual_memory_result = MappedMemory::Allocate(inner_buffer_size * 12);
    if(virtual_memory_result.isErr())
        return Err(-1);

    auto virtual_memory = virtual_memory_result.unwrap();

    // Indicates to the TranslationInstruction function to simply return if the instruction is supported
    // without emitting any code
    bool isProbing{false};

    while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, buffer + offset, inner_buffer_size - offset,
        &instruction, operands, ZYDIS_MAX_OPERAND_COUNT_VISIBLE, 
        ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY)))
    {
        // Format & print the binary instruction structure to human-readable format
        char text_buffer[256] = { 0 };
        ZydisFormatterFormatInstruction(&formatter, &instruction, operands,
            instruction.operand_count_visible, text_buffer, sizeof(text_buffer), 0);

        spdlog::info("---------------");
        spdlog::info("{}", text_buffer);

        const auto translation_result = Translation::TranslateInstruction(
            instruction,
            operands,
            virtual_memory,
            isProbing
        );

        // If the return result is INSTRUCTION_NOT_SUPPORTED
        // We need to make a patch of native instructions and signal the virtual machine to switch back
        // Lets just store them in a vector until we hit a support instruction
        if(translation_result == RetResult::INSTRUCTION_NOT_SUPPORTED)
        {
            // If it's true, we already generate the switch
            if(isProbing == true) 
            {
                // Generate the switch instruction to move into native mode
                spdlog::info("Emitting -> kVmSwitch");
                const auto kswitch_inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVmSwitch);
                virtual_memory.Write<Virtual::InstructionLength>(kswitch_inst.AssembleInstruction());
            } else {
                isProbing = true; 
            }

            // Write the native instruction
            virtual_memory.Write(std::bit_cast<std::uint8_t*>(buffer + offset), instruction.length);
        }

        // Probing is done, we found a supported instruction.
        // We now need to finish the block and emitting code for this instruction
        // No code was emitted for this one previously because of the probing flag
        if(isProbing && translation_result == RetResult::OK)
        {
            isProbing = false;

            // Emit native code which will bring the execution back to the machine
        }

        offset += instruction.length;
        spdlog::info("---------------");
    }

    // Generate instruction to notify the virtual machine that the execution is over.
    // The machine should restore everything and return to the caller
    const auto exit_inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVmExit);
    virtual_memory.Write<Virtual::InstructionLength>(exit_inst.AssembleInstruction());

    return Ok(virtual_memory);
}