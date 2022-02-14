#include <Translation.hpp>

Result<bool, Translation::TranslationError> Translation::TranslateInstruction(
    const ZydisDecodedInstruction &instruction,
    const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
    MappedMemory& mapped_memory
)
{
    return Ok(true);
}