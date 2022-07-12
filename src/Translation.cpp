#include <type_traits>
#include <random>

#include <Translation.hpp>
#include <Cryptography.hpp>

HOT_PATH FORCE_INLINE Translation::RetResult Translation::TranslateInstruction(
    const ZydisDecodedInstruction &instruction,
    const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
    MappedMemory& mapped_memory,
    const Translation::Context& context,
    bool is_probing
)
{
    bool success = true;

    switch(instruction.mnemonic)
    {
        case ZydisMnemonic::ZYDIS_MNEMONIC_SUB:
            if(is_probing)
                return RetResult::OK;
            success = SubInstLogic(operands, mapped_memory);
            break;
        case ZydisMnemonic::ZYDIS_MNEMONIC_ADD:
            if(is_probing)
                return RetResult::OK;
            success = AddInstLogic(operands, mapped_memory);
            break;
        case ZydisMnemonic::ZYDIS_MNEMONIC_MOV:
            if(is_probing)
                return RetResult::OK;
            success = MovInstLogic(operands, mapped_memory);
            break;
        case ZydisMnemonic::ZYDIS_MNEMONIC_CALL:
            if(is_probing)
                return RetResult::OK;
            success = CallInstLogic(operands, mapped_memory, context);
            break;
        default: // Instruction was not found
            return RetResult::INSTRUCTION_NOT_SUPPORTED;
            break;
    }
    
    if(!success) {
        return RetResult::OUT_OF_MEMORY;
    }

    return RetResult::OK;
}

HOT_PATH std::optional<MappedMemory> 
Translation::TranslateInstructionBlock(
    const MappedMemory& instruction_block,
    const std::shared_ptr<NativeEmitter> native_emitter,
    const Translation::Context& context
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

    const auto virtual_memory_result = MappedMemory::Allocate(inner_buffer_size * 334);
    if(!virtual_memory_result) {
        return {};
    }

    auto virtual_memory = virtual_memory_result.value();

    // Indicates to the TranslationInstruction function to simply return if the instruction is supported
    // without emitting any code
    bool is_probing{false};

    std::vector<ZydisDecodedInstruction> unsupported_instructions;
    unsupported_instructions.reserve(10);

    bool vm_switched{false};

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
            context,
            is_probing
        );

        // If the return result is INSTRUCTION_NOT_SUPPORTED
        // We need to make a patch of native instructions and signal the virtual machine to switch back
        // Lets just store them in a vector until we hit a support instruction
        if(translation_result == RetResult::INSTRUCTION_NOT_SUPPORTED)
        {
            // If it's true, we already generated the switch
            if(!is_probing) 
            {
                // Generate the switch instruction to move into native mode
                spdlog::info("Emitting -> kVmSwitch");
                const auto kswitch_inst = Virtual::Instruction(
                    Parameter(Parameter::kNone),
                    Virtual::Command::kVmSwitch
                );

                if(!virtual_memory.Write<Virtual::InstructionLength>(kswitch_inst.AssembleInstruction())) {
                    std::puts("Out of memory");
                    return {};
                }
                
                vm_switched = true;
                is_probing = true;
            }

            // Write the native instruction
            spdlog::info("Emitting native instruction");
            if(!virtual_memory.Write(std::bit_cast<std::uint8_t*>(buffer + offset), instruction.length)) {
                return {};
            }
        }

        if(translation_result == RetResult::OUT_OF_MEMORY) {
            return {};
        }

        // Probing is done, we found a supported instruction.
        // We now need to finish the block and emitting code for this instruction
        // No code was emitted for this one previously because of the probing flag
        if(is_probing && translation_result == RetResult::OK)
        {
            is_probing = false;

            // Emit native code which will bring the execution back to the machine
            const auto relative_offset = context.vcode_block_rva - context.vm_block_rva;

            const std::uint32_t vip = relative_offset + virtual_memory.CursorPos() + 15;

            const auto vip_enc_key = cryptography::Generate16BitKey();
            const auto enc_vip = cryptography::EncodeVIPEntry(vip, vip_enc_key);
            native_emitter->EmitPush32Bit(enc_vip, virtual_memory); // Push where the VIP should be

            const auto ret_relative = context.vm_block_rva - (context.original_block_rva + 10);
            native_emitter->EmitPush32Bit(ret_relative, virtual_memory); // Push where it should return after kVmExit

            const std::int32_t jump_offset = context.vm_block_rva - (context.vcode_block_rva + virtual_memory.CursorPos());
            native_emitter->EmitNearJmp(jump_offset, virtual_memory); // Jump to entry of vm

            spdlog::info("Emitting native instruction to resume VM execution");

            // We need to translate the instruction
            const auto res = Translation::TranslateInstruction(
                instruction,
                operands,
                virtual_memory,
                context,
                is_probing
            );

            // The result should be ok here since the if statement was reached
            // We can safely assume an error occured, time to return
            if(res != RetResult::OK) {
                return {};
            }
        }

        offset += instruction.length;
        spdlog::info("---------------");
    }

    // Generate instruction to notify the virtual machine that the execution is over.
    // The machine should restore everything and return to the caller

    // We need to check if the virtual machine has previously switched.
    // If it did, we need to emit a different command to restore
    // the original state

    // Simple lambda to find the command
    // More exits will be added in the futre. Thus making the lambda reasonable
    const auto exit_command = [&](){
        if(vm_switched) {
            return Virtual::Command::kVmExit2;
        }

        return Virtual::Command::kVmExit;
    }();

    const auto exit_inst = Virtual::Instruction(
        Parameter(Parameter::kNone),
        exit_command
    );

    if(!virtual_memory.Write<Virtual::InstructionLength>(exit_inst.AssembleInstruction())) {
        return {};
    }

    return virtual_memory;
}