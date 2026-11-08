#pragma once

#include <string>

namespace loadout {

// A user-scope desktop entry for an AppImage installed into ~/Applications.
// The desktop file and its icon live under ~/.local/share, so no root access
// is needed and the menu entry follows the user's home directory.
struct DesktopEntry final {
    std::string id;             // also the desktop file and icon name
    std::string name;
    std::string comment;
    std::string executable;     // absolute path to the AppImage
    std::string iconResource;   // GResource path of the bundled PNG
    std::string home;           // user's home directory
    std::string categories{"Utility;"};
};

std::string desktopFileContents(const DesktopEntry &entry);

} // namespace loadout
