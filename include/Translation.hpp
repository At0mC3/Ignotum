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
    using Virtual::Parameter;
    enum TranslationError
    {
        INSTRUCTION_NOT_FOUND, // The equivalent virtual instruction doesn't exist. A switch is needed
        OUT_OF_MEMORY          // The mapped memory object ran out of space with it's internal buffer
    };

    HOT_PATH FORCE_INLINE void Ldr(const ZydisRegister &reg, MappedMemory &mapped_memory)
    {
        std::puts("[DEBUG] Emitting -> LDR");
        const auto inst = Virtual::Instruction(Virtual::Parameter(static_cast<std::uint16_t>(reg)), Virtual::Command::kLdr);
        mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
    }

    HOT_PATH FORCE_INLINE void Ldi(const ZydisDecodedOperand::ZydisDecodedOperandImm_ &imm, MappedMemory &mapped_memory)
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

    HOT_PATH FORCE_INLINE void Ldi(const std::uint64_t &imm, MappedMemory &mapped_memory)
    {
        // Construct the instruction and write it to the memory
        const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kLdImm);
        mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());

        auto unsigned_imm = imm;
        // Write the immediate in the mapped memory
        mapped_memory.Write<decltype(unsigned_imm)>(unsigned_imm);
    }

    HOT_PATH FORCE_INLINE void Svr(const ZydisRegister &reg, MappedMemory &mapped_memory)
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
    HOT_PATH FORCE_INLINE void UnrollMemoryAddressing(const ZydisDecodedOperand::ZydisDecodedOperandMem_ &mem, MappedMemory &mapped_memory)
    {
        // Load the content of the base register on the stack
        // If the operation doesn't use a base, just load 0
        if (mem.base != ZYDIS_REGISTER_NONE)
            Ldr(mem.base, mapped_memory);
        else
            Ldi(0, mapped_memory);

        // Load the content of the base register on the stack
        // If the operation doesn't use a base, just load 0
        if (mem.disp.has_displacement)
            Ldi(mem.disp.value, mapped_memory);
        else
            Ldi(0, mapped_memory);

        // Generate the virtual instruction to add both values together
        const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVAdd);
        mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());

        // Load the content of the index register on the stack
        // If the operation doesn't use a index, just load 0
        if (mem.index != ZYDIS_REGISTER_NONE)
            Ldr(mem.index, mapped_memory);
        else
            Ldi(0, mapped_memory);

        if (mem.scale != 0)
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

    HOT_PATH FORCE_INLINE void Ldm(const ZydisDecodedOperand::ZydisDecodedOperandMem_ &mem, MappedMemory &mapped_memory)
    {
        std::puts("[DEBUG] Emitting -> LDM");
        // Unroll the memory addressing and place the value on the stack
        UnrollMemoryAddressing(mem, mapped_memory);

        // Load the data specified at the unrolled memory addressing
        auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kLdm);
        mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
    }

    HOT_PATH FORCE_INLINE void Ldm(MappedMemory &mapped_memory)
    {
        std::puts("[DEBUG] Emitting -> LDM");
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
        MappedMemory &mapped_memory)
    {
        const auto first_operand = operands[0];
        switch (first_operand.type)
        {
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_REGISTER:
            Ldr(first_operand.reg.value, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_MEMORY:
            Ldm(first_operand.mem, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_POINTER:
            // LdPtr(operands[0].ptr.segment, mapped_memory)
            break;
        default:
            break;
        }

        const auto second_operand = operands[1];
        switch (second_operand.type)
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
        const ZydisDecodedOperand &operand,
        MappedMemory &mapped_memory)
    {
        switch (operand.type)
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
        MappedMemory &mapped_memory)
    {
        HandleLoadGenericOperands(operands, mapped_memory);

        std::puts("[DEBUG] Emitting -> kVSUB");
        const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVSub);
        mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());

        HandleSaveGeneric(operands[0], mapped_memory);
    }

    HOT_PATH FORCE_INLINE void AddInstLogic(
        const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
        MappedMemory &mapped_memory)
    {
        HandleLoadGenericOperands(operands, mapped_memory);

        const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVAdd);
        mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
    }

    HOT_PATH FORCE_INLINE void MovInstLogic(
        const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
        MappedMemory &mapped_memory)
    {
        HandleLoadGenericOperands(operands, mapped_memory);

        // const auto inst = Virtual::Instruction(Parameter(Parameter::kNone))
    }

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
        MappedMemory &mapped_memory);

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
    HOT_PATH Result<MappedMemory, int> TranslateInstructionBlock(const MappedMemory &instruction_block);
}

#endif