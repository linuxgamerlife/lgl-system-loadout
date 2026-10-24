#include "core/system.h"

#include "core/process.h"

#include <pwd.h>
#include <unistd.h>

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace loadout {
namespace {

std::string trimmed(const std::string &text)
{
    constexpr const char *whitespace = " \t\r\n\f\v";
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string::npos)
        return {};
    return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
}

} // namespace

std::string parseFedoraVersion(const std::string &raw)
{
    const auto value = trimmed(raw);
    if (value.empty() || value.front() == '%')
        return "44";
    for (const char character : value)
        if (!std::isdigit(static_cast<unsigned char>(character)))
            return "44";
    return value;
}

bool isValidTargetUser(const std::string &name)
{
    if (name.empty() || name.size() > 32 || name == "root")
        return false;
    const auto isLower = [](const char character) {
        return character >= 'a' && character <= 'z';
    };
    if (!isLower(name.front()) && name.front() != '_')
        return false;
    for (const char character : name)
        if (!isLower(character) && character != '_' && character != '-' &&
            !std::isdigit(static_cast<unsigned char>(character)))
            return false;
    return true;
}

SystemInfo detectSystem()
{
    SystemInfo system;

    const auto release = runCommand({"/usr/bin/rpm", "-E", "%fedora"}, 3000);
    system.fedoraVersion = parseFedoraVersion(release.output);

    const auto machine = runCommand({"/usr/bin/uname", "-m"}, 2000);
    const auto architecture = trimmed(machine.output);
    if (!architecture.empty() && architecture.size() <= 16) {
        system.architecture = architecture;
    } else {
#if defined(__aarch64__)
        system.architecture = "aarch64";
#elif defined(__x86_64__)
        system.architecture = "x86_64";
#endif
    }

    // SUDO_USER is only trustworthy when the process really is root; a user can
    // put anything in the environment of a normal GUI process.
    const char *sudoUser = std::getenv("SUDO_USER");
    if (geteuid() == 0 && sudoUser && isValidTargetUser(sudoUser)) {
        system.targetUser = sudoUser;
    } else {
        const auto login = runCommand({"/usr/bin/logname"}, 2000);
        const auto name = trimmed(login.output);
        if (isValidTargetUser(name)) {
            system.targetUser = name;
        } else if (const passwd *self = getpwuid(getuid());
                   self && self->pw_name && isValidTargetUser(self->pw_name)) {
            system.targetUser = self->pw_name;
        } else {
            system.targetUser = "root";
        }
    }

    if (const passwd *entry = getpwnam(system.targetUser.c_str());
        entry && entry->pw_dir && *entry->pw_dir)
        system.targetHome = entry->pw_dir;

    return system;
}

int availableDiskMb()
{
    const auto result = runCommand({"/usr/bin/df", "--output=avail", "-m", "/"}, 3000);
    std::istringstream lines(result.output);
    std::string line;
    std::string last;
    while (std::getline(lines, line))
        if (!trimmed(line).empty())
            last = trimmed(line);
    if (last.empty())
        return -1;
    try {
        const auto value = std::stoi(last);
        return value > 0 ? value : -1;
    } catch (const std::exception &) {
        return -1;
    }
}

bool helperActionKnown()
{
    const auto result = runCommand(
        {"/usr/bin/pkaction", "--action-id", "com.linuxgamerlife.lgl-system-loadout.run-helper"},
        3000);
    return result.exitCode == 0;
}

} // namespace loadout
