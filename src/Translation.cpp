#include <Translation.hpp>

std::variant<std::size_t, Translation::TranslationError> Translation::TranslateInstruction(
    const ZydisDecodedInstruction &instruction, 
    std::byte *out_buffer, \
    const std::size_t &out_buffer_size
)
{
    try 
    {
        // Get the function to translate that instruction inside the hash map
        auto translation_func = vm_translation_map.at(instruction.mnemonic);
        // Call the function retrived from the map
        return translation_func(instruction, nullptr, 0x200);
    }
    catch(std::out_of_range& ex)
    {
        // Function not found, return error
        return TranslationError::INSTRUCTION_NOT_FOUND;
    }
}