#include <Translation.hpp>

std::variant<std::size_t, Translation::TranslationError> Translation::TranslateInstruction(
    const ZydisDecodedInstruction &instruction,
    const ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE],
    std::byte *out_buffer,
    const std::size_t &out_buffer_size
)
{
    try 
    {
        // Get the function to translate that instruction inside the hash map
        auto translation_func = vm_translation_map.at(instruction.mnemonic);
        // Call the function retrived from the map
        return translation_func(instruction, operands, out_buffer, out_buffer_size);
    }
    catch(std::out_of_range& ex)
    {
        // Function not found, return error
        return TranslationError::INSTRUCTION_NOT_FOUND;
    }
}