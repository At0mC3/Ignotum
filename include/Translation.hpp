#ifndef INCLUDE_TRANSLATION_
#define INCLUDE_TRANSLATION_

// Std libraries
#include <iostream>
#include <functional>
#include <bit>
#include <variant>
#include <cstdint>
#include <optional>
#include <array>
#include <memory>

// Project libraries
#include <Virtual.hpp>
#include <Parameter.hpp>
#include <MappedMemory.hpp>
#include <utl/Utl.hpp>
#include <NativeEmitter/NativeEmitter.hpp>
#include <TranslationContext.hpp>

// 3rd party Library
#include <Zydis/Zydis.h>
#include <result.h>
#include <spdlog/spdlog.h>

namespace Translation
{
    using Virtual::Parameter;
    enum class RetResult
    {
        OK, // Everything went perfectly fine
        INSTRUCTION_NOT_SUPPORTED, // The equivalent virtual instruction doesn't exist. A switch is needed
        OUT_OF_MEMORY          // The mapped memory object ran out of space with it's internal buffer
    };

    static constexpr std::array<std::uint16_t, 16> register_map = 
    {
        128,
        16,
        24,
        8,
        32,
        40,
        48,
        56,
        64,
        72,
        80,
        88,
        96,
        104,
        112,
        120
    };

    HOT_PATH FORCE_INLINE std::uint16_t GetRegisterIndex(const ZydisRegister &reg)
    {
        const auto base = 53;
        const auto reg_index = static_cast<std::uint16_t>(reg) - base;

        return register_map[reg_index];
    }

    HOT_PATH FORCE_INLINE bool Ldr(const ZydisRegister &reg, MappedMemory &mapped_memory)
    {
        spdlog::info("Emitting -> LDR");
        const auto vm_reg_index = GetRegisterIndex(reg);
        const auto inst = Virtual::Instruction(Virtual::Parameter(vm_reg_index), Virtual::Command::kLdr);

        return mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
    }

    HOT_PATH FORCE_INLINE bool Ldi(const ZydisDecodedOperandImm &imm, MappedMemory &mapped_memory)
    {
        spdlog::info("Emitting -> LDI");
        // Construct the instruction and write it to the memory
        const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kLdImm);

