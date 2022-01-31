#include <iostream>
#include <string_view>
#include <filesystem>
#include <optional>
#include <Zydis/Zydis.h>

#include <PeFile.hpp>

// Disable the exceptions
#define TOML_EXCEPTIONS 0
#include <toml++/toml.h>

/// Checks whether it's a file and it exists
std::optional<std::filesystem::path> ValidateFile(const std::string_view& file_path)
{
    std::filesystem::path p{file_path};

    if(std::filesystem::exists(p) != true)
        return {};
    if(std::filesystem::is_regular_file(p) != true)
        return {};

    return p;
}

int main([[maybe_unused]]int argc, [[maybe_unused]]char** argv) 
{
    if(argc < 2) 
    {
        std::puts("Not enough arguments were given");
        return 0;
    }

    const std::string_view file_path{argv[1]};
    const auto path_handle = ValidateFile(file_path);
    
    if(path_handle == std::nullopt)
    {
        std::puts("The file does not exist or the file is not valid");
        return 0;
    }

    auto pe_file = PeFile::Load(*path_handle, PeFile::LoadOption::FULL_LOAD);
    if(pe_file == std::nullopt)
    {
        std::puts("The file provided is not valid.\n");
        return 0;
    }

    return 0;
}