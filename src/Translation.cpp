#include <type_traits>

#include <Translation.hpp>

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
        case ZydisMnemonic::ZYDIS_MNEMONIC_ADD:
            AddInstLogic(operands, mapped_memory);
            break;
        case ZydisMnemonic::ZYDIS_MNEMONIC_MOV:
            MovInstLogic(operands, mapped_memory);
            break;
        default: // Instruction was not found
            break;
    }
    
    return Ok(true);
}

HOT_PATH Result<MappedMemory, int> Translation::TranslateInstructionBlock(const MappedMemory& instruction_block)
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
            virtual_memory
        );

        if(translation_result.isErr())
        {
            spdlog::warn("An error occured and the instruction couldn't be virtualized");
            spdlog::warn("Do you wish to continue? This could make the program unstable [y/n]");

dialog:
            std::string answer;
            std::cin >> answer;

            if(answer == "n" || answer == "N") {
                spdlog::info("Aborting virtualization");
                return Err(-1);
            }
            else if (answer == "y" || answer == "Y") {
                spdlog::info("Resuming virtualization");
                offset += instruction.length;
                continue;
            }
            else {
                goto dialog;
            }
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