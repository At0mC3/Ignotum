#include <iostream>
#include <string_view>
#include <filesystem>
#include <optional>
#include <memory>
#include <functional>
#include <functional>
#include <Zydis/Zydis.h>

#include <PeFile.hpp>
#include <Translation.hpp>

// Disable the exceptions
#define TOML_EXCEPTIONS 0
#include <toml++/toml.h>

std::vector<std::byte> TranslateInstructionBlock(const std::byte* buffer, const std::size_t& buffer_size)
{
    // Initialize formatter. Only required when you actually plan to do instruction
    // formatting ("disassembling"), like we do here
    // ZydisFormatter formatter;
    // ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

    // Initialize decoder context
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

    std::size_t offset = 0;
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE];

    const auto translated_buffer_size = buffer_size * 5;
    std::vector<std::byte> translated_buffer(translated_buffer_size);

    // Holds where we are at in the buffer which indicates how much was used
    std::size_t translated_buffer_offset = 0;

    while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, buffer + offset, buffer_size - offset,
        &instruction, operands, ZYDIS_MAX_OPERAND_COUNT_VISIBLE, 
        ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY)))
    {
        // Format & print the binary instruction structure to human readable format
        // char buffer[256];
        // ZydisFormatterFormatInstruction(&formatter, &instruction, operands,
        //     instruction.operand_count_visible, buffer, sizeof(buffer), 0);
        // puts(buffer);

        const auto translation_result = Translation::TranslateInstruction(instruction, &translated_buffer.front(), translated_buffer_size - translated_buffer_offset);
        // The result is holding an error and not the amount of bytes written
        if(std::holds_alternative<std::size_t>(translation_result) != true)
        {
            // Handle the instruction not being translated
            std::cout << "Instruction not found\n";
        }
        else
        {
            // Add the size of the newly translated instruction to the index to keep the bounds of the buffer in check
            translated_buffer_offset += std::get<std::size_t>(translation_result);
        }

        offset += instruction.length;
    }

    return translated_buffer;
}

/// Checks whether it's a file and it exists
std::optional<std::filesystem::path> ValidateFile(const std::string_view& file_path)
{
    std::filesystem::path p{file_path};

    if(std::filesystem::exists(p) != true)
        return {};
    if(std::filesystem::is_regular_file(p) != true)
        return {};

    return p;
}

int main([[maybe_unused]]int argc, [[maybe_unused]]char** argv) 
{
    if(argc < 2) 
    {
        std::puts("Not enough arguments were given");
        return 0;
    }

    // Get the second argument as a string view and check if it's a valid file path
    const std::string_view file_path{argv[1]};
    const auto path_handle = ValidateFile(file_path);
    
    if(path_handle == std::nullopt)
    {
        std::puts("The file does not exist or the file is not valid");
        return 0;
    }

    // Parse the exe file to begin the translation process
    auto pe_file = PeFile::Load(*path_handle, PeFile::LoadOption::FULL_LOAD);
    if(pe_file == std::nullopt)
    {
        std::puts("The file provided is not valid.\n");
        return 0;
    }

    const auto BLOCK_SIZE = 199;

    // Load that section of the file in memory to start going over the instructions
    const auto instruction_block = pe_file->LoadByteArea(0x4070, BLOCK_SIZE);
    if(instruction_block == std::nullopt)
    {
        std::puts("The memory block could not be found");
        return 0;
    }
    const auto raw_ptr = (*instruction_block).get();

    const auto translated_block = TranslateInstructionBlock(raw_ptr, BLOCK_SIZE);

    return 0;
}