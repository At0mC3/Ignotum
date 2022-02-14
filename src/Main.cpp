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

/**
 * @brief In charge of validating a given path. It will check if exists.
 * Then it will check if the file is valid and not a directory
 * If it's valid, a filesystem::path will be returned
 * 
 * @param file_path The path of the file to be checked
 * @return Result<std::filesystem::path, const char*>
 * Either a path for the given path string or an error message 
 */
Result<std::filesystem::path, const char*> ValidateFile(const std::string_view& file_path)
{
    std::filesystem::path p{file_path};

    if(!std::filesystem::exists(p))
        return Err("The file does not exist");
    if(!std::filesystem::is_regular_file(p))
        return Err("The format of the file is not valid");

    return Ok(p);
}

/**
 * @brief Given a vector of n amount of size_t, this function check if the size of it is big enough for pairs
 * by using modulo. If the format is correct, they are then but in pairs to make them more readable when processing
 * 
 * @param vec The vector containing all of the addresses and the size of them
 * @return Result<std::vector<std::pair<std::size_t, std::size_t>>, const char*>
 * A vector of pair is returned is it was successful, otherwise, an error message is returned 
 */
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