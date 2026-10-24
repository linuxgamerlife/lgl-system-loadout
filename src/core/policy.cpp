#include "core/policy.h"

#include <cstddef>
#include <set>

namespace loadout::policy {
namespace {

enum class Rule { Fixed, Package, PipxPackage, CoprRepo, UrlHttps, FilePathRun, Username };

struct ArgRule final {
    Rule rule{Rule::Fixed};
    const char *value{nullptr};
};

struct Op final {
    const char *program;
    std::vector<ArgRule> rules;
    bool variadic{false};   // extra arguments repeat the last rule
};

ArgRule fixed(const char *value)
{
    return {Rule::Fixed, value};
}
ArgRule package()
{
    return {Rule::Package, nullptr};
}
ArgRule pipxPackage()
{
    return {Rule::PipxPackage, nullptr};
}
ArgRule coprRepo()
{
    return {Rule::CoprRepo, nullptr};
}
ArgRule urlHttps()
{
    return {Rule::UrlHttps, nullptr};
}
ArgRule filePathRun()
{
    return {Rule::FilePathRun, nullptr};
}
ArgRule username()
{
    return {Rule::Username, nullptr};
}

// Only the repositories the catalogue actually enables. Adding a new COPR to
// the install plan also means adding it here.
const std::set<std::string> &allowedCoprRepos()
{
    static const std::set<std::string> repos{
        "atim/heroic-games-launcher",
        "bieszczaders/kernel-cachyos",
        "bieszczaders/kernel-cachyos-addons",
        "faugus/faugus-launcher",
        "imput/helium",
        "linuxgamerlife/lgl-cliamp",
        "linuxgamerlife/lgl-essentials",
        "linuxgamerlife/lgl-powerprofile-manager",
        "linuxgamerlife/lgl-toolkit",
        "linuxgamerlife/lgl-vm-curator",
        "scottames/ghostty",
    };
    return repos;
}

// Only the hosts the install plan downloads from.
const std::set<std::string> &allowedDownloadHosts()
{
    static const std::set<std::string> hosts{
        "brave-browser-rpm-release.s3.brave.com",
        "download1.rpmfusion.org",
        "packages.microsoft.com",
        "repo.vivaldi.com",
    };
    return hosts;
}

// pipx runs as the target user, so keep it to the tools the catalogue offers.
const std::set<std::string> &allowedPipxTools()
{
    static const std::set<std::string> tools{"tldr", "yt-dlp"};
    return tools;
}

const std::vector<Op> &allowList()
{
    static const std::vector<Op> list{
        {"/usr/bin/dnf", {fixed("-y"), fixed("install"), package()}, true},
        {"/usr/bin/dnf", {fixed("-y"), fixed("install"), urlHttps()}},
        {"/usr/bin/dnf", {fixed("-y"), fixed("upgrade"), fixed("--refresh")}},
        {"/usr/bin/dnf", {fixed("swap"), fixed("-y"), package(), package(),
                          fixed("--allowerasing")}},
        {"/usr/bin/dnf", {fixed("copr"), fixed("enable"), coprRepo()}},
        {"/usr/bin/dnf", {fixed("copr"), fixed("enable"), fixed("-y"), coprRepo()}},
        {"/usr/bin/dnf", {fixed("config-manager"), fixed("addrepo"),
                          fixed("--from-repofile"), filePathRun()}},
        {"/usr/bin/dnf", {fixed("clean"), fixed("all")}},
        {"/usr/bin/dnf",
         {fixed("-y"), fixed("install"), fixed("--skip-unavailable"), package()}},
        {"/usr/bin/dnf",
         {fixed("-y"), fixed("install"), fixed("--skip-unavailable"), fixed("--allowerasing"),
          package()}},
        {"/usr/bin/dnf", {fixed("-y"), fixed("install"), fixed("--allowerasing"), package()}},

        {"/usr/bin/flatpak",
         {fixed("install"), fixed("-y"), fixed("--system"), fixed("flathub"), package()}},
        {"/usr/bin/flatpak",
         {fixed("remote-add"), fixed("--if-not-exists"), fixed("--system"), fixed("flathub"),
          fixed("https://flathub.org/repo/flathub.flatpakrepo")}},
        {"/usr/bin/flatpak", {fixed("update"), fixed("-y"), fixed("--system")}},

        {"/usr/bin/rpm", {fixed("-q"), fixed("--quiet"), package()}, true},

        {"/usr/bin/curl", {fixed("-fsSL"), fixed("-o"), filePathRun(), urlHttps()}},

        {"/usr/bin/systemctl", {fixed("enable"), fixed("--now"), fixed("libvirtd")}},
        {"/usr/bin/systemctl",
         {fixed("disable"), fixed("--now"), fixed("NetworkManager-wait-online")}},
        {"/usr/bin/systemctl", {fixed("reboot")}},

        {"/usr/sbin/usermod", {fixed("-aG"), fixed("libvirt"), username()}},
        {"/usr/sbin/usermod", {fixed("-aG"), fixed("kvm"), username()}},

        {"/usr/sbin/setsebool",
         {fixed("-P"), fixed("domain_kernel_load_modules"), fixed("on")}},

        {"/usr/bin/sudo",
         {fixed("-u"), username(), fixed("/usr/bin/pipx"), fixed("install"),
          fixed("--include-deps"), pipxPackage()}},
    };
    return list;
}

bool isLower(const char character)
{
    return character >= 'a' && character <= 'z';
}
bool isDigit(const char character)
{
    return character >= '0' && character <= '9';
}

bool isPackageChar(const char character)
{
    return isLower(character) || (character >= 'A' && character <= 'Z') || isDigit(character) ||
           character == '_' || character == '-' || character == '.' || character == '+';
}

bool validPackage(const std::string &value)
{
    if (value.empty() || value.size() > 255)
        return false;
    // A leading hyphen would be parsed by dnf, rpm or flatpak as an option.
    if (!isLower(value.front()) && !(value.front() >= 'A' && value.front() <= 'Z') &&
        !isDigit(value.front()) && value.front() != '_')
        return false;
    for (const char character : value)
        if (!isPackageChar(character))
            return false;
    return true;
}

bool validCoprRepo(const std::string &value)
{
    if (value.empty() || value.size() > 128)
        return false;
    const auto slash = value.find('/');
    if (slash == std::string::npos || slash == 0 || slash + 1 >= value.size())
        return false;
    if (value.find('/', slash + 1) != std::string::npos)
        return false;
    for (std::size_t index = 0; index < slash; ++index) {
        const char character = value[index];
        if (!isLower(character) && !isDigit(character) && character != '_' && character != '-')
            return false;
    }
    for (std::size_t index = slash + 1; index < value.size(); ++index) {
        const char character = value[index];
        if (!isLower(character) && !isDigit(character) && character != '_' &&
            character != '-' && character != '.')
            return false;
    }
    return allowedCoprRepos().count(value) == 1;
}

bool validUrl(const std::string &value)
{
    if (value.size() <= 8 || value.size() > 512 || value.rfind("https://", 0) != 0)
        return false;
    for (const char character : value) {
        const bool plain = (character >= 'A' && character <= 'Z') || isLower(character) ||
                           isDigit(character);
        const bool punctuation = character == '-' || character == '_' || character == '.' ||
                                 character == '/' || character == '?' || character == '=' ||
                                 character == '&' || character == '%' || character == '+' ||
                                 character == ':' || character == '#' || character == '@';
        if (!plain && !punctuation)
            return false;
    }

    // The host is everything before the path, query or fragment. Only known
    // download hosts are allowed, and userinfo or ports are not.
    const std::string rest = value.substr(8);
    const std::string host = rest.substr(0, rest.find_first_of("/?#"));
    if (host.empty())
        return false;
    for (const char character : host) {
        const bool plain = isLower(character) || isDigit(character);
        if (!plain && character != '.' && character != '-')
            return false;
    }
    return allowedDownloadHosts().count(host) == 1;
}

bool validFilePathRun(const std::string &value)
{
    if (value.size() > 256)
        return false;
    constexpr const char *prefix = "/run/lgl-";
    if (value.rfind(prefix, 0) != 0)
        return false;

    const std::size_t slash = value.find('/', std::char_traits<char>::length(prefix));
    if (slash == std::string::npos || slash + 1 >= value.size())
        return false;
    if (value.find('/', slash + 1) != std::string::npos)
        return false;

    for (std::size_t index = std::char_traits<char>::length(prefix); index < slash; ++index) {
        const char character = value[index];
        if (!isLower(character) && !(character >= 'A' && character <= 'Z') && !isDigit(character))
            return false;
    }
    for (std::size_t index = slash + 1; index < value.size(); ++index) {
        const char character = value[index];
        if (!isLower(character) && !(character >= 'A' && character <= 'Z') &&
            !isDigit(character) && character != '_' && character != '-' && character != '.')
            return false;
    }
    return true;
}

bool validUsername(const std::string &value)
{
    if (value.empty() || value.size() > 32 || value == "root")
        return false;
    if (!isLower(value.front()) && value.front() != '_')
        return false;
    for (const char character : value)
        if (!isLower(character) && !isDigit(character) && character != '_' && character != '-')
            return false;
    return true;
}

bool matches(const ArgRule &rule, const std::string &value, const std::string &expectedUser)
{
    switch (rule.rule) {
    case Rule::Fixed:
        return rule.value && value == rule.value;
    case Rule::Package:
        return validPackage(value);
    case Rule::PipxPackage:
        return allowedPipxTools().count(value) == 1;
    case Rule::CoprRepo:
        return validCoprRepo(value);
    case Rule::UrlHttps:
        return validUrl(value);
    case Rule::FilePathRun:
        return validFilePathRun(value);
    case Rule::Username:
        // User-scoped operations may only target the pkexec caller.
        return validUsername(value) && (expectedUser.empty() || value == expectedUser);
    }
    return false;
}

} // namespace

bool validate(const std::string &program, const std::vector<std::string> &args,
              std::string &error, const std::string &expectedUser)
{
    for (const Op &op : allowList()) {
        if (program != op.program)
            continue;

        const std::size_t ruleCount = op.rules.size();
        if (op.variadic ? args.size() < ruleCount : args.size() != ruleCount)
            continue;

        bool matched = true;
        for (std::size_t index = 0; index < args.size(); ++index) {
            const ArgRule &rule = index < ruleCount ? op.rules[index] : op.rules.back();
            if (!matches(rule, args[index], expectedUser)) {
                matched = false;
                break;
            }
        }
        if (matched)
            return true;
    }

    error = "operation not permitted: " + program;
    return false;
}

} // namespace loadout::policy
