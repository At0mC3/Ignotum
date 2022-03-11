#include <type_traits>
#include <iostream>

#include <Translation.hpp>

using Virtual::Parameter;

std::string DebugPrintReg(const ZydisRegister& reg)
{
    switch(reg)
    {
        case ZydisRegister::ZYDIS_REGISTER_RAX:
            return "RAX";
        case ZydisRegister::ZYDIS_REGISTER_RBX:
            return "RBX";
        case ZydisRegister::ZYDIS_REGISTER_RCX:
            return "RCX";
        case ZydisRegister::ZYDIS_REGISTER_RDX:
            return "RDX";
        case ZydisRegister::ZYDIS_REGISTER_RDI:
            return "RDI";
        case ZydisRegister::ZYDIS_REGISTER_RSI:
            return "RSI";
        case ZydisRegister::ZYDIS_REGISTER_RSP:
            return "RSP";
        case ZydisRegister::ZYDIS_REGISTER_RBP:
            return "RBP";
        default:
            return "None";
    }
}

HOT_PATH FORCE_INLINE void Ldr(const ZydisRegister& reg, MappedMemory& mapped_memory)
{
    std::puts("[DEBUG] Emitting -> LDR");
    const auto inst = Virtual::Instruction(Virtual::Parameter(static_cast<std::uint16_t>(reg)), Virtual::Command::kLdr);
    mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
}

HOT_PATH FORCE_INLINE void Ldi(const ZydisDecodedOperand::ZydisDecodedOperandImm_& imm, MappedMemory& mapped_memory)
{
    std::puts("[DEBUG] Emitting -> LDI");
    // Construct the instruction and write it to the memory
    const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kLdImm);
    mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());

    assert(!imm.is_signed && "Signed value not supported in Ldm");

    auto unsigned_imm = imm.value.u;
    // Write the immediate in the mapped memory
    mapped_memory.Write<decltype(unsigned_imm)>(unsigned_imm);
}

HOT_PATH FORCE_INLINE void Ldi(const std::uint64_t& imm, MappedMemory& mapped_memory)
{
    // Construct the instruction and write it to the memory
    const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kLdImm);
    mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());

    auto unsigned_imm = imm;
    // Write the immediate in the mapped memory
    mapped_memory.Write<decltype(unsigned_imm)>(unsigned_imm);
}


HOT_PATH FORCE_INLINE void Svr(const ZydisRegister& reg, MappedMemory& mapped_memory)
{
    std::puts("[DEBUG] Emitting -> SVR");
    const auto inst = Virtual::Instruction(Virtual::Parameter(static_cast<std::uint16_t>(reg)), Virtual::Command::kVSvr);
    mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
}

/**
 * @brief 
 * 
 * This function is in charge of handling the complex memory addressing of x86
 * Example: push dword ptr[eax + ecx * 4 + 1000]
 * 
 * This function would push the value of eax and 1000 on the virtual stack and add them together.
 * It would then push ecx and 4 on the stack and multiply them together.
 * It would then add those two results together before executing the instruction that would access that memory region.
 * 
 * @param operand The operand to be handled by the function
 * @param mapped_memory 
 * Mapped memory which will receive the virtual instructions
 * @return HOT_PATH 
 */
HOT_PATH FORCE_INLINE void UnrollMemoryAddressing(const ZydisDecodedOperand::ZydisDecodedOperandMem_& mem, MappedMemory& mapped_memory)
{
    // Load the content of the base register on the stack
    // If the operation doesn't use a base, just load 0
    if(mem.base != ZYDIS_REGISTER_NONE)
        Ldr(mem.base, mapped_memory);
    else
        Ldi(0, mapped_memory);

    // Load the content of the base register on the stack
    // If the operation doesn't use a base, just load 0
    if(mem.disp.has_displacement)
        Ldi(mem.disp.value, mapped_memory);
    else
        Ldi(0, mapped_memory);

    // Generate the virtual instruction to add both values together
    const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVAdd);    
    mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());

    // Load the content of the index register on the stack
    // If the operation doesn't use a index, just load 0
    if(mem.index != ZYDIS_REGISTER_NONE)
        Ldr(mem.index, mapped_memory);
    else
        Ldi(0, mapped_memory);

    if(mem.scale != 0)
    {
        Ldi(mem.scale, mapped_memory);

        const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVMul);
        mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
    }
    else
    {
        Ldi(0, mapped_memory);

        const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVAdd);
        mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
    }
}


