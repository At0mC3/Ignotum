#include <iostream>
#include <string_view>
#include <fstream>
#include <filesystem>
#include <functional>
#include <vector>
#include <utility>
#include <cstring>
#include <cstdint>
#include <random>

#include <Main.hpp>
#include <Translation.hpp>
#include <Virtual.hpp>
#include <MappedMemory.hpp>
#include <Assembler.hpp>
#include <NativeEmitter/x64NativeEmitter.hpp>
#include <TranslationContext.hpp>

#include <result.h>
#include <Zydis/Zydis.h>
#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>

#define DEBUG

inline void Panic(const char* msg)
{
    std::puts(msg);
    std::exit(-1);
}

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

std::optional<MappedMemory> LoadVirtualMachine(const std::string& path)
{
    std::filesystem::path p{path};
    if(!std::filesystem::exists(p) && !std::filesystem::is_regular_file(p))
        return {};

    std::ifstream ifs(p, std::ios::binary);

    const auto file_size = std::filesystem::file_size(p);

    auto mapped_memory = MappedMemory::Allocate(file_size).unwrap();
    ifs.read(std::bit_cast<char*>(mapped_memory.InnerPtr().get()), file_size);

    return mapped_memory;
}

inline std::uint16_t Generate16BitKey()
{
    std::random_device rd;
    std::independent_bits_engine<std::mt19937, 16, std::uint16_t> key_engine(rd());
    return key_engine();
}

std::uint32_t EncodeVIPEntry(std::uint32_t original_vip, std::uint16_t key_value)
{
    const auto key1 = (std::uint8_t)key_value;
    const auto key2 = (std::uint8_t)(key_value >> 8);

    auto enc_vip = original_vip ^ ((std::uint32_t)key1 << 8);
    enc_vip = enc_vip ^ ((std::uint32_t)key2);

    return (enc_vip << 16) | key_value;
}

argparse::ArgumentParser ParseCmdArgs(int argc, char** argv)
{
    argparse::ArgumentParser arg_parser("Project Ignotum");

    arg_parser.add_argument("--input", "-i")
        .help("Path of the file to be translated")
        .required();

    arg_parser.add_argument("--vm")
        .help("Path of the virtual machine")
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

    return arg_parser;
}

int BeginTranslationProcess(const Main::BeginProcessContext& process_context)
{
    // Keeps track of where we're at in the virtual code section
    // The section can't grow more than 4.2gb because of the windows header definition
    std::uint32_t vcode_offset{0};

    auto native_emitter = std::make_shared<x64NativeEmitter>();

    // Go over every region specified to translated them
    for(const auto& pair : process_context.region_pairs)
    {
        const auto block_size = pair.second;
        const auto start_address = pair.first;

        Translation::Context context(
            start_address, // Rva of the original instructions to maybe do some fixups for relative addressing
            block_size,
            process_context.vm_section.VirtualAddress, // Pass the start of the vm RVA and the size of it
            process_context.vm_section.SizeOfRawData,
            process_context.vcode_section.VirtualAddress + vcode_offset, // Pass where we currently at in the virtualized code section
            process_context.vcode_section.SizeOfRawData - vcode_offset // Substract the offset to keep a accurate size
        );

#ifdef DEBUG
        spdlog::info("Start RVA: 0x{:X}", start_address);
        spdlog::info("Block size: 0x{:X}", block_size);
#endif
        // Load that section of the file in memory to start going over the instructions
        auto instruction_block = process_context.pe_file->LoadRegion(start_address, block_size)
                .expect("The provided address could not be loaded in memory");

        const auto translated_block_res = Translation::TranslateInstructionBlock(instruction_block, native_emitter, context);
        if(!translated_block_res)
            Panic("The translation failed");


        const auto ign2_write_res = process_context.pe_file->WriteToRegion(process_context.vcode_section.VirtualAddress + vcode_offset, translated_block_res.value());
        if(ign2_write_res.isErr())
            Panic("The translated code could not be written to the code section");

        // Update the offset
        vcode_offset += translated_block_res.value().CursorPos();

        // Write the patched instructions to the buffer to patch the region
        const std::uint32_t section_offset_raw = process_context.vcode_section.VirtualAddress - process_context.vm_section.VirtualAddress;
        if(section_offset_raw > std::numeric_limits<std::uint16_t>::max())
            Panic("The section offset is too big");

        // Generate a unique key to encode the VIP
        // This could help with scanning
        const auto enc_key = Generate16BitKey();
        const std::uint32_t encoded_section_offset = EncodeVIPEntry(section_offset_raw, enc_key);
        
        if(!native_emitter->EmitPush32Bit(encoded_section_offset, instruction_block))
            Panic("The buffer is too small to call the virtual machine");

        const auto call_offset = process_context.vm_section.VirtualAddress - (pair.first + instruction_block.CursorPos());
        if(!native_emitter->EmitNearCall(call_offset, instruction_block))
            Panic("The buffer is too small to call the virtual machine");

        const auto size_remaining = instruction_block.Size() - instruction_block.CursorPos();
        std::memset(instruction_block.InnerPtr().get() + instruction_block.CursorPos(), '\x90', size_remaining);

        // Write the new buffer to patch them to call the virtual machine
        const auto native_overwrite_res = process_context.pe_file->WriteToRegion(pair.first, instruction_block);
        if(native_overwrite_res.isErr())
            Panic("Could not patch the original native code");
    }

    return 0;
}

int main([[maybe_unused]]int argc, [[maybe_unused]]char** argv) 
{
    // MIGHT NOT RETURN
    const auto cmd_args = ParseCmdArgs(argc, argv);

    const auto file_path = cmd_args.get<std::string>("--input");
    const auto path_handle = ValidateFile(file_path).expect("The given file is not valid");

    const auto vm_path = cmd_args.get<std::string>("--vm");
    const auto virtual_machine_res = LoadVirtualMachine(vm_path);
    if(!virtual_machine_res) {
        Panic("The path for the virtual machine is invalid");
    }

    const auto virtual_machine = *virtual_machine_res;

    // Parse the exe file to begin the translation process
    auto pe_file_res = PeFile::Load(path_handle, PeFile::LoadOption::FULL_LOAD);
    if(pe_file_res.isErr()) {
        spdlog::critical("Failed to load the PE file: MSG-> {}", pe_file_res.unwrapErr());
        return -1;
    }

    auto pe_file = pe_file_res.unwrap();

    // Create the first region which will hold the virtual machine
    // Write the vm to it
    const auto vm_region_size = 0x1000;
    const auto ign1_region_res = pe_file->AddSection(".Ign1", vm_region_size);
    if(!ign1_region_res) {
        Panic("Failed to add the first region for the virtual machine");
    }

    // Write the virtual machine in the section
    const auto ign1_region = *ign1_region_res;
    pe_file->WriteToRegion(ign1_region.VirtualAddress, virtual_machine).unwrap();

    const auto vcode_region_size = 0x1000;
    // Create the second region which will hold all of the translated code
    const auto ign2_region_res = pe_file->AddSection(".Ign2", vcode_region_size);
    if(!ign2_region_res) {
        Panic("Failed to add the second region for the virtualized code");
    }
    const auto ign2_region = *ign2_region_res;

    // Once the file was successfully loaded, we manage the specified block for translation
    const auto regions = cmd_args.get<std::vector<std::uint64_t>>("--block");
    const auto region_pairs = ValidateRegions(regions).expect("Failed to pair the regions");

    Main::BeginProcessContext process_context(pe_file, ign1_region, ign2_region, region_pairs);
    
    return BeginTranslationProcess(process_context);
}