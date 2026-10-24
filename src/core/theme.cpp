#include "core/theme.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <regex>
#include <set>
#include <sstream>

namespace loadout::theme {
namespace {

const std::regex commentPattern(R"(/\*[\s\S]*?\*/)");
const std::regex importPattern(
    R"re(@import\s+(?:url\(\s*)?["']?([^"')\s;]+)["']?\s*\)?\s*;)re");
const std::regex definePattern(R"(@define-color\s+([\w-]+)\s+([^;]+);)");
const std::regex cssColorPattern(R"(#[0-9a-fA-F]{3,8}|rgba?\([^()]*\))");
const std::regex fontSizePattern(R"(\s+\d+(?:\.\d+)?(?:px)?$)");
const std::regex fontStylesPattern(
    R"((?:\s+(?:thin|ultra-light|extra-light|light|semi-light|book|regular|normal|medium|semi-bold|demi-bold|bold|ultra-bold|extra-bold|heavy|black|italic|oblique|condensed|expanded))+$)",
    std::regex::icase);
const std::regex hexPattern(R"(#([0-9a-fA-F]{3}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8}))");
const std::regex numberPattern(R"([\d.]+)");

constexpr const char *fallbackMono = R"("Adwaita Mono", "JetBrains Mono", monospace)";

// Must list every class whose stylesheet rule uses the monospace font stack.
constexpr const char *monoClasses = ".mono, .wordmark, .section-label, .lo-chip, .install-log";

std::optional<std::string> readText(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return std::nullopt;
    std::ostringstream text;
    text << file.rdbuf();
    return std::regex_replace(text.str(), commentPattern, "");
}

std::string trimmed(const std::string &text)
{
    constexpr const char *whitespace = " \t\r\n\f\v";
    const auto first = text.find_first_not_of(whitespace);
    return first == std::string::npos
               ? std::string{}
               : text.substr(first, text.find_last_not_of(whitespace) - first + 1);
}

std::string quoted(const std::string &family)
{
    std::string clean;
    for (const char character : family)
        if (character != '\\' && character != '"')
            clean += character;
    return '"' + clean + '"';
}

} // namespace

std::vector<std::string> gtkCssPaths(const std::string &configDir)
{
    namespace fs = std::filesystem;
    std::vector<std::string> found;
    std::function<void(const fs::path &)> visit = [&](const fs::path &path) {
        std::error_code error;
        const std::string key = path.string();
        if (std::find(found.begin(), found.end(), key) != found.end() ||
            !fs::is_regular_file(path, error))
            return;
        found.push_back(key);
        const auto text = readText(key);
        if (!text)
            return;
        for (auto it = std::sregex_iterator(text->begin(), text->end(), importPattern);
             it != std::sregex_iterator(); ++it) {
            std::string target = (*it)[1].str();
            const bool local =
                target.find("://") == std::string::npos || target.rfind("file://", 0) == 0;
            if (!local)
                continue;
            if (target.rfind("file://", 0) == 0)
                target.erase(0, 7);
            fs::path resolved = fs::weakly_canonical(path.parent_path() / target, error);
            if (error)
                resolved = (path.parent_path() / target).lexically_normal();
            visit(resolved);
        }
    };
    visit(fs::path(configDir) / "gtk-4.0" / "gtk.css");
    return found;
}

Colors gtkColors(const std::vector<std::string> &paths)
{
    Colors raw;
    for (const auto &path : paths) {
        const auto text = readText(path);
        if (!text)
            continue;
        for (auto it = std::sregex_iterator(text->begin(), text->end(), definePattern);
             it != std::sregex_iterator(); ++it)
            raw[(*it)[1].str()] = trimmed((*it)[2].str());
    }

    std::function<std::optional<std::string>(const std::string &, std::set<std::string>)> resolve =
        [&](const std::string &value, std::set<std::string> seen) -> std::optional<std::string> {
        if (!value.empty() && value.front() == '@') {
            const auto name = value.substr(1);
            const auto found = raw.find(name);
            if (seen.count(name) || found == raw.end())
                return std::nullopt;
            seen.insert(name);
            return resolve(found->second, seen);
        }
        return std::regex_match(value, cssColorPattern) ? std::optional<std::string>(value)
                                                        : std::nullopt;
    };

    Colors colors;
    for (const auto &[name, value] : raw)
        if (const auto color = resolve(value, {}))
            colors[name] = *color;
    return colors;
}

std::string fontFamily(const std::string &description)
{
    std::string family = std::regex_replace(trimmed(description), fontSizePattern, "");
    family = trimmed(std::regex_replace(family, fontStylesPattern, ""));
    while (!family.empty() && family.back() == ',')
        family.pop_back();
    return family;
}

std::optional<double> luminance(const std::string &color)
{
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    std::smatch match;
    if (std::regex_match(color, match, hexPattern)) {
        std::string digits = match[1].str();
        if (digits.size() == 3)
            digits = {digits[0], digits[0], digits[1], digits[1], digits[2], digits[2]};
        red = std::stoi(digits.substr(0, 2), nullptr, 16);
        green = std::stoi(digits.substr(2, 2), nullptr, 16);
        blue = std::stoi(digits.substr(4, 2), nullptr, 16);
    } else {
        std::vector<double> numbers;
        for (auto it = std::sregex_iterator(color.begin(), color.end(), numberPattern);
             it != std::sregex_iterator() && numbers.size() < 3; ++it) {
            try {
                numbers.push_back(std::stod(it->str()));
            } catch (const std::exception &) {
                return std::nullopt;
            }
        }
        if (numbers.size() < 3)
            return std::nullopt;
        red = numbers[0];
        green = numbers[1];
        blue = numbers[2];
    }
    return (0.2126 * red + 0.7152 * green + 0.0722 * blue) / 255.0;
}

bool isDark(const Colors &colors, const bool preferDark)
{
    auto background = colors.find("window_bg_color");
    if (background == colors.end())
        background = colors.find("theme_bg_color");
    if (background == colors.end())
        return preferDark;
    const auto value = luminance(background->second);
    return value ? *value < 0.5 : preferDark;
}

std::string fontStylesheet(const Theme &theme)
{
    const std::string mono =
        theme.monoFont.empty() ? std::string(fallbackMono) : quoted(theme.monoFont) + ", " + fallbackMono;
    return std::string(monoClasses) + " {\n  font-family: " + mono + ";\n}\n";
}

} // namespace loadout::theme
