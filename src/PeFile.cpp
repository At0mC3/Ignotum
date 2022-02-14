#include <PeFile.hpp>

#include <iostream>
#include <bit>

/**
 * @brief 
 * Converts the given relative virtual address to a raw address that
 * can be directly accessed in the file buffer
 * 
 * @param rva Relative virtual address to be converted
 * @return std::uint32_t The raw address. IF 0 then it was not found
 */
std::uint32_t PeFile::RvaToRaw(const std::uint32_t& rva)
{
    // Iterate over all of the sections and check if the given value is
    // within the range of the section. If yes, convert it to a memory region address
    for(const auto& section_it : m_sections_map)
    {
        const auto section = section_it.second;

        if(rva >= section.VirtualAddress && rva <= section.VirtualAddress + section.Misc.VirtualSize)
            return section.PointerToRawData + (rva - section.VirtualAddress);
    }

    return 0;
}

/**
 * @brief 
 * Gets the entry import directory from the right structure
 * @return std::optional<Win32::IMAGE_DATA_DIRECTORY> 
 * The requested structure or nullopt
 */
std::optional<Win32::IMAGE_DATA_DIRECTORY> PeFile::GetImportDirectory()
{
    if(m_arch == Win32::Architecture::AMD64)
        return nt_headers64.OptionalHeader64.DataDirectory[Win32::Definitions::IMAGE_DIRECTORY_ENTRY_IMPORT];
    else if(m_arch == Win32::Architecture::I386)
        return nt_headers32.OptionalHeader32.DataDirectory[Win32::Definitions::IMAGE_DIRECTORY_ENTRY_IMPORT];
    
    // Else we just return a nullopt
    return {};
}

/**
 * @brief 
 * Goes over all of the functions imported from a library and maps them to a easily accessible structure
 * @param dll_name Name of the imported library
 * @param first_thunk_rva The rva to the first IMAGE_THUNK_DATA structure
 * @return true The functions for the library were loaded and mapped in the structure
 * @return false The functions could not be loaded and mapped because of invalid data
 */
bool PeFile::MapImports(const std::string& dll_name, const std::uint32_t& first_thunk_rva)
{
    // Save the position, so we can roll back to it after being done
    const auto saved_position = m_file_handle.tellg();

    // We convert the rva to raw, to access it in the buffer
    const auto first_thunk_raw = RvaToRaw(first_thunk_rva);
    // We failed to find it, return
    if(first_thunk_raw == 0)
        return false;

    // Seek to the first thunk
    m_file_handle.seekg(first_thunk_raw);

    std::vector<ImportedFunction> imported_functions;
    Win32::IMAGE_THUNK_DATA thunk_data{ 0 };
    for(std::size_t i = 0;; ++i)
    {
        m_file_handle.read(std::bit_cast<char *>(&thunk_data), sizeof(Win32::IMAGE_THUNK_DATA));
    
        const auto import_by_name_rva = std::bit_cast<std::uintptr_t>(thunk_data.u1.AddressOfData);
        const auto import_by_name_raw = RvaToRaw(import_by_name_rva);
        if(import_by_name_raw == 0)
            break;

        // Save the position to know where we were before jumping to the string rva
        const auto descriptor_position = m_file_handle.tellg();

        // Seek to the raw address of the name import
        m_file_handle.seekg(import_by_name_raw);

        Win32::IMAGE_IMPORT_BY_NAME name_import{ 0 };
        // Read the hint flag
        m_file_handle.read(std::bit_cast<char *>(&name_import), sizeof(Win32::IMAGE_IMPORT_BY_NAME::Hint));
        
        std::string import_name;
        // Read the name of the import
        std::getline(m_file_handle, import_name, '\0');
        if(import_name[0] == 'l')
            break;
        
        // Seek to the raw address of the name import
        m_file_handle.seekg(descriptor_position);

        imported_functions.emplace_back(ImportedFunction{
            import_name,
            0
        });
    }
    m_imported_functions_map[dll_name] = imported_functions;

    // Roll back to the original position
    m_file_handle.seekg(saved_position);
    return true;
}

/**
 * @brief 
 * Goes over all of the imported libraries and loads and maps them into memory
 * @return true All of the libraries and their functions are mapped
 * @return false Could not be fully mapped due to invalid data
 */
