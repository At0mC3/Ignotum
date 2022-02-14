#include <iostream>
#include <string_view>
#include <filesystem>
#include <functional>
#include <vector>
#include <utility>

#include <PeFile.hpp>
#include <Translation.hpp>
#include <Virtual.hpp>
#include <MappedMemory.hpp>

#include <result.h>
#include <Zydis/Zydis.h>
#include <argparse/argparse.hpp>

#define DEBUG

/// Checks whether it's a file and it exists
Result<std::filesystem::path, const char*> ValidateFile(const std::string_view& file_path)
{
    std::filesystem::path p{file_path};

    if(!std::filesystem::exists(p))
        return Err("The file does not exist");
    if(!std::filesystem::is_regular_file(p))
        return Err("The format of the file is not valid");

    return Ok(p);
}

Result<std::vector<std::pair<std::size_t, std::size_t>>, const char*> ValidateRegions(const std::vector<std::size_t> &vec)
{
    if(vec.size() % 2 != 0)
        return Err("The format of the regions is invalid");

    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    for(auto i = 0; i < vec.size() - 1; i += 2)
    {
        pairs.emplace_back(std::make_pair(vec[i], vec[i+1]));
    }

    return Ok(pairs);
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
    const auto path_handle = ValidateFile(file_path).expect("The given file is not valid");

    // Parse the exe file to begin the translation process
    auto pe_file = PeFile::Load(path_handle, PeFile::LoadOption::FULL_LOAD)
            .expect("Failed to load the specified file");

    // Once the file was successfully loaded, we manage the specified block for translation
    const auto regions = arg_parser.get<std::vector<std::uint64_t>>("--block");
    const auto region_pairs = ValidateRegions(regions).expect("Failed to pair the regions");

    // Go over every region specified to translated them
    for(const auto& pair : region_pairs)
    {
        const auto block_size = pair.second;
        const auto start_address = pair.first;

#ifdef DEBUG
        std::cout << std::hex << "Block size: " << block_size << "\n" << "Start address: " << start_address << "\n";
#endif
        // Load that section of the file in memory to start going over the instructions
        const auto instruction_block = pe_file->LoadRegion(start_address, block_size)
                .expect("The provided address could not be loaded in memory");

        const auto translated_block = Translation::TranslateInstructionBlock(instruction_block);
    }

    return 0;
}