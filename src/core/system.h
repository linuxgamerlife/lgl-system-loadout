#pragma once

#include <string>

namespace loadout {

struct SystemInfo final {
    std::string fedoraVersion{"44"};
    std::string targetUser{"root"};
    std::string targetHome;
    std::string architecture{"x86_64"};
};

// Startup probe: rpm -E %fedora and logname. Only fast local binaries, with a
// timeout, and never dnf or the network.
SystemInfo detectSystem();

bool isValidTargetUser(const std::string &name);
std::string parseFedoraVersion(const std::string &raw);

int availableDiskMb();
bool helperActionKnown();

} // namespace loadout
