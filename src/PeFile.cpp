#include <PeFile.hpp>

#include <cstring>
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
std::uint32_t PeFile::RvaToRaw(const std::uint32_t rva) const
{
    // Iterate over all of the sections and check if the given value is
    // within the range of the section. If yes, convert it to a memory region address
    for(const auto& section_it : m_sections_map)
    {
        const auto section = section_it.second;

        if(
            rva >= section.VirtualAddress &&
            rva <= section.VirtualAddress + section.Misc.VirtualSize
        )
        {
            return section.PointerToRawData + (rva - section.VirtualAddress);
        }
    }

    return 0;
}

/**
 * @brief
 * Gets the entry import directory from the right structure
 * @return std::optional<Win32::IMAGE_DATA_DIRECTORY>
 * The requested structure or nullopt
 */
std::optional<Win32::IMAGE_DATA_DIRECTORY> PeFile::GetImportDirectory() const
{
    if(m_arch == Win32::Architecture::AMD64) {
        return m_nt_headers64.OptionalHeader64.DataDirectory[Win32::Constants::IMAGE_DIRECTORY_ENTRY_IMPORT];
    }
    else if(m_arch == Win32::Architecture::I386) {
        return m_nt_headers32.OptionalHeader32.DataDirectory[Win32::Constants::IMAGE_DIRECTORY_ENTRY_IMPORT];
    }

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
bool PeFile::MapImports(const std::string& dll_name, const std::uint32_t first_thunk_rva)
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
        m_file_handle.read(
            std::bit_cast<char *>(&thunk_data),
            sizeof(Win32::IMAGE_THUNK_DATA)
        );

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
    if(import_directory == std::nullopt) {
        return false;
    }

    const auto import_descriptor_rva = (*import_directory).VirtualAddress;

    // No imports were found. This does not mean that the import parsing failed
    if(import_descriptor_rva == 0) {
        return true;
    }

    // We need to get the raw address to find it in the local buffer
    const auto import_descriptor_raw = RvaToRaw(import_descriptor_rva);
    // It failed to find the raw address in the buffer, we stop the import parsing
    if(import_descriptor_raw == 0) {
        return false;
    }

    // Save the position, so we can roll back to it after being done
    const auto saved_position = m_file_handle.tellg();

    m_file_handle.seekg(import_descriptor_raw);

    // This is the table place in an array
    Win32::IMAGE_IMPORT_DESCRIPTOR import_descriptor { 0 };
    for(std::size_t i = 0;; ++i)
    {
        // We increment to go over every structures
        m_file_handle.read(
            std::bit_cast<char *>(&import_descriptor),
            sizeof(Win32::IMAGE_IMPORT_DESCRIPTOR)
        );

        // When we have reached the end, the name rva will be nulL
        const auto import_name_rva = import_descriptor.Name;
        // The struct is empty, we break
        if(import_name_rva == 0) {
            break;
        }

        // We convert the rva for the dll name to raw
        const auto import_name_raw = RvaToRaw(import_name_rva);
        if(import_name_raw == 0) {
            break;
        }

        // Save the position to know where we were before jumping to the string rva
        const auto descriptor_position = m_file_handle.tellg();

        // Peek to where the string is stored to copy it
        m_file_handle.seekg(import_name_raw);

        const auto MAX_FN_NAME_LEN = 0x1000;

        // We copy the name
        std::string dll_import_name;
        std::getline(m_file_handle, dll_import_name, '\0');

        // The names can't be greater than 4096 bytes
        if(dll_import_name.size() > MAX_FN_NAME_LEN) {
            return false;
        }

        // Roll back to the array of import descriptors
        m_file_handle.seekg(descriptor_position);

        bool map_success = MapImports(
            dll_import_name,
            import_descriptor.OriginalFirstThunk
        );

        if(!map_success) {
            return false;
        }
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
bool PeFile::LoadSections()
{
    const std::uint16_t section_count = nt_headers_hybrid->FileHeader.NumberOfSections;

    Win32::IMAGE_SECTION_HEADER section;

    std::uintmax_t nt_headers_size = [&]() -> std::uintmax_t {
        if(m_arch == Win32::Architecture::AMD64)
            return sizeof(Win32::IMAGE_NT_HEADERS64);
        else if(m_arch == Win32::Architecture::I386)
            return sizeof(Win32::IMAGE_NT_HEADERS32);
        else
            return 0;
    }();

    m_file_handle.seekg(
        m_dos_header.e_lfanew + nt_headers_size,
        m_file_handle.beg
    );

    for(std::uint16_t i = 0; i < section_count; ++i)
    {
        m_file_handle.read(
            std::bit_cast<char *>(&section),
            sizeof(Win32::IMAGE_SECTION_HEADER)
        );

        // Verify if any data is invalid
        if(section.PointerToRawData == 0) {
            continue;
        }

        char* section_name = std::bit_cast<char*>(&section.Name);

        // Settign the last byte as null to prevent overflowing the buffer
        section_name[Win32::Constants::IMAGE_SIZEOF_SHORT_NAME - 1] = '\0';

        if(m_sections_map.find(section_name) == m_sections_map.end()) {
            m_sections_map[section_name] = section;
        }
        else
        {
            std::uint64_t index = 2;
            std::string new_name = section_name;
            new_name.append("#");
            new_name.append(std::to_string(index));

            while(m_sections_map.find(new_name) != m_sections_map.end())
            {
                ++index;
                new_name = section_name;
                new_name.append("#");
                new_name.append(std::to_string(index));
            }

            m_sections_map[new_name] = section;
        }
    }

    return true;
}

/**
 * @brief
 * Reads the first bytes of the NT_HEADERS to find the architecture that the file runs on
 * @param pe The structure holding the handles of the file
 * @return Win32::Architecture The architecture that the file runs on
 */
Win32::Architecture PeFile::GetArchitecture()
{
    // Save the position, so we can roll back to it after being done
    const auto saved_position = m_file_handle.tellg();

    // Let's seek paste the signature to read the 2 bytes in charge of telling the machine type
    m_file_handle.seekg(
        sizeof(Win32::IMAGE_NT_HEADERS32::Signature),
        std::ios::cur
    );

    // Create a temporary variable to find the architecture
    std::uint16_t nt_machine = 0;
    m_file_handle.read(
        std::bit_cast<char *>(&nt_machine),
        sizeof(nt_machine)
    );

    // Roll back
    m_file_handle.seekg(saved_position);

    return static_cast<Win32::Architecture>(nt_machine);
}

std::uint32_t PeFile::GetEntryPoint() const
{
    switch(m_arch)
    {
        case Win32::Architecture::AMD64:
            return m_nt_headers64.OptionalHeader64.AddressOfEntryPoint;
        case Win32::Architecture::I386:
            return m_nt_headers32.OptionalHeader32.AddressOfEntryPoint;
        default:
            return 0;
    }
}

Result<bool, const char*>
PeFile::WriteToRegion(const std::uint32_t rva, const MappedMemory& mapped_memory)
{
    const auto raw_address = RvaToRaw(rva);
    if(raw_address == 0) {
        return Err("The provided rva was not found in the sections");
    }

    // Save the old position, so it can be rolled back at the end
    const auto previous_cur_position = m_file_handle.tellg();

    // Seek to where the requested region is
    m_file_handle.seekg(raw_address);

    // Write the memory to the specified location
    m_file_handle.write(
        std::bit_cast<const char*>(mapped_memory.InnerPtr().get()),
        mapped_memory.Size()
    );

    // Roll back the region
    m_file_handle.seekg(previous_cur_position);
    return Ok(true);
}

Result<bool, const char*>
PeFile::WriteToRegionPos(const std::uint32_t rva, const MappedMemory& mapped_memory)
{
    const auto raw_address = RvaToRaw(rva);
    if(raw_address == 0) {
        return Err("The provided rva was not found in the sections");
    }

    // Save the old position, so it can be rolled back at the end
    const auto previous_cur_position = m_file_handle.tellg();

    // Seek to where the requested region is
    m_file_handle.seekg(raw_address);

    // Write the memory to the specified location
    m_file_handle.write(
        std::bit_cast<const char*>(mapped_memory.InnerPtr().get()),
        mapped_memory.CursorPos()
    );

    // Roll back the region
    m_file_handle.seekg(previous_cur_position);
    return Ok(true);
}

Result<MappedMemory, const char*>
PeFile::LoadRegion(const std::uint32_t rva, const std::size_t region_size)
{
    const auto raw_address = RvaToRaw(rva);
    if(raw_address == 0)
        return Err("The provided rva was not found in the sections");

    // Save the old position, so it can be rolled back at the end
    const auto previous_cur_position = m_file_handle.tellg();

    // Seek to where the requested region is
    m_file_handle.seekg(raw_address);

    auto memory_buffer_res = MappedMemory::Allocate(region_size);
    if(!memory_buffer_res) {
        return Err("Allocation of the memory buffer for the region failed");
    }

    auto memory_buffer = memory_buffer_res.value();

    // Read the file in the allocated buffer
    m_file_handle.read(
        std::bit_cast<char *>(memory_buffer.InnerPtr().get()),
        region_size
    );

    // Roll back the region
    m_file_handle.seekg(previous_cur_position);
    return Ok(memory_buffer);
}

std::optional<Win32::IMAGE_SECTION_HEADER>
PeFile::AddSection(const std::string_view& section_name, const std::uint32_t section_size)
{
    const auto section_alignment = [&]() -> std::uint32_t {
        if(m_arch == Win32::Architecture::AMD64)
            return m_nt_headers64.OptionalHeader64.SectionAlignment;
        else if(m_arch == Win32::Architecture::I386)
            return m_nt_headers32.OptionalHeader32.SectionAlignment;

        return 0;
    }();

    if(section_size < section_alignment) {
        return {};
    }

    std::uintmax_t nt_headers_size = [&]() -> std::uintmax_t {
        if(m_arch == Win32::Architecture::AMD64)
            return sizeof(Win32::IMAGE_NT_HEADERS64);
        else if(m_arch == Win32::Architecture::I386)
            return sizeof(Win32::IMAGE_NT_HEADERS32);
        else
            return 0;
    }();

    // Go at the end of the nt headers
    m_file_handle.seekg(m_dos_header.e_lfanew + nt_headers_size, m_file_handle.beg);

    auto section_count = nt_headers_hybrid->FileHeader.NumberOfSections;

    // Seek to the end of the image section headers - 1. Which will get us the previous section
    m_file_handle.seekg(
        sizeof(Win32::IMAGE_SECTION_HEADER) * (section_count - 1),
        m_file_handle.cur
    );

    // Init the struct and write into it
    Win32::IMAGE_SECTION_HEADER previous_section;

    m_file_handle.read(
        std::bit_cast<char*>(&previous_section),
        sizeof(Win32::IMAGE_SECTION_HEADER)
    );

    // Go at the end of the nt headers
    m_file_handle.seekg(
        m_dos_header.e_lfanew + nt_headers_size,
        m_file_handle.beg
    );

    m_file_handle.seekg(
        sizeof(Win32::IMAGE_SECTION_HEADER) * section_count,
        std::ios_base::cur
    );

    // After reading the last section, the cursor is now at the place of the new section
    Win32::IMAGE_SECTION_HEADER new_section;

    // Set the pointer for the new section
    new_section.PointerToRawData = previous_section.PointerToRawData + previous_section.SizeOfRawData;

    // Size of the new section
    new_section.SizeOfRawData = section_size;

    // The virtual address for the new section and the virtual size
    new_section.VirtualAddress = previous_section.VirtualAddress + 0x1000;
    new_section.Misc.VirtualSize = 0x200;

    new_section.Characteristics = 0x60000020;

    std::memcpy(&new_section.Name, section_name.data(), section_name.length());

    // Write the structure in the file
    m_file_handle.write(
        std::bit_cast<char*>(&new_section),
        sizeof(Win32::IMAGE_SECTION_HEADER)
    );

    // Go at the start of the nt headers
    m_file_handle.seekg(
        m_dos_header.e_lfanew,
        std::ios_base::beg
    );

    const auto image_size = new_section.VirtualAddress - previous_section.VirtualAddress + new_section.Misc.VirtualSize;

    switch(m_arch)
    {
        case Win32::Architecture::AMD64:
            m_nt_headers64.FileHeader.NumberOfSections += 1;
            m_nt_headers64.OptionalHeader64.SizeOfImage += 0x400;
            m_nt_headers64.OptionalHeader64.SizeOfImage += image_size;
            break;
        case Win32::Architecture::I386:
            m_nt_headers32.FileHeader.NumberOfSections += 1;
            m_nt_headers32.OptionalHeader32.SizeOfImage += 0x400;
            m_nt_headers32.OptionalHeader32.SizeOfImage += image_size;
            break;
        default:
            break;
    }

    // Write the modified nt header to add the new section to the file
    m_file_handle.write(
        std::bit_cast<char*>(&m_nt_headers64),
        nt_headers_size
    );

    // Go to the end of the file
    m_file_handle.seekg(0, std::ios_base::end);

    // Expand the size of the file by writing null bytes
    for(std::uintmax_t i = 0; i < new_section.SizeOfRawData; ++i){
        m_file_handle << '\x00';
    }

    m_sections_map[std::string(section_name)] = new_section;

    return new_section;
}

bool PeFile::ParseAndVerifyDosHeader()
{
    // Load the dos header in memory for further analysis
    m_file_handle.read(
        std::bit_cast<char*>(&m_dos_header),
        sizeof(m_dos_header)
    );

    // Check if the first bytes of the file is equal to "MZ"
    if(m_dos_header.e_magic != Win32::Constants::DOS_MAGIC) {
        return false;
    }

    const auto nt_header_pos = m_dos_header.e_lfanew;

    if(nt_header_pos <= sizeof(m_dos_header) + sizeof(Win32::IMAGE_DOS_STUB)) {
        return false;
    }

    if(
        nt_header_pos >=
        m_file_size - sizeof(m_dos_header) - sizeof(Win32::IMAGE_DOS_STUB)
    )
    {
        return false;
    }

    return true;
}

bool PeFile::ParseAndVerifyNtHeaders()
{
    char nt_headers_raw[sizeof(Win32::IMAGE_NT_HEADERS64)] = { 0 };

    try {
        // Load the dos header in memory for further analysis
        m_file_handle.read(
            nt_headers_raw,
            sizeof(Win32::IMAGE_NT_HEADERS64)
        );

    } catch (const std::ios::failure& ex) {
        return false;
    }

    const auto nt_headers_64 = std::bit_cast<Win32::IMAGE_NT_HEADERS64*>(&nt_headers_raw);

    switch(nt_headers_64->FileHeader.Machine)
    {
        case Win32::Constants::IMAGE_FILE_MACHINE_AMD64:
            m_arch = Win32::Architecture::AMD64;
            std::memcpy(&m_nt_headers64, nt_headers_raw, sizeof(m_nt_headers64));
            nt_headers_hybrid = std::bit_cast<Win32::IMAGE_NT_HEADERS_HYBRID*>(&m_nt_headers64);
            break;
        case Win32::Constants::IMAGE_FILE_MACHINE_I386:
            m_arch = Win32::Architecture::I386;
            std::memcpy(&m_nt_headers32, nt_headers_raw, sizeof(m_nt_headers32));
            nt_headers_hybrid = std::bit_cast<Win32::IMAGE_NT_HEADERS_HYBRID*>(&m_nt_headers32);
            break;
        default: // If it's titanium or something else, we signal false.
            m_arch = Win32::Architecture::NOT_SUPPORTED;
            return false;
    }

    return true;
}

/**
 * @brief
 * Loads the file at the specified path.
 * The file is not fully loaded in memory but rather
 * mapped so it can allow huge files to be used without eating the memory
 * @param path The path of the file to be parsed and mapped
 * @param load_option
 * How the file will be loaded. If the lazy option is selected, the imported
 * functions won't be loaded in memory.
 * This can save quite a lot of memory and execution time depending on the
 * file imports size
 * @return std::optional<PeFile> If the process succeeded, the file will be
 * returned. Otherwise nullopt will be returned to signal a invalid file
 */
Result<std::shared_ptr<PeFile>, const char*>
PeFile::Load(const std::filesystem::path& path, const LoadOption& load_option)
{
    const auto file_size = std::filesystem::file_size(path);
    // If the file size is somehow smaller than the two first structures. Just cancel the parsing

    constexpr auto headers_size = sizeof(Win32::IMAGE_DOS_HEADER) + sizeof(Win32::IMAGE_NT_HEADERS32);
    if(file_size < headers_size)
    {
        return Err("File size invalid");
    }

    // Create the struct and create the handle to the file using the open function.
    auto pe = std::make_shared<PeFile>();
    pe->m_load_option = load_option;

    // Open the file and verify if the handle is valid
    pe->m_file_handle.open(
        path,
        std::ios::in | std::ios::out | std::ios::binary
    );

    if(!pe->m_file_handle.is_open()) {
        return Err("Could not open the file");
    }

    if(!pe->ParseAndVerifyDosHeader()) {
        return Err("The dos header is invalid");
    }

    try {
        // Seek to the absolute position of e_lfanew
        pe->m_file_handle.seekg(
            pe->m_dos_header.e_lfanew,
            pe->m_file_handle.beg
        );

    } catch (const std::ios::failure& ex) {
        return Err("Failed to reach the nt header location");
    }

    // Load the nt header in memory
    if(!pe->ParseAndVerifyNtHeaders()) {
        return Err("The nt header is invalid");
    }

    const auto nt_section_size = [&]() -> std::uint64_t {
        switch(pe->m_arch)
        {
            case Win32::Architecture::AMD64:
                return sizeof(Win32::IMAGE_NT_HEADERS64);
            case Win32::Architecture::I386:
                return sizeof(Win32::IMAGE_NT_HEADERS32);
            default:
                return 0;
        };
    }();

    if(nt_section_size == 0) {
        return Err("Invalid NT Header size");
    }

    // Check if there's enough space for at least 1 section before loading them
    if(
        file_size - sizeof(Win32::IMAGE_DOS_HEADER) <
        sizeof(nt_section_size) + sizeof(Win32::IMAGE_SECTION_HEADER)
    )
    {
        return Err("File size invalid");
    }

    // Load the sections in memory
    if(!pe->LoadSections()) {
        return Err("The parsing and loading the sections failed");
    }

    // If the lazy option was not selected, load all the imported functions
    if(pe->m_load_option != LoadOption::LAZY_LOAD)
    {
        if(!pe->LoadImports()) {
            return Err("Failed to load the imports");
        }
    }

    return Ok(pe);
}