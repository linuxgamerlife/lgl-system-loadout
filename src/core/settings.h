#pragma once

#include <glibmm/keyfile.h>

#include <string>

namespace loadout {

// Preferences in $XDG_CONFIG_HOME/lgl-system-loadout/settings.ini.
// A missing, unreadable or malformed file falls back to the defaults; a failed
// save is not fatal and never interrupts the user.
class Settings final {
public:
    Settings();
    explicit Settings(std::string path);

    bool systemTitleBar() const;
    void setSystemTitleBar(bool enabled);

private:
    void save();

    std::string m_path;
    Glib::RefPtr<Glib::KeyFile> m_file;
};

} // namespace loadout
