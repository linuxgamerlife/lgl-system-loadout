#pragma once

#include "core/selection.h"
#include "core/system.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace loadout {

// One privileged operation. command[0] is the program; the rest are arguments.
// An empty command is a no-op marker that counts as success.
struct InstallStep final {
    std::string id;
    std::string description;
    std::vector<std::string> command;
    bool optional{false};
    std::vector<std::string> alreadyInstalledCheck;
    std::set<int> allowedExitCodes;
    // Runs in the GUI process as the normal user, never through the privileged
    // helper. Used for AppImages in ~/Applications.
    bool local{false};
    // For local multi-step installs: skip this step unless the named step
    // succeeded. Used to publish an AppImage only after a good download.
    std::string dependsOn;
    // For a local download: verify this file against expectedSha256 after the
    // command succeeds.
    std::string verifyPath;
    std::string expectedSha256;

    InstallStep() = default;
    InstallStep(std::string stepId, std::string stepDescription,
                std::vector<std::string> stepCommand, const bool isOptional = false,
                std::vector<std::string> check = {}, std::set<int> exitCodes = {},
                const bool isLocal = false)
        : id(std::move(stepId))
        , description(std::move(stepDescription))
        , command(std::move(stepCommand))
        , optional(isOptional)
        , alreadyInstalledCheck(std::move(check))
        , allowedExitCodes(std::move(exitCodes))
        , local(isLocal)
    {
    }
};

// How to find out whether one catalogue item is installed. Every argv in `any`
// is tried; the item counts as installed if one exits 0. `negate` inverts the
// result, for settings that are correct when a command fails (a disabled unit).
struct Check final {
    std::vector<std::vector<std::string>> any;
    std::string outputContains;
    bool negate{false};
};

std::vector<InstallStep> buildPlan(const Selection &selection, const SystemInfo &system,
                                   const std::string &tmpDir);

int estimateDiskMb(const Selection &selection);

Check checkFor(const std::string &key, const SystemInfo &system);

bool needsFlatpak(const Selection &selection);

} // namespace loadout
