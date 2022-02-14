#ifndef INCLUDE_PEFILE
#define INCLUDE_PEFILE

#include <optional>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

#include <Win32.hpp>
#include <result.h>
#include <MappedMemory.hpp>

class PeFile
{
public:
    struct ImportedFunction { std::string name; std::uint32_t rva; };
    enum class LoadOption{ LAZY_LOAD, FULL_LOAD };
    LoadOption m_load_option{ LoadOption::LAZY_LOAD };
private:
    std::ifstream m_file_handle;
    Win32::Architecture m_arch;
    Win32::IMAGE_NT_HEADERS32 nt_headers32{ 0 };
    Win32::IMAGE_NT_HEADERS64 nt_headers64{ 0 };
    std::unordered_map<std::string, Win32::IMAGE_SECTION_HEADER> m_sections_map;
    std::unordered_map<std::string, std::vector<ImportedFunction>> m_imported_functions_map;
private:
    static Win32::Architecture FindArchitecture(PeFile& pe);
private:
    std::uint32_t RvaToRaw(const std::uint32_t& rva);
    bool MapImports(const std::string& dll_name, const std::uint32_t& first_thunk_rva);
    std::optional<Win32::IMAGE_DATA_DIRECTORY> GetImportDirectory();
    bool LoadImports();
    void LoadSections();
    bool LoadNtHeaders();
public:
    [[maybe_unused]] [[nodiscard]] std::uint32_t GetEntryPoint() const;
    Result<MappedMemory, const char*> LoadRegion(const std::uint32_t& rva, const std::size_t& region_size);
    static Result<std::shared_ptr<PeFile>, const char*> Load(const std::filesystem::path& p, const LoadOption& load_option);
};

#endif