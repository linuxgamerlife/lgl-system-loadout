#include "core/desktop.h"

namespace loadout {

std::string desktopFileContents(const DesktopEntry &entry)
{
    std::string text;
    text += "[Desktop Entry]\n";
    text += "Type=Application\n";
    text += "Version=1.0\n";
    text += "Name=" + entry.name + "\n";
    if (!entry.comment.empty())
        text += "Comment=" + entry.comment + "\n";
    text += "Exec=" + entry.executable + "\n";
    text += "Icon=" + entry.id + "\n";
    text += "Terminal=false\n";
    if (!entry.categories.empty())
        text += "Categories=" + entry.categories + "\n";
    text += "StartupNotify=true\n";
    return text;
}

} // namespace loadout
