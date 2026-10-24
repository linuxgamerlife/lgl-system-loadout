#pragma once

#include <functional>
#include <string>
#include <vector>

namespace loadout {

struct CommandResult final {
    int exitCode{-1};
    std::string output;   // merged stdout and stderr
    bool timedOut{false};
    bool cancelled{false};
};

// Runs a program with an argument vector, no shell. Blocks the calling thread,
// so callers must be off the main loop. A timeout or a true cancelled callback
// kills the child and sets the matching flag.
CommandResult runCommand(const std::vector<std::string> &argv, int timeoutMs = 5000,
                         const std::function<bool()> &cancelled = {});

} // namespace loadout
