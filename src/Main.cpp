#include <iostream>
#include <string_view>
#include <filesystem>
#include <functional>
#include <vector>

#include <PeFile.hpp>
#include <Translation.hpp>
#include <Virtual.hpp>

#include <utl/Result.hpp>
using utl::Result;
using utl::Ok;
using utl::Err;

#include <Zydis/Zydis.h>
#include <argparse/argparse.hpp>

std::vector<std::byte> TranslateInstructionBlock(const std::byte* buffer, const std::size_t& buffer_size)
{
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

    const auto translated_buffer_size = buffer_size * 5;
    std::vector<std::byte> translated_buffer(translated_buffer_size);

    // Holds where we are at in the buffer which indicates how much was used
    std::size_t translated_buffer_offset = 0;

    while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, buffer + offset, buffer_size - offset,
        &instruction, operands, ZYDIS_MAX_OPERAND_COUNT_VISIBLE, 
        ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY)))
    {
        // Format & print the binary instruction structure to human readable format
        char buffer[256];
        ZydisFormatterFormatInstruction(&formatter, &instruction, operands,
            instruction.operand_count_visible, buffer, sizeof(buffer), 0);
        puts(buffer);

        const auto translation_result = Translation::TranslateInstruction(
            instruction,
            operands,
            (&translated_buffer.front()) + translated_buffer_offset, 
            translated_buffer_size - translated_buffer_offset
        );

        // The result is holding an error and not the amount of bytes written
        if(!std::holds_alternative<std::size_t>(translation_result))
        {
            // Handle the instruction not being translated
            // std::cout << "Instruction not found\n";
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
Result<std::filesystem::path, const char*> ValidateFile(const std::string_view& file_path)
{
    std::filesystem::path p{file_path};

    if(!std::filesystem::exists(p))
        return Err<std::filesystem::path, const char*>("The file does not exist");
    if(!std::filesystem::is_regular_file(p))
        return Err<std::filesystem::path, const char*>("The format of the file is not valid");

    return Ok<std::filesystem::path, const char*>(p);
}

int main([[maybe_unused]]int argc, [[maybe_unused]]char** argv) 
{
    argparse::ArgumentParser arg_parser("Project Ignotum");

    arg_parser.add_argument("--path", "-p")
        .help("Path of the file to be translated")
        .required();

    arg_parser.add_argument("--block", "-b")
        .help("Used to specify the block to be translated. The format used is: --block [address] [size]")
        .scan<'x', std::uint64_t>()
        .nargs(2)
        .append()
        .required();

    try
    {
        arg_parser.parse_args(argc, argv);
    }
    catch (const std::exception& error)
    {
        std::cout << error.what() << "\n";
        std::cout << arg_parser << "\n";
        std::exit(0);
    }

    const auto file_path = arg_parser.get<std::string>("--path");
    const auto path_handle = ValidateFile(file_path).Expect("The given file is not valid");

    // Parse the exe file to begin the translation process
    auto pe_file = PeFile::Load(path_handle, PeFile::LoadOption::FULL_LOAD)
            .Expect("Failed to load the specified file");

    // Once the file was successfully loaded, we manage the specified block for translation
    auto regions = arg_parser.get<std::vector<std::uint64_t>>("--block");
    for(const auto& value : regions)
    {
        std::cout << std::hex << value << "\n";
    }

    const auto BLOCK_SIZE = 0x9f;

    // Load that section of the file in memory to start going over the instructions
    const auto instruction_block = pe_file->LoadByteArea(0x4070, BLOCK_SIZE)
            .Expect("The provided address could not be loaded in memory");

    const auto raw_ptr = instruction_block.get();

    const auto translated_block = TranslateInstructionBlock(raw_ptr, BLOCK_SIZE);

    return 0;
}