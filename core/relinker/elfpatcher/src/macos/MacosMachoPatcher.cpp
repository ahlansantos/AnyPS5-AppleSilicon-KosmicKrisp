#include <elfpatcher/macos/MacosMachoPatcher.hpp>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <cstring>
#include <stdexcept>

namespace Elfpatcher {
namespace Macos {

std::vector<std::uint8_t> MacosMachoPatcher::Patch(
    const std::vector<std::uint8_t>& sourceElf,
    const std::vector<Domain::ProgramHeader>& originalHeaders,
    const Domain::SysVDynamicSection& dynamicSection,
    std::uint64_t originalPltGotVaddr,
    const std::string& runPath,
    bool lazyBinding,
    bool dependencyDiagnostics,
    const std::vector<Codegen::TrampolineSite>& trampolines
) {
    std::vector<std::uint8_t> out;

    // We need to calculate the size of commands
    uint32_t sizeofcmds = 0;
    uint32_t ncmds = 0;

// Collect unique dependencies and imports
    std::vector<std::string> dependencies;
    std::vector<Domain::GuestImport> imports;
    for (const auto& module : dynamicSection.GuestModules) {
        if (!module.Path.empty()) {
            dependencies.push_back(module.Path);
        }
        for (const auto& imp : module.Imports) {
            imports.push_back(imp);
        }
    }

    // 1. __PAGEZERO
    sizeofcmds += sizeof(segment_command_64);
    ncmds++;

    // 2. Original segments
    for (const auto& ph : originalHeaders) {
        if (ph.Type != 1) continue; // PT_LOAD only
        sizeofcmds += sizeof(segment_command_64);
        ncmds++;
    }

    // 3. LC_LOAD_DYLINKER
    const char* dyld_path = "/usr/lib/dyld";
    uint32_t dylinker_cmd_size = sizeof(dylinker_command) + ((strlen(dyld_path) + 1 + 7) & ~7);
    sizeofcmds += dylinker_cmd_size;
    ncmds++;

    // 4. LC_MAIN
    sizeofcmds += sizeof(entry_point_command);
    ncmds++;

    // 5. LC_LOAD_DYLIB for dependencies
    for (const auto& dep : dependencies) {
        uint32_t cmd_size = sizeof(dylib_command) + ((dep.size() + 1 + 7) & ~7);
        sizeofcmds += cmd_size;
        ncmds++;
    }

    // 6. LC_DYLD_INFO_ONLY
    sizeofcmds += sizeof(dyld_info_command);
    ncmds++;

    out.resize(sizeof(mach_header_64) + sizeofcmds);
    auto* header = reinterpret_cast<mach_header_64*>(out.data());
    header->magic = MH_MAGIC_64;
    header->cputype = CPU_TYPE_X86_64;
    header->cpusubtype = CPU_SUBTYPE_X86_64_ALL;
    header->filetype = MH_EXECUTE;
    header->ncmds = ncmds;
    header->sizeofcmds = sizeofcmds;
    header->flags = MH_NOUNDEFS | MH_DYLDLINK | MH_TWOLEVEL | MH_PIE;
    header->reserved = 0;

    uint8_t* cmd_ptr = out.data() + sizeof(mach_header_64);

    // Write __PAGEZERO
    auto* pagezero = reinterpret_cast<segment_command_64*>(cmd_ptr);
    pagezero->cmd = LC_SEGMENT_64;
    pagezero->cmdsize = sizeof(segment_command_64);
    strncpy(pagezero->segname, "__PAGEZERO", 16);
    pagezero->vmaddr = 0;
    pagezero->vmsize = 0x100000000ULL; // Typical pagezero size for 64-bit
    pagezero->fileoff = 0;
    pagezero->filesize = 0;
    pagezero->maxprot = 0;
    pagezero->initprot = 0;
    pagezero->nsects = 0;
    pagezero->flags = 0;
    cmd_ptr += pagezero->cmdsize;

    // Write segments
    int seg_idx = 0;
    for (const auto& ph : originalHeaders) {
        if (ph.Type != 1) continue;
        auto* seg = reinterpret_cast<segment_command_64*>(cmd_ptr);
        seg->cmd = LC_SEGMENT_64;
        seg->cmdsize = sizeof(segment_command_64);
        std::string name = "__SEG" + std::to_string(seg_idx++);
        strncpy(seg->segname, name.c_str(), 16);
        seg->vmaddr = ph.MappedAddress;
        seg->vmsize = ph.MemorySize;
        seg->fileoff = ph.Offset;
        seg->filesize = ph.FileSize;
        seg->maxprot = 7;
        seg->initprot = 7; // Just make everything rwx for now
        seg->nsects = 0;
        seg->flags = 0;
        cmd_ptr += seg->cmdsize;
    }

    // Write LC_LOAD_DYLINKER
    auto* dylinker = reinterpret_cast<dylinker_command*>(cmd_ptr);
    dylinker->cmd = LC_LOAD_DYLINKER;
    dylinker->cmdsize = dylinker_cmd_size;
    dylinker->name.offset = sizeof(dylinker_command);
    strcpy(reinterpret_cast<char*>(cmd_ptr + sizeof(dylinker_command)), dyld_path);
    cmd_ptr += dylinker->cmdsize;

    // Write LC_MAIN
    auto* main_cmd = reinterpret_cast<entry_point_command*>(cmd_ptr);
    main_cmd->cmd = LC_MAIN;
    main_cmd->cmdsize = sizeof(entry_point_command);
    main_cmd->entryoff = 0x1000; // Fake entry point offset
    main_cmd->stacksize = 0;
    cmd_ptr += main_cmd->cmdsize;

    // Write LC_LOAD_DYLIB
    for (const auto& dep : dependencies) {
        auto* dylib = reinterpret_cast<dylib_command*>(cmd_ptr);
        dylib->cmd = LC_LOAD_DYLIB;
        uint32_t cmd_size = sizeof(dylib_command) + ((dep.size() + 1 + 7) & ~7);
        dylib->cmdsize = cmd_size;
        dylib->dylib.name.offset = sizeof(dylib_command);
        dylib->dylib.timestamp = 2;
        dylib->dylib.current_version = 0x10000;
        dylib->dylib.compatibility_version = 0x10000;
        strcpy(reinterpret_cast<char*>(cmd_ptr + sizeof(dylib_command)), dep.c_str());
        cmd_ptr += dylib->cmdsize;
    }

    // Write LC_DYLD_INFO_ONLY
    auto* dyld_info = reinterpret_cast<dyld_info_command*>(cmd_ptr);
    dyld_info->cmd = LC_DYLD_INFO_ONLY;
    dyld_info->cmdsize = sizeof(dyld_info_command);
    cmd_ptr += dyld_info->cmdsize;

    // Generate binding info
    std::vector<uint8_t> binding_info;
    
    // For each import, generate binding opcodes
    int ordinal = 1; // 1-based index into LC_LOAD_DYLIB commands
    for (const auto& imp : imports) {
        // BIND_OPCODE_SET_DYLIB_ORDINAL_IMM + ordinal
        if (ordinal <= 15) {
            binding_info.push_back(BIND_OPCODE_SET_DYLIB_ORDINAL_IMM | ordinal);
        } else {
            binding_info.push_back(BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB);
            binding_info.push_back(ordinal); // assuming < 128 for simplicity
        }

        // BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM
        binding_info.push_back(BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM | 0);
        for (char c : imp.Name) {
            binding_info.push_back(c);
        }
        binding_info.push_back(0); // null terminator

        // BIND_OPCODE_SET_TYPE_IMM
        binding_info.push_back(BIND_OPCODE_SET_TYPE_IMM | 1); // BIND_TYPE_POINTER

        // BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
        binding_info.push_back(BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | 1); // Segment 1 (first segment after PAGEZERO)
        
        // Offset = virtual address - segment base (assuming segment base is virtual address of first segment)
        // We'll just put 0 as a placeholder for now since we're generating a basic one
        binding_info.push_back(0);

        // BIND_OPCODE_DO_BIND
        binding_info.push_back(BIND_OPCODE_DO_BIND);
    }
    binding_info.push_back(BIND_OPCODE_DONE);
    
    // Align to 8 bytes
    while (binding_info.size() % 8 != 0) {
        binding_info.push_back(0);
    }

    // Write the segments content
    // Find max offset to resize output buffer
    uint64_t max_offset = out.size();
    for (const auto& ph : originalHeaders) {
        if (ph.Type != 1) continue;
        if (ph.Offset + ph.FileSize > max_offset) {
            max_offset = ph.Offset + ph.FileSize;
        }
    }
    
    out.resize(max_offset);
    for (const auto& ph : originalHeaders) {
        if (ph.Type != 1) continue;
        if (ph.Offset + ph.FileSize <= sourceElf.size()) {
            std::memcpy(out.data() + ph.Offset, sourceElf.data() + ph.Offset, ph.FileSize);
        }
    }

    // Append binding info at the end
    uint32_t bind_off = out.size();
    out.insert(out.end(), binding_info.begin(), binding_info.end());
    
    // Update dyld_info command with binding info offset and size
    dyld_info->bind_off = bind_off;
    dyld_info->bind_size = binding_info.size();

    return out;
}

}
}
