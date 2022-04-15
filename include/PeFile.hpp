#ifndef INCLUDE_PEFILE
#define INCLUDE_PEFILE

#include <optional>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <string_view>

#include <Win32.hpp>
#include <result.h>
#include <MappedMemory.hpp>

class PeFile
{
public:
    struct ImportedFunction { std::string name; std::uint32_t rva; };
    enum class LoadOption{ LAZY_LOAD, FULL_LOAD };
private:
    std::fstream m_file_handle;
    LoadOption m_load_option{ LoadOption::LAZY_LOAD };
    Win32::Architecture m_arch{ Win32::Architecture::NOT_SUPPORTED };
    std::uintmax_t m_nt_headers_offset{ 0 };
    Win32::IMAGE_NT_HEADERS32 nt_headers32{ 0 };
    Win32::IMAGE_NT_HEADERS64 nt_headers64{ 0 };
    Win32::IMAGE_NT_HEADERS_HYBRID* nt_headers_hybrid{ nullptr };
    std::unordered_map<std::string, Win32::IMAGE_SECTION_HEADER> m_sections_map;
    std::unordered_map<std::string, std::vector<ImportedFunction>> m_imported_functions_map;
private:
    [[nodiscard]] static Win32::Architecture FindArchitecture(PeFile& pe);
private:
    [[nodiscard]] std::uint32_t RvaToRaw(const std::uint32_t& rva) const;
    [[nodiscard]] bool MapImports(const std::string& dll_name, const std::uint32_t& first_thunk_rva);
    [[nodiscard]] std::optional<Win32::IMAGE_DATA_DIRECTORY> GetImportDirectory() const;
    [[nodiscard]] bool LoadImports();
    [[nodiscard]] bool LoadSections();
    [[nodiscard]] bool LoadNtHeaders();
public:
    [[nodiscard]] std::uint32_t GetEntryPoint() const;
    [[nodiscard]] Result<bool, const char*> WriteToRegion(const std::uint32_t& rva, const MappedMemory& mapped_memory);
    [[nodiscard]] Result<MappedMemory, const char*> LoadRegion(const std::uint32_t& rva, const std::size_t& region_size);
    [[nodiscard]] Result<Win32::IMAGE_SECTION_HEADER, const char*> AddSection(const std::string_view& section_name);
    static Result<std::shared_ptr<PeFile>, const char*> Load(const std::filesystem::path& p, const LoadOption& load_option);
};

#endif