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

HOT_PATH Result<bool, Translation::TranslationError> Translation::TranslateInstruction(
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