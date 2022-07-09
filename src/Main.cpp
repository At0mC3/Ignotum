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
#include <Cryptography.hpp>

#include <result.h>
#include <Zydis/Zydis.h>
#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>

#define DEBUG

/**
 * @brief
 * Displays a messages before quiting.
 * This function does not return
 *
 * @param msg Text to be displayed before the exit
 */
[[noreturn]] inline void Panic(const char* msg)
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
Result<std::filesystem::path, const char*>
ValidateFile(const std::string_view& file_path)
{
    std::filesystem::path p{file_path};
    
    // my thought here is that you could print a descriptive error message like "The test.txt file does not exist" or "test.txt formatting is not valid"
    if(!std::filesystem::exists(p))
        return Err("The file does not exist");
    if(!std::filesystem::is_regular_file(p))
        return Err("The format of the file is not valid");

    return Ok(p);
}

/**
 * @brief Given a vector of n amount of size_t, this function check if the size of it is big enough for pairs
 * by using modulo. If the format is correct, they are then put in pairs to make them more readable when processing
 *
 * @param vec The vector containing all of the addresses and the size of them
 * @return Result<std::vector<std::pair<std::size_t, std::size_t>>, const char*>
 * A vector of pair is returned is it was successful, otherwise, an error message is returned
 */
Result<std::vector<std::pair<std::size_t, std::size_t>>, const char*>
ValidateRegions(const std::vector<std::size_t> &vec)
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

/**
 * @brief
 * Using a filesystem path, the raw binary for the virtual machine
 * is loaded in memory.
 * @param path The path for the location of the virtual machine .bin file.
 * @return std::optional<MappedMemory> If the function succeeds,
 * the buffer will be returned as a MappedMemory object. If it fails,
 * std::nullopt is returned.
 */
std::optional<MappedMemory>
LoadVirtualMachine(const std::string& path)
{
    // Convert the std::string path into a filesystem path object for easy use
    std::filesystem::path p{path};

    //maybe this is a stupid question but why not use your helper function you made earlier
    // Result<std::filesystem::path, const char* ValidateFile(const std::string_view& file_path)
    // this one^
    if(!std::filesystem::exists(p) && !std::filesystem::is_regular_file(p)) {
        return {};
    }

    // Open a stream of the file and make sure the handle is opened
    std::ifstream ifs(
        p,
        std::ios::binary | std::ios::in
    );

    if(!ifs.is_open()) {
        return {};
    }

    const auto file_size = std::filesystem::file_size(p);

    auto mapped_memory = MappedMemory::Allocate(file_size);
    if(!mapped_memory) {
        return {};
    }

    ifs.read(
        std::bit_cast<char*>(mapped_memory.value().InnerPtrRaw()),
        file_size
    );

    return mapped_memory;
}

/**
 * @brief
 * The function is in charge of initializing the argparse library to parse
 * the command line arguments. Once everything was initialized and parsed,
 * the object containing the flags is returned.
 *
 * @param argc
 * @param argv
 * @return argparse::ArgumentParser Object containing the parsed command line
 * arguments.
 */
argparse::ArgumentParser
InitAndParseCmdArgs(int argc, char** argv)
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

/**
 * @brief
 * This function goes over all of the provided virtual addresses and loads the regions
 * in memory. Once loaded, these regions are then translated to the custom p-code
 *
 * @param process_context
 * @return int The value 0 is returned to indicate a success
 */
int BeginTranslationProcess(const Main::BeginProcessContext& process_context)
{
    // Keeps track of where we're at in the virtual code section
    // The section can't grow more than 4.2gb because of the Windows header definition
    std::uint32_t vcode_offset{0};

    auto native_emitter = std::make_shared<x64NativeEmitter>();

    // Go over every region specified to translate them
    for(const auto& pair : process_context.region_pairs)
    {
        const auto block_size = pair.second;
        const auto start_address = pair.first;

        Translation::Context context(
            start_address, // Rva of the original instructions to maybe do some fixups for relative addressing
            block_size, // The size of the block
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

        // Translate the whole instruction block. The p-code should be returned
        // From this function.
        const auto translated_block_res = Translation::TranslateInstructionBlock(
            instruction_block, native_emitter, context
        );

        if(!translated_block_res) {
            Panic("The translation failed");
        }

        /*
        Everything was translated succesfully, write it to the '.Ign2' section
        Inside the pe file.
        */
        const auto ign2_write_res = process_context.pe_file->WriteToRegionPos(
            context.vcode_block_rva,
            translated_block_res.value()
        );

        if(ign2_write_res.isErr()) {
            spdlog::critical("Writing to section failed with msg: {}", ign2_write_res.unwrapErr());
            return -1;
        }

        // Update the offset with the size of the translated block
        vcode_offset += translated_block_res.value().CursorPos();

        // Write the patched instructions to the buffer to patch the region
        const std::uint32_t section_offset_raw = context.vcode_block_rva - process_context.vm_section.VirtualAddress;
        if(section_offset_raw > std::numeric_limits<std::uint16_t>::max()) {
            Panic("The section offset is too big");
        }

        // Generate a unique key to encode the VIP(virtual instruction pointer)
        const auto enc_key = cryptography::Generate16BitKey();
        const std::uint32_t encoded_section_offset = cryptography::EncodeVIPEntry(section_offset_raw, enc_key);
        std::cout << "VIP: " << section_offset_raw << "\n";


        /// Patching section.
        /// This part is in charge of removing the original instructions with
        /// NOPs.

        // Emit a push instruction with the encoded value containing the
        // offset of where the vip should start
        // In x86, this would look like this [push 0xdeadbeef]
        if(!native_emitter->EmitPush32Bit(encoded_section_offset, instruction_block)) {
            Panic("The buffer is too small to call the virtual machine");
        }

        // Calculate the distance from the rva to the virtual machine inside the file
        // This offset will be used to generate a call inside the virtual machine
        const auto call_offset = process_context.vm_section.VirtualAddress - (pair.first + instruction_block.CursorPos());

        // Emit the call instruction using the relative offset that we just calculated
        if(!native_emitter->EmitNearCall(call_offset, instruction_block)) {
            Panic("The buffer is too small to call the virtual machine");
        }

        // Overwrite everything after the new instructions and replace them
        // With 0x90(NOP) to remove any original instructions
        const auto size_remaining = instruction_block.Size() - instruction_block.CursorPos();
        std::memset(instruction_block.InnerPtr().get() + instruction_block.CursorPos(), '\x90', size_remaining);

        // Write the patched buffer back to the original location
        const auto native_overwrite_res = process_context.pe_file->WriteToRegion(pair.first, instruction_block);
        if(native_overwrite_res.isErr()) {
            Panic("Could not patch the original native code");
        }

        // Once this is all done, the patched function should look like this
        // Push 0xdeadbeef // Encoded vip location
        // Call vm // Relative offset to the virtual machione
    }

    return 0;
}

int main([[maybe_unused]]int argc, [[maybe_unused]]char** argv)
{
    // MIGHT NOT RETURN - prob make this lowercase or more descriptive
    const auto cmd_args = InitAndParseCmdArgs(argc, argv);

    // Get the input file path from the arg parser and make sure the file is valid
    const auto file_path = cmd_args.get<std::string>("--input");
    const auto path_handle = ValidateFile(file_path)
                                .expect("The given file is not valid");

    // Get the virtual machine path from the arg parser and load it in memory
    const auto vm_path = cmd_args.get<std::string>("--vm");
    const auto virtual_machine_res = LoadVirtualMachine(vm_path);
    if(!virtual_machine_res) {
        Panic("The path for the virtual machine is invalid");
    }

    const auto virtual_machine = virtual_machine_res.value();

    // Parse the exe file to begin the translation process
    // The imports are not loaded right now because the API hollowing is not yet available
    auto pe_file_res = PeFile::Load(path_handle, PeFile::LoadOption::LAZY_LOAD);
    if(pe_file_res.isErr()) {
        spdlog::critical("Failed to load the PE file: MSG-> {}", pe_file_res.unwrapErr());
        return -1;
    }

    auto pe_file = pe_file_res.unwrap();

    // Create the first region which will hold the virtual machine
    // Write the vm to it
    const auto vm_region_size = 0x1000; // it's 0x1000 because of the alignment - is it always this? hard coded is not usually good
    const auto ign1_region_res = pe_file->AddSection(".Ign1", vm_region_size);
    if(!ign1_region_res) {
        Panic("Failed to add the first region for the virtual machine");
    }

    // The addition of the section was a section, get the returned handle
    const auto ign1_region = ign1_region_res.value();

    // Write the vm binary to the '.Ign1' region
    pe_file->WriteToRegion(ign1_region.VirtualAddress, virtual_machine)
            .expect("The writing of the virtual machine failed");

    const auto vcode_region_size = 0x1000; // it's 0x1000 because of the alignment

    // Create the second region which will hold all of the translated code
    const auto ign2_region_res = pe_file->AddSection(".Ign2", vcode_region_size);
    if(!ign2_region_res) {
        Panic("Failed to add the second region for the virtualized code");
    }
    const auto ign2_region = ign2_region_res.value();

    // Once the file was successfully loaded, we manage the specified block for translation
    const auto regions = cmd_args.get<std::vector<std::uint64_t>>("--block");
    const auto region_pairs = ValidateRegions(regions)
                                .expect("Failed to pair the regions");

    Main::BeginProcessContext process_context(
        pe_file,
        ign1_region,
        ign2_region,
        region_pairs
    );

    return BeginTranslationProcess(process_context);
}
