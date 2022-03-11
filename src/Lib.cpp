#include <filesystem>
#include <string_view>

#include <Translation.hpp>
#include <PeFile.hpp>
#include <result.h>

#ifdef _WIN32
#define DllExport __declspec( dllexport )
#endif

#ifdef __unix__
#define DllExport __attribute__((visibility("default")))
#endif

#ifdef _WIN32
#pragma pack(push, 1)
#endif
#ifdef __unix__
__attribute__((packed));
#endif
struct Query 
{
     const char* file_path; // Path of the file to be translated
     size_t* regions; // Array of address to find the code to be translated
     size_t regions_length; // Size of the array
};
#ifdef _WIN32
#pragma pack(pop)
#endif

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
Result<std::vector<std::pair<std::size_t, std::size_t>>, const char*> ValidateRegions(const size_t* regions, const size_t& size)
{
    if(size % 2 != 0)
        return Err("The format of the regions is invalid");

    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    for(auto i = 0; i < size - 1; i += 2)
    {
        pairs.emplace_back(std::make_pair(regions[i], regions[i+1]));
    }

    return Ok(pairs);
}

extern "C" DllExport int Obfuscate(const Query& query)
{
    const auto file_path = std::string_view(query.file_path);
    const auto validate_path_res = ValidateFile(file_path);
    if(validate_path_res.isErr())
        return -1;

    const auto path_handle = validate_path_res.unwrap();

    // Parse the exe file to begin the translation process
    const auto pe_file_res = PeFile::Load(path_handle, PeFile::LoadOption::FULL_LOAD);
    if(pe_file_res.isErr())
        return -1;

    auto pe_file = pe_file_res.unwrap();

    pe_file->AddSection(".vm");

    const auto region_pairs_result = ValidateRegions(query.regions, query.regions_length);
    if(region_pairs_result.isErr())
        return -1;

    const auto region_pairs = region_pairs_result.unwrap();

    // Go over every region specified to translated them
    for(const auto& pair : region_pairs)
    {
        const auto block_size = pair.second;
        const auto start_address = pair.first;

#ifdef DEBUG    
        std::cout << std::hex << "Block size: " << block_size << "\n" << "Start address: " << start_address << "\n";
#endif
        // Load that section of the file in memory to start going over the instructions
        const auto instruction_block_res = pe_file->LoadRegion(start_address, block_size);
        if(instruction_block_res.isErr())
            return -1;

        const auto instruction_block = instruction_block_res.unwrap();

        const auto translated_block = Translation::TranslateInstructionBlock(instruction_block);
    }

    return 0;
}