bool PeFile::LoadImports()
{
    const auto import_directory = GetImportDirectory();
    if(import_directory == std::nullopt)
        return false;

    const auto import_descriptor_rva = (*import_directory).VirtualAddress;

    // No imports were found. This does not mean that the import parsing failed
    if(import_descriptor_rva == 0)
        return true;

    // We need to get the raw address to find it in the local buffer
    const auto import_descriptor_raw = RvaToRaw(import_descriptor_rva);
    // It failed to find the raw address in the buffer, we stop the import parsing
    if(import_descriptor_raw == 0)
        return false;
    
    // Save the position, so we can roll back to it after being done
    const auto saved_position = m_file_handle.tellg();

    m_file_handle.seekg(import_descriptor_raw);

    // This is the table place in an array
    Win32::IMAGE_IMPORT_DESCRIPTOR import_descriptor { 0 };
    for(std::size_t i = 0;; ++i)
    {
        // We increment to go over every structures
        m_file_handle.read(std::bit_cast<char *>(&import_descriptor), sizeof(Win32::IMAGE_IMPORT_DESCRIPTOR));
        
        // When we have reached the end, the name rva will be nulL
        const auto import_name_rva = import_descriptor.Name;
        // The struct is empty, we break
        if(import_name_rva == 0)
            break;

        // We convert the rva for the dll name to raw
        const auto import_name_raw = RvaToRaw(import_name_rva);
        if(import_name_raw == 0)
            break;

        // Save the position to know where we were before jumping to the string rva
        const auto descriptor_position = m_file_handle.tellg();

        // Peek to where the string is stored to copy it
        m_file_handle.seekg(import_name_raw);

        // We copy the name
        std::string dll_import_name;
        std::getline(m_file_handle, dll_import_name, '\0');

        // Roll back to the array of import descriptors
        m_file_handle.seekg(descriptor_position);

        bool map_success = MapImports(dll_import_name, import_descriptor.OriginalFirstThunk);
        if(!map_success)
            return false;
    }

    // Roll back to the original position before this function was called
    m_file_handle.seekg(saved_position);
    return true;
}

/**
 * @brief 
 * Goes over all of the sections after the NT_HEADERS and maps them in memory
 * @return true All of the sections were mapped successfully
 * @return false Bad data cause the mapping to fail
 */
void PeFile::LoadSections()
{
    std::uint16_t section_count = 0;
    if(m_arch == Win32::Architecture::AMD64)
        section_count = nt_headers64.FileHeader.NumberOfSections;
    else if(m_arch == Win32::Architecture::I386)
        section_count = nt_headers32.FileHeader.NumberOfSections;

    for(std::uint16_t i = 0; i < section_count; ++i)
    {
        Win32::IMAGE_SECTION_HEADER section{ 0 };
        m_file_handle.read(std::bit_cast<char *>(&section), sizeof(Win32::IMAGE_SECTION_HEADER));

        // Verify if any data is invalid
        if(section.PointerToRawData == 0)
            continue;
        if(section.PointerToRawData == 0)
            continue;

        char* name = std::bit_cast<char*>(&section.Name);

        if(m_sections_map.find(name) == m_sections_map.end())
            m_sections_map[name] = section;
    }
}

/**
 * @brief 
 * Load the right header depending on the architecture of the file
 * @return true The architecture of the file is supported and the struct was mapped
 * @return false The architecture is not supported and the mapping failed
 */
bool PeFile::LoadNtHeaders()
{
    switch(m_arch)
    {
        case Win32::Architecture::AMD64:
            m_file_handle.read(std::bit_cast<char *>(&nt_headers64), sizeof(nt_headers64));
            break;
        case Win32::Architecture::I386:
            m_file_handle.read(std::bit_cast<char *>(&nt_headers32), sizeof(nt_headers32));
            break;
        case Win32::Architecture::NOT_SUPPORTED:
            return false;
    }

    return true;
}

/**
 * @brief 
 * Reads the first bytes of the NT_HEADERS to find the architecture that the file runs on
 * @param pe The structure holding the handles of the file
 * @return Win32::Architecture The architecture that the file runs on
 */
Win32::Architecture PeFile::FindArchitecture(PeFile &pe)
{
    // Save the position, so we can roll back to it after being done
    const auto saved_position = pe.m_file_handle.tellg();
    pe.m_file_handle.seekg(sizeof(std::uint32_t), std::ios::cur);

    // Create a temporary struct to find the architecture
    std::uint32_t nt_machine = 0;
    pe.m_file_handle.read(std::bit_cast<char *>(&nt_machine), sizeof(std::uint32_t));

    // Roll back
    pe.m_file_handle.seekg(saved_position);

    return static_cast<Win32::Architecture>(nt_machine);
}

[[maybe_unused]] std::uint32_t PeFile::GetEntryPoint() const
{
    switch(m_arch)
    {
        case Win32::Architecture::AMD64:
            return nt_headers64.OptionalHeader64.AddressOfEntryPoint;
        case Win32::Architecture::I386:
            return nt_headers32.OptionalHeader32.AddressOfEntryPoint;
        default:
            return 0;
    }
}