        if(!mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction()))
            return false;

        // assert(!imm.is_signed && "Signed value not supported in Ldm");

        auto unsigned_imm = imm.value.u;
        // Write the immediate in the mapped memory
        return mapped_memory.Write<decltype(unsigned_imm)>(unsigned_imm);
    }

    HOT_PATH FORCE_INLINE bool Ldi(const std::uint64_t &imm, MappedMemory &mapped_memory)
    {
        spdlog::info("Emitting -> LDI");
        // Construct the instruction and write it to the memory
        const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kLdImm);
        if(!mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction()))
            return false;

        auto unsigned_imm = imm;
        // Write the immediate in the mapped memory
        return mapped_memory.Write<decltype(unsigned_imm)>(unsigned_imm);
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
    HOT_PATH FORCE_INLINE bool UnrollMemoryAddressing(const ZydisDecodedOperandMem &mem, MappedMemory &mapped_memory)
    {
        spdlog::info("Starting memory unrolling sequence.");
        // Load the content of the base register on the stack
        // If the operation doesn't use a base, just load 0
        if (mem.base != ZYDIS_REGISTER_NONE)
        {
            if(!Ldr(mem.base, mapped_memory))
                return false;
        }
        else
        {
            if(!Ldi(0, mapped_memory))
                return false;
        }

        // Load the content of the base register on the stack
        // If the operation doesn't use a base, just load 0
        if (mem.disp.has_displacement)
        {
            if(!Ldi(mem.disp.value, mapped_memory))
                return false;
        }
        else
        {
            if(!Ldi(0, mapped_memory))
                return false;
        }

        spdlog::info("Emitting -> kVADD");
        // Generate the virtual instruction to add both values together
        const auto add_inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVAdd);
        if(!mapped_memory.Write<Virtual::InstructionLength>(add_inst.AssembleInstruction()))
            return false;

        // Load the content of the index register on the stack
        // If the operation doesn't use a index, just load 0
        if (mem.index != ZYDIS_REGISTER_NONE)
        {
            if(!Ldr(mem.index, mapped_memory))
                return false;
        }
        else
        {
            if(!Ldi(0, mapped_memory))
                return false;
        }

        if (mem.scale != 0)
        {
            if(!Ldi(mem.scale, mapped_memory))
                return false;

            spdlog::info("Emitting -> kVMUL");
            const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVMul);
            
            if(!mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction()))
                return false;
        }
        else
        {
            if(!Ldi(0, mapped_memory))
                return false;

            spdlog::info("Emitting -> kVADD");
            const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVAdd);
            
            if(!mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction()))
                return false;
        }

        spdlog::info("Emitting -> kVADD");
        
        if(!mapped_memory.Write<Virtual::InstructionLength>(add_inst.AssembleInstruction()))
            return false;

        spdlog::info("Memory unrolling sequence done.");

        return true;
    }

    HOT_PATH FORCE_INLINE bool Svr(const ZydisRegister &reg, MappedMemory &mapped_memory)
    {
        spdlog::info("Emitting -> SVR");
        const auto vm_reg_index = GetRegisterIndex(reg);
        const auto inst = Virtual::Instruction(Virtual::Parameter(vm_reg_index), Virtual::Command::kVSvr);
        
        return mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
    }

    HOT_PATH FORCE_INLINE bool Svm(const ZydisDecodedOperandMem& mem, MappedMemory &mapped_memory)
    {
        if(!UnrollMemoryAddressing(mem, mapped_memory))
            return false;

        spdlog::info("Emitting -> SVM");
        const auto inst = Virtual::Instruction(Virtual::Parameter(Parameter::kNone), Virtual::Command::kVSvm);
        
        return mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
    }

    HOT_PATH FORCE_INLINE bool Ldm(const ZydisDecodedOperandMem &mem, MappedMemory &mapped_memory)
    {
        // Unroll the memory addressing and place the value on the stack
        if(!UnrollMemoryAddressing(mem, mapped_memory))
            return false;

        spdlog::info("Emitting -> LDM");

        // Load the data specified at the unrolled memory addressing
        auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kLdm);
        return mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
    }

    HOT_PATH FORCE_INLINE bool Ldm(MappedMemory &mapped_memory)
    {
        spdlog::info("Emitting -> LDM");
        // Load the data specified at the unrolled memory addressing
        auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kLdm);
        
        return mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
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
    HOT_PATH FORCE_INLINE bool HandleLoadGenericOperands(
        const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
        MappedMemory &mapped_memory)
    {
        const auto first_operand = operands[0];
        switch (first_operand.type)
        {
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_REGISTER:
            if(!Ldr(first_operand.reg.value, mapped_memory))
                return false;
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_MEMORY:
            if(!Ldm(first_operand.mem, mapped_memory))
                return false;
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_POINTER:
            // if(!LdPtr(operands[0].ptr.segment, mapped_memory))
                // return false;
            break;
        default:
            return true;
            break;
        }

        const auto second_operand = operands[1];
        switch (second_operand.type)
        {
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_REGISTER:
            return Ldr(first_operand.reg.value, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_MEMORY:
            return Ldm(first_operand.mem, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_IMMEDIATE:
            return Ldi(second_operand.imm, mapped_memory);
            break;
        default:
            return true;
            break;
        }
    }

    HOT_PATH FORCE_INLINE bool HandleLoadSourceOperand(
        const ZydisDecodedOperand& source_operand,
        MappedMemory &mapped_memory)
    {
        switch (source_operand.type)
        {
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_REGISTER:
            return Ldr(source_operand.reg.value, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_MEMORY:
            return Ldm(source_operand.mem, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_IMMEDIATE:
            return Ldi(source_operand.imm, mapped_memory);
            break;
        default:
            return true;
            break;
        }
    }

    HOT_PATH FORCE_INLINE bool HandleSaveGeneric(
        const ZydisDecodedOperand &operand,
        MappedMemory &mapped_memory)
    {
        switch (operand.type)
        {
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_REGISTER:
            return Svr(operand.reg.value, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_MEMORY:
            return Svm(operand.mem, mapped_memory);
            // Ldm(operand.mem, mapped_memory);
            break;
        case ZydisOperandType::ZYDIS_OPERAND_TYPE_POINTER:
            // return LdPtr(operands[0].ptr.segment, mapped_memory)
            break;
        default:
            return true;
            break;
        }

        return true;
    }

    HOT_PATH FORCE_INLINE bool SubInstLogic(
        const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
        MappedMemory &mapped_memory)
    {
        if(!HandleLoadGenericOperands(operands, mapped_memory))
            return false;

        spdlog::info("Emitting -> kVSUB");
        const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVSub);
        
        if(!mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction()))
            return false;

        return HandleSaveGeneric(operands[0], mapped_memory);
    }

    HOT_PATH FORCE_INLINE bool AddInstLogic(
        const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
        MappedMemory &mapped_memory)
    {
        if(!HandleLoadGenericOperands(operands, mapped_memory))
            return false;

        spdlog::info("Emitting -> kVADD");
        const auto inst = Virtual::Instruction(Parameter(Parameter::kNone), Virtual::Command::kVAdd);
        
        if(!mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction()))
            return false;
        
        return HandleSaveGeneric(operands[0], mapped_memory);
    }

    HOT_PATH FORCE_INLINE bool MovInstLogic(
        const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
        MappedMemory &mapped_memory)
    {
        if(!HandleLoadSourceOperand(operands[1], mapped_memory))
            return false;

        return HandleSaveGeneric(operands[0], mapped_memory);
    }

    HOT_PATH FORCE_INLINE bool CallInstLogic(
        const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
        MappedMemory &mapped_memory,
        const Translation::Context& context
    )
    {
        assert( !(operands[0].type != ZydisOperandType::ZYDIS_OPERAND_TYPE_IMMEDIATE) && "Invalid call type");

        // Get the relative value of where this will call
        const auto call_relative_imm = operands[0].imm.value.s;

        return true;
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
    HOT_PATH FORCE_INLINE Translation::RetResult TranslateInstruction(
        const ZydisDecodedInstruction &instruction,
        const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
        MappedMemory &mapped_memory,
        const Translation::Context& context,
        bool is_probing
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
    HOT_PATH std::optional<MappedMemory>
    TranslateInstructionBlock(
        const MappedMemory &instruction_block,
        const std::shared_ptr<NativeEmitter> native_emitter,
        const Translation::Context& context
    );
}

#endif