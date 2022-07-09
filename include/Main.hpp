#ifndef __MAIN_H__
#define __MAIN_H__

#include <memory>

#include <PeFile.hpp>

namespace Main
{
    struct BeginProcessContext
    {
        std::shared_ptr<PeFile> pe_file; // File which is currently being worked on
        Win32::IMAGE_SECTION_HEADER vm_section; // Section where the virtual machine is located. It is always stored at the top
        Win32::IMAGE_SECTION_HEADER vcode_section; // Section of where the virtual code will be stored
        // All the regions stored in pairs.
        // The first item is the rva of where the region is starting
        // The second item is the size of the region to be virtualized
        std::vector<std::pair<std::size_t, std::size_t>> region_pairs;

        explicit BeginProcessContext(
            std::shared_ptr<PeFile> _pe_file,
            Win32::IMAGE_SECTION_HEADER _vm_section,
            Win32::IMAGE_SECTION_HEADER _vcode_section,
            std::vector<std::pair<std::size_t, std::size_t>> _region_pairs
        ) : 
        pe_file(_pe_file), vm_section(_vm_section), vcode_section(_vcode_section),
        region_pairs(_region_pairs) 
        {

        }
    };
}

#endif // __MAIN_H__