Result<MappedMemory, const char*> PeFile::LoadRegion(const std::uint32_t& rva, const std::size_t& region_size)
{
    const auto raw_address = RvaToRaw(rva);
    if(raw_address == 0)
        return Err("The provided rva was not found in the sections");
    
    // Save the old position, so it can be rolled back at the end
    const auto previous_cur_position = m_file_handle.tellg();

    // Seek to where the requested region is
    m_file_handle.seekg(raw_address);

    auto memory_buffer = MappedMemory::Allocate(region_size).expect("Failed to allocate");
//    std::shared_ptr<std::byte[]> buffer(new std::byte[region_size]);

    // Read the file in the allocated buffer
    m_file_handle.read(std::bit_cast<char *>(memory_buffer.InnerPtr().get()), region_size);

    // Roll back the region
    m_file_handle.seekg(previous_cur_position);
    return Ok(memory_buffer);
}

/**
 * @brief 
 * Loads the file at the specified path. The file is not fully loaded in memory but rather
 * mapped so it can allow huge files to be used without eating the memory
 * @param path The path of the file to be parsed and mapped
 * @param load_option 
 * How the file will be loaded. If the lazy option is selected, the imported functions won't be loaded in memory.
 * This can save quite a lot of memory and execution time depending on the file imports size
 * @return std::optional<PeFile> If the process succeeded, the file will be returned. Otherwise nullopt will be returned to signal a invalid file
 */
Result<std::shared_ptr<PeFile>, const char*> PeFile::Load(const std::filesystem::path& path, const LoadOption& load_option)
{
    const auto file_size = std::filesystem::file_size(path);
    if(file_size < sizeof(Win32::IMAGE_DOS_HEADER))
        return Err("File size invalid");
    
    // Create the struct and create the handle to the file using the open function.
    auto pe = std::make_shared<PeFile>();
    pe->m_load_option = load_option;

    // Open the file and verify if the handle is valid
    pe->m_file_handle.open(path, std::ios::in | std::ios::binary);
    if(!pe->m_file_handle.is_open())
        return Err("Could not open the file");

    // Seek to the end of IMAGE_DOS_HEADER and remove 4 byte to get the 32bit e_lfanew
    pe->m_file_handle.seekg(sizeof(Win32::IMAGE_DOS_HEADER) - 4);

    // Init the variable and read into it
    std::int32_t e_lfanew = 0;
    pe->m_file_handle.read(std::bit_cast<char*>(&e_lfanew), sizeof(std::int32_t));
    if(e_lfanew < 0)
        return Err("File size invalid");
    
    // e_lfanew points to somewhere in the file and the file needs to match the size where that would be
    if(file_size < e_lfanew)
        return Err("File size invalid");
    
    // Seek to the end of IMAGE_DOS_HEADER and remove 4 byte to get the 32bit e_lfanew
    pe->m_file_handle.seekg(e_lfanew);

    // The cursor is now in the nt headers section of the file

    // A check is needed to see if the smallest version of nt headers can fit in the remaining space
    // The 32 bit and 64 bit version are identical except the entry point field is either 32 or 64 bits
    if(file_size - e_lfanew < sizeof(std::uint32_t) * 2)
        return Err("File size invalid");
    
    pe->m_arch = FindArchitecture(*pe);
    
    auto nt_section_size = 0;
    switch(pe->m_arch)
    {
        case Win32::Architecture::AMD64:
        {
            if(file_size - e_lfanew < sizeof(Win32::IMAGE_NT_HEADERS64))
                return Err("File size invalid");
            nt_section_size = sizeof(Win32::IMAGE_NT_HEADERS64);
            break;
        }
        case Win32::Architecture::I386:
        {
            if(file_size - e_lfanew < sizeof(Win32::IMAGE_NT_HEADERS32))
                return Err("File size invalid");
            nt_section_size = sizeof(Win32::IMAGE_NT_HEADERS32);
            break;
        }
        case Win32::Architecture::NOT_SUPPORTED:
            break;
    };

    if(file_size - e_lfanew < nt_section_size)
        return Err("File size invalid");

    // Load the NT_HEADERS in memory for easing parsing
    const auto traverse_result = pe->LoadNtHeaders();
    if(!traverse_result)
        return Err("Failed to load the Nt Headers");
    
    // Check if there's enough space for at least 1 section before loading them
    if(file_size - e_lfanew < sizeof(nt_section_size) + sizeof(Win32::IMAGE_SECTION_HEADER))
        return Err("File size invalid");

    // Load the sections in memory
    pe->LoadSections();

    // If the lazy option was not selected, load all the imported functions
    if(pe->m_load_option != LoadOption::LAZY_LOAD)
    {
        if(!pe->LoadImports())
            return Err("Failed to load the imports");
    }

    return Ok(pe);
}