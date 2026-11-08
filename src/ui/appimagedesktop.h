#pragma once

#include "core/desktop.h"

#include <string>

namespace loadout {

// Writes a user-scope desktop entry and its icon for an installed AppImage.
// Both files live under ~/.local/share, so no privileged helper is involved.
bool writeAppImageDesktopEntry(const DesktopEntry &entry, std::string &error);

} // namespace loadout
