#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace loadout::theme {

using Colors = std::map<std::string, std::string>;

struct Theme final {
    Colors colors;
    bool dark{false};
    std::string monoFont;
};

// The user's gtk.css followed by every local file it @imports, in load order.
std::vector<std::string> gtkCssPaths(const std::string &configDir);

// Named colours (@define-color) resolved through alias chains.
Colors gtkColors(const std::vector<std::string> &paths);

// Font family from a Pango font description such as "JetBrains Mono 10.5".
std::string fontFamily(const std::string &description);

// Relative luminance of a #rgb / #rrggbb / #rrggbbaa / rgb() / rgba() colour.
std::optional<double> luminance(const std::string &color);

bool isDark(const Colors &colors, bool preferDark);

// Stylesheet that applies the desktop monospace font to every mono class.
std::string fontStylesheet(const Theme &theme);

} // namespace loadout::theme