HOT_PATH FORCE_INLINE void Ldm(const ZydisDecodedOperand::ZydisDecodedOperandMem_& mem, MappedMemory& mapped_memory)
{
    std::puts("[DEBUG] Emitting -> LDM");
    // Unroll the memory addressing and place the value on the stack
    UnrollMemoryAddressing(mem, mapped_memory);

    // Load the data specified at the unrolled memory addressing
    auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kLdm);
    mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
}


/**
 * @brief 
 * 2 operands instructions are very commin in x86, instead of creating a switch every time, this function
 * will handle the logic of generating the proper virtual instructions
 * 
 * @param operand The operand to be handled by the function
 * @param mapped_memory 
 * Mapped memory which will receive the virtual instructions
 * @return HOT_PATH 
 */
HOT_PATH FORCE_INLINE void HandleLoadGenericOperands(
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
            std::cout << "[DEBUG]: " << DebugPrintReg(first_operand.mem.base) << "\n";
            Ldm(first_operand.mem, mapped_memory);
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
            Ldm(first_operand.mem, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_IMMEDIATE:
            Ldi(second_operand.imm, mapped_memory);
            break;
        default:
            break;
    }
}

HOT_PATH FORCE_INLINE void HandleSaveGeneric(
    const ZydisDecodedOperand& operand,
    MappedMemory& mapped_memory
)
{
    switch(operand.type)
    {
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_REGISTER:
            Svr(operand.reg.value, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_MEMORY:
            // std::cout << "[DEBUG]: " << DebugPrintReg(operand.mem.base) << "\n";
            // Ldm(operand.mem, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_POINTER:
            // LdPtr(operands[0].ptr.segment, mapped_memory)
            break;
        default:
            break;
    }
}

HOT_PATH FORCE_INLINE void SubInstLogic(
        const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
        MappedMemory& mapped_memory
)
{
    HandleLoadGenericOperands(operands, mapped_memory);

    std::puts("[DEBUG] Emitting -> kVSUB");
    const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVSub);
    mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());

    HandleSaveGeneric(operands[0], mapped_memory);
}

HOT_PATH FORCE_INLINE void AddInstLogic(
    const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
    MappedMemory& mapped_memory
)
{
    HandleLoadGenericOperands(operands, mapped_memory);

    const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVAdd);
    mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
}

HOT_PATH FORCE_INLINE void MovInstLogic(
    const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
    MappedMemory& mapped_memory
)
{
    HandleLoadGenericOperands(operands, mapped_memory);

    // const auto inst = Virtual::Instruction(Parameter(Parameter::kNone))
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
        case ZydisMnemonic::ZYDIS_MNEMONIC_ADD:
            AddInstLogic(operands, mapped_memory);
            break;
        case ZydisMnemonic::ZYDIS_MNEMONIC_MOV:
            MovInstLogic(operands, mapped_memory);
            break;
        default:
            return Err(TranslationError::INSTRUCTION_NOT_FOUND);
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
        // std::puts(text_buffer);

        const auto translation_result = Translation::TranslateInstruction(
            instruction,
            operands,
            virtual_memory
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

    // Generate instruction to notify the virtual machine that the execution is over.
    // The machine should restore everything and return to the caller
    const auto exit_inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVmExit);
    virtual_memory.Write<Virtual::InstructionLength>(exit_inst.AssembleInstruction());

    return Ok(virtual_memory);
}