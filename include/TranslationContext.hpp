#ifndef __TRANSLATIONCONTEXT_H__
#define __TRANSLATIONCONTEXT_H__

#include <cstdint>

namespace Translation
{
    struct Context
    {
        // Relative virtual address of where the native instruction start at
        std::uintmax_t original_block_rva;
        std::uintmax_t original_block_size;

        // Relative virtual address at the top of the virtual machine
        // This is usually the entry for the vm
        std::uintmax_t vm_block_rva;
        std::uintmax_t vm_block_size;

        // Relative virtual address which indicates the start of
        // where these instructions Will be stored
        std::uintmax_t vcode_block_rva;
        std::uintmax_t vcode_block_size;

        Context(
            std::uintmax_t _original_block_rva, 
            std::uintmax_t _original_block_size,
            std::uintmax_t _vm_block_rva,
            std::uintmax_t _vm_block_size,
            std::uintmax_t _vcode_block_rva,
            std::uintmax_t _vcode_block_size
        ) : 
        original_block_rva(_original_block_rva), original_block_size(_original_block_size),
        vm_block_rva(_vm_block_rva), vm_block_size(_vm_block_size),
        vcode_block_rva(_vcode_block_rva), vcode_block_size(_vcode_block_size) {}

    };
}

#endif // __TRANSLATIONCONTEXT_H__