#ifndef WIN32_DEF_H
#define WIN32_DEF_H

#include <cstdint>

namespace Win32
{
    enum class Arch : std::uint8_t
    {
        X86,
        X86_64,
        UNKNOWN
    };

    namespace Definitions
    {
        constexpr std::uint16_t DOS_MAGIC = 0x5A4D;
        constexpr std::uint32_t IMAGE_NUMBEROF_DIRECTORY_ENTRIES = 16;
        constexpr std::uint32_t IMAGE_SIZEOF_SHORT_NAME = 8;
        constexpr std::uint32_t PE_HEADER_MAGIC = 0x4550;

        // Export Directory
        constexpr std::uint32_t IMAGE_DIRECTORY_ENTRY_EXPORT = 0;
        // Import Directory
        constexpr std::uint32_t IMAGE_DIRECTORY_ENTRY_IMPORT = 1;
        // Resource Directory
        constexpr std::uint32_t IMAGE_DIRECTORY_ENTRY_RESOURCE = 2;
        // Exception Directory
        constexpr std::uint32_t IMAGE_DIRECTORY_ENTRY_EXCEPTION = 3;
        // Security Directory
        constexpr std::uint32_t IMAGE_DIRECTORY_ENTRY_SECURITY = 4;
        // Base Relocation Table
        constexpr std::uint32_t IMAGE_DIRECTORY_ENTRY_BASERELOC = 5;
        // Debug Directory
        constexpr std::uint32_t IMAGE_DIRECTORY_ENTRY_DEBUG = 6;
        // Description String
        constexpr std::uint32_t IMAGE_DIRECTORY_ENTRY_COPYRIGHT = 7;
        // Machine Value (MIPS GP)
        constexpr std::uint32_t IMAGE_DIRECTORY_ENTRY_GLOBALPTR = 8;
        // TLS Directory
        constexpr std::uint32_t IMAGE_DIRECTORY_ENTRY_TLS = 9;
        // Load Configuration Director
        constexpr std::uint32_t IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG = 10;
    }

    enum class Architecture : std::uint16_t
    {
        // For 32bit INTEL
        I386 = 0x014c,
        // For 64bit INTEL
        AMD64 = 0x8664,
        NOT_SUPPORTED
    };

    typedef std::uint16_t WORD;
    typedef std::uint32_t DWORD;
    typedef std::uint64_t ULONGLONG;
    typedef std::uint8_t BYTE;
    typedef std::int32_t LONG;

    struct IMAGE_IMPORT_DESCRIPTOR {
        DWORD    OriginalFirstThunk;
        DWORD    TimeDateStamp;
        DWORD     ForwarderChain;
        DWORD     Name; // RVA for the name
        DWORD      FirstThunk;
    };

    struct IMAGE_IMPORT_BY_NAME
    {
        WORD Hint;
        BYTE Name[1];
    };

    struct IMAGE_THUNK_DATA {
        union {
            DWORD* Function;             // address of imported function
            DWORD  Ordinal;              // ordinal value of function
            IMAGE_IMPORT_BY_NAME* AddressOfData;        // RVA of imported name
            DWORD ForwarderStringl;              // RVA to forwarder string
        } u1;
    };

    struct IMAGE_DOS_HEADER {      // DOS .EXE header
        WORD   e_magic;                     // Magic number
        WORD   e_cblp;                      // Bytes on last page of file
        WORD   e_cp;                        // Pages in file
        WORD   e_crlc;                      // Relocations
        WORD   e_cparhdr;                   // Size of header in paragraphs
        WORD   e_minalloc;                  // Minimum extra paragraphs needed
        WORD   e_maxalloc;                  // Maximum extra paragraphs needed
        WORD   e_ss;                        // Initial (relative) SS value
        WORD   e_sp;                        // Initial SP value
        WORD   e_csum;                      // Checksum
        WORD   e_ip;                        // Initial IP value
        WORD   e_cs;                        // Initial (relative) CS value
        WORD   e_lfarlc;                    // File address of relocation table
        WORD   e_ovno;                      // Overlay number
        WORD   e_res[4];                    // Reserved words
        WORD   e_oemid;                     // OEM identifier (for e_oeminfo)
        WORD   e_oeminfo;                   // OEM information; e_oemid specific
        WORD   e_res2[10];                  // Reserved words
        LONG   e_lfanew;                    // File address of new exe header
    };

    struct IMAGE_DOS_STUB {             // DOS .EXE stub
        char data[0xAF];
    };

    struct IMAGE_SECTION_HEADER {
        BYTE  Name[Definitions::IMAGE_SIZEOF_SHORT_NAME];
        union {
            DWORD PhysicalAddress;
            DWORD VirtualSize;
        } Misc;
        DWORD VirtualAddress;
        DWORD SizeOfRawData;
        DWORD PointerToRawData;
        DWORD PointerToRelocations;
        DWORD PointerToLinenumbers;
        WORD  NumberOfRelocations;
        WORD  NumberOfLinenumbers;
        DWORD Characteristics;
    };

    struct IMAGE_FILE_HEADER {
        WORD  Machine;
        WORD  NumberOfSections;
        DWORD TimeDateStamp;      
        DWORD PointerToSymbolTable;
        DWORD NumberOfSymbols;
        WORD  SizeOfOptionalHeader;
        WORD  Characteristics;
    };

    struct IMAGE_DATA_DIRECTORY {
        DWORD VirtualAddress;
        DWORD Size;
    };

    struct IMAGE_OPTIONAL_HEADER64 {
        WORD        Magic;
        BYTE        MajorLinkerVersion;
        BYTE        MinorLinkerVersion;
        DWORD       SizeOfCode;
        DWORD       SizeOfInitializedData;
        DWORD       SizeOfUninitializedData;
        DWORD       AddressOfEntryPoint;
        DWORD       BaseOfCode;
        ULONGLONG   ImageBase;
        DWORD       SectionAlignment;
        DWORD       FileAlignment;
        WORD        MajorOperatingSystemVersion;
        WORD        MinorOperatingSystemVersion;
        WORD        MajorImageVersion;
        WORD        MinorImageVersion;
        WORD        MajorSubsystemVersion;
        WORD        MinorSubsystemVersion;
        DWORD       Win32VersionValue;
        DWORD       SizeOfImage;
        DWORD       SizeOfHeaders;
        DWORD       CheckSum;
        WORD        Subsystem;
        WORD        DllCharacteristics;
        ULONGLONG   SizeOfStackReserve;
        ULONGLONG   SizeOfStackCommit;
        ULONGLONG   SizeOfHeapReserve;
        ULONGLONG   SizeOfHeapCommit;
        DWORD       LoaderFlags;
        DWORD       NumberOfRvaAndSizes;
        IMAGE_DATA_DIRECTORY DataDirectory[Definitions::IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
    };

    struct IMAGE_OPTIONAL_HEADER32 {
        WORD                 Magic;
        BYTE                 MajorLinkerVersion;
        BYTE                 MinorLinkerVersion;
        DWORD                SizeOfCode;
        DWORD                SizeOfInitializedData;
        DWORD                SizeOfUninitializedData;
        DWORD                AddressOfEntryPoint;
        DWORD                BaseOfCode;
        DWORD                BaseOfData;
        DWORD                ImageBase;
        DWORD                SectionAlignment;
        DWORD                FileAlignment;
        WORD                 MajorOperatingSystemVersion;
        WORD                 MinorOperatingSystemVersion;
        WORD                 MajorImageVersion;
        WORD                 MinorImageVersion;
        WORD                 MajorSubsystemVersion;
        WORD                 MinorSubsystemVersion;
        DWORD                Win32VersionValue;
        DWORD                SizeOfImage;
        DWORD                SizeOfHeaders;
        DWORD                CheckSum;
        WORD                 Subsystem;
        WORD                 DllCharacteristics;
        DWORD                SizeOfStackReserve;
        DWORD                SizeOfStackCommit;
        DWORD                SizeOfHeapReserve;
        DWORD                SizeOfHeapCommit;
        DWORD                LoaderFlags;
        DWORD                NumberOfRvaAndSizes;
        IMAGE_DATA_DIRECTORY DataDirectory[Definitions::IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
    };

    struct IMAGE_NT_HEADERS32 {
        DWORD                 Signature;
        IMAGE_FILE_HEADER     FileHeader;
        IMAGE_OPTIONAL_HEADER32 OptionalHeader32;
    };

    struct IMAGE_NT_HEADERS64 {
        DWORD                 Signature;
        IMAGE_FILE_HEADER     FileHeader;
        IMAGE_OPTIONAL_HEADER64 OptionalHeader64;
    };

    // This struct is not part of the PE format
    // It was created to bridge the 64 and 32 bit nt_headers
    struct IMAGE_NT_HEADERS_HYBRID {
        DWORD                Signature;
        IMAGE_FILE_HEADER     FileHeader;
    };

}



#endif