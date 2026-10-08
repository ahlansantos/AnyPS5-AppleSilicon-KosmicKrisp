#ifndef ELFPATCHER_MACOSMACHOPATCHER_HPP
#define ELFPATCHER_MACOSMACHOPATCHER_HPP

#include <elfpatcher/general/IElfPatcher.hpp>

namespace Elfpatcher {
namespace Macos {

class MacosMachoPatcher : public IElfPatcher {
public:
    std::vector<std::uint8_t> Patch(
        const std::vector<std::uint8_t>& sourceElf,
        const std::vector<Domain::ProgramHeader>& originalHeaders,
        const Domain::SysVDynamicSection& dynamicSection,
        std::uint64_t originalPltGotVaddr,
        const std::string& runPath,
        bool lazyBinding,
        bool dependencyDiagnostics,
        const std::vector<Codegen::TrampolineSite>& trampolines
    ) override;
};

}
}

#endif
