#include <filesystem>
#include <string_view>
#include <cstdint>
#include <cstring>
#include <iostream>

#include <Translation.hpp>
#include <PeFile.hpp>
#include <Assembler.hpp>
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
     const char* vm_path; // Path to the virtual machine
     size_t region; // Array of address to find the code to be translated
     size_t region_size; // Size of the array
};
#ifdef _WIN32
#pragma pack(pop)
#endif

enum class ObfuscateResult : std::uint32_t
{
    kSuccess,
    kInvalidPath,
    kInvalidFile,
    kVmNotFound,
    kBufferTooSmall,
    kInvalidFunctionAddress
};

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


std::optional<MappedMemory> LoadVirtualMachine(const char* path)
{
    std::filesystem::path p{path};
    std::ifstream ifs(p, std::ios::binary);
    if(!ifs.is_open())
        return {};

    const auto file_size = std::filesystem::file_size(p);

    auto mapped_memory_res = MappedMemory::Allocate(file_size);
    if(mapped_memory_res.isErr())
        return {};

    const auto mapped_memory = mapped_memory_res.unwrap();
    ifs.read(std::bit_cast<char*>(mapped_memory.InnerPtr().get()), file_size);

    return mapped_memory;
}

extern "C" DllExport ObfuscateResult Obfuscate(const Query* query)
{
    const auto file_path = std::string_view(query->file_path);
    const auto validate_path_res = ValidateFile(file_path);
    if(validate_path_res.isErr())
        return ObfuscateResult::kInvalidPath;

    const auto path_handle_res = validate_path_res;
    if(path_handle_res.isErr())
        return ObfuscateResult::kInvalidPath;

    const auto path_handle = path_handle_res.unwrap();

    // Parse the exe file to begin the translation process
    const auto pe_file_res = PeFile::Load(path_handle, PeFile::LoadOption::FULL_LOAD);
    if(pe_file_res.isErr())
        return ObfuscateResult::kInvalidFile;

    auto pe_file = pe_file_res.unwrap();

    const auto virtual_machine_res = LoadVirtualMachine(query->vm_path);
    if(!virtual_machine_res)
        return ObfuscateResult::kVmNotFound;

    const auto virtual_machine = *virtual_machine_res;

    // Create the first region which will hold the virtual machine
    // Write the vm to it
    const auto ign1_region_res = pe_file->AddSection(".Ign1");
    if(ign1_region_res.isErr())
        return ObfuscateResult::kInvalidFile;

    const auto ign1_region = ign1_region_res.unwrap();
    pe_file->WriteToRegion(ign1_region.VirtualAddress, virtual_machine).unwrap();

    // Create the second region which will hold all of the translated code
    const auto ign2_region_res = pe_file->AddSection(".Ign2");
    if(ign2_region_res.isErr())
        return ObfuscateResult::kInvalidFile;

    const auto ign2_region = ign2_region_res.unwrap();

    // Go over the region specified to translate them
    const auto block_size = query->region_size;
    const auto start_address = query->region;

    // Load that section of the file in memory to start going over the instructions
    const auto instruction_block_res = pe_file->LoadRegion(start_address, block_size);
    if(instruction_block_res.isErr())
        return ObfuscateResult::kInvalidFunctionAddress;

    auto instruction_block = instruction_block_res.unwrap();

    // Translated the block and then write it in it's region block which is 'Ign2'
    const auto translated_block_res = Translation::TranslateInstructionBlock(instruction_block);
    if(translated_block_res.isErr())
        return ObfuscateResult::kInvalidFunctionAddress;
        

    const auto translated_block = translated_block_res.unwrap();
    pe_file->WriteToRegion(ign2_region.VirtualAddress, translated_block);

    // Calculate the offset of the virtual block relative to the start address of the virtual machine
    // Then, generate the instruction to push it on the stack
    const auto section_offset = ign2_region.VirtualAddress - ign1_region.VirtualAddress;
    if(!X64::Generator::PushX32(instruction_block, section_offset))
        return ObfuscateResult::kBufferTooSmall;

    // Calculate the offset of the virtual machine entry relative to the function we're in right now
    // Then, generate the instruction to call that relative offset
    const auto call_offset = ign1_region.VirtualAddress - (start_address + instruction_block.CursorPos());
    if(!X64::Generator::CallNear(instruction_block, call_offset))
        return ObfuscateResult::kBufferTooSmall;

    // Fill the rest of the function with NOP instructions for now.
    // A mutation engine should be created in the future to put valid instructions
    const auto size_remaining = instruction_block.Size() - instruction_block.CursorPos();
    std::memset(instruction_block.InnerPtr().get() + instruction_block.CursorPos(), '\x90', size_remaining);

    // Write back the original patched function
    pe_file->WriteToRegion(start_address, instruction_block);

    return ObfuscateResult::kSuccess;
}
