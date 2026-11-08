#include "core/catalog.h"

#include "core/plan.h"
#include "core/policy.h"
#include "core/selection.h"
#include "core/system.h"

#include <glib.h>

#include <algorithm>
#include <set>
#include <string>

using namespace loadout;

namespace {

void everyItemHasAnInstallStep()
{
    std::set<std::string> seen;
    for (const auto *item : catalog::allItems()) {
        g_assert_true(seen.insert(item->key).second);
        Selection selection;
        selection.set(item->key, true);
        selection.setChoice("gpu", "amd");
        SystemInfo system;
        const auto plan = buildPlan(selection, system, "/run/lgl-tmp");
        g_assert_cmpuint(plan.size(), >=, 2);   // bootstrap plus the item
    }
}

void everyPLannedCommandPassesTheAllowList()
{
    SystemInfo system;
    system.targetUser = "tester";
    Selection selection;
    for (const auto *item : catalog::allItems())
        selection.set(item->key, true);
    selection.setChoice("gpu", "amd");
    selection.setChoice("terminal", "ghostty");

    const auto plan = buildPlan(selection, system, "/run/lgl-abc123");
    g_assert_cmpuint(plan.size(), >, 50);

    for (const auto &step : plan) {
        if (step.command.empty() || step.local)
            continue;
        std::string program = step.command.front();
        std::vector<std::string> args(step.command.begin() + 1, step.command.end());
        std::string error;
        g_assert_true(policy::validate(program, args, error, system.targetUser));
        if (!step.alreadyInstalledCheck.empty()) {
            std::string checkError;
            std::vector<std::string> checkArgs(step.alreadyInstalledCheck.begin() + 1,
                                               step.alreadyInstalledCheck.end());
            g_assert_true(policy::validate(step.alreadyInstalledCheck.front(), checkArgs,
                                           checkError, system.targetUser));
        }
    }
}

void putsFlathubBeforeAnyFlatpakInstall()
{
    SystemInfo system;
    Selection selection;
    selection.set("browsers/chrome", true);

    const auto plan = buildPlan(selection, system, "/run/lgl-abc123");
    std::size_t remote = plan.size();
    std::size_t install = plan.size();
    for (std::size_t index = 0; index < plan.size(); ++index) {
        if (plan[index].id == "flathub_remote")
            remote = index;
        if (plan[index].id == "chrome")
            install = index;
    }
    g_assert_cmpuint(remote, <, install);
    g_assert_cmpuint(remote, <, plan.size());
}

void usesFedora44PackageNames()
{
    SystemInfo system;
    system.fedoraVersion = "44";
    Selection selection;
    selection.set("gpu/amd/mesa_va", true);
    selection.set("gpu/amd/mesa_vulkan", true);
    selection.setChoice("gpu", "amd");

    const auto plan = buildPlan(selection, system, "/run/lgl-abc123");
    bool sawVa = false;
    bool sawVulkan = false;
    for (const auto &step : plan)
        for (const auto &argument : step.command) {
            if (argument == "mesa-va-drivers-freeworld")
                sawVa = true;
            if (argument == "mesa-vulkan-drivers-freeworld")
                sawVulkan = true;
        }
    g_assert_true(sawVa);
    g_assert_true(sawVulkan);
}

void usesOldPackageNamesBeforeFedora44()
{
    SystemInfo system;
    system.fedoraVersion = "43";
    Selection selection;
    selection.set("gpu/amd/mesa_va", true);
    selection.setChoice("gpu", "amd");

    const auto plan = buildPlan(selection, system, "/run/lgl-abc123");
    bool sawOld = false;
    for (const auto &step : plan)
        for (const auto &argument : step.command)
            if (argument == "mesa-va-drivers")
                sawOld = true;
    g_assert_true(sawOld);
}

void installsFlatpakOnlyWhenNeeded()
{
    SystemInfo system;
    Selection empty;
    g_assert_false(needsFlatpak(empty));

    Selection flatpak;
    flatpak.set("browsers/chrome", true);
    g_assert_true(needsFlatpak(flatpak));

    Selection newer;
    newer.set("browsers/zen", true);
    g_assert_true(needsFlatpak(newer));

    Selection dnfOnly;
    dnfOnly.set("browsers/firefox", true);
    g_assert_false(needsFlatpak(dnfOnly));
}

void installsTheChosenEssentials()
{
    SystemInfo system;
    Selection selection;
    selection.set("choice:file-manager:dolphin", true);
    selection.set("choice:file-manager:nemo", true);
    selection.set("choice:terminal:kitty", true);
    selection.set("choice:terminal:foot", true);

    const auto plan = buildPlan(selection, system, "/run/lgl-abc123");
    std::set<std::string> ids;
    for (const auto &step : plan)
        ids.insert(step.id);

    g_assert_true(ids.count("essential_dolphin") == 1);
    g_assert_true(ids.count("essential_nemo") == 1);
    g_assert_true(ids.count("essential_kitty") == 1);
    g_assert_true(ids.count("essential_foot") == 1);
    g_assert_true(ids.count("essential_konsole") == 0);
    g_assert_cmpint(estimateDiskMb(selection), ==, 20 + 15 + 20 + 5);
}

void installsWirelessSupport()
{
    SystemInfo system;
    Selection selection;
    selection.set("essentials/wifi_nm", true);
    selection.set("essentials/wifi_wpa", true);
    selection.set("essentials/wifi_firmware", true);

    const auto plan = buildPlan(selection, system, "/run/lgl-abc123");
    bool nm = false;
    bool wpa = false;
    bool firmware = false;
    bool enabled = false;
    for (const auto &step : plan) {
        for (const auto &argument : step.command) {
            nm = nm || argument == "NetworkManager-wifi";
            wpa = wpa || argument == "wpa_supplicant";
            firmware = firmware || argument == "linux-firmware";
        }
        enabled = enabled || step.id == "wifi_nm_enable";
    }
    g_assert_true(nm);
    g_assert_true(wpa);
    g_assert_true(firmware);
    g_assert_true(enabled);
}

void installsTextEditors()
{
    SystemInfo system;
    Selection selection;
    selection.set("comms/kate", true);
    selection.set("comms/kwrite", true);

    const auto plan = buildPlan(selection, system, "/run/lgl-abc123");
    std::set<std::string> ids;
    for (const auto &step : plan)
        ids.insert(step.id);
    g_assert_true(ids.count("kate") == 1);
    g_assert_true(ids.count("kwrite") == 1);
}

void installsGhosttyFromItsCopr()
{
    SystemInfo system;
    Selection selection;
    selection.set("choice:terminal:ghostty", true);

    const auto plan = buildPlan(selection, system, "/run/lgl-abc123");
    std::size_t copr = plan.size();
    std::size_t install = plan.size();
    for (std::size_t index = 0; index < plan.size(); ++index) {
        if (plan[index].id == "copr_ghostty")
            copr = index;
        if (plan[index].id == "essential_ghostty")
            install = index;
    }

    // Ghostty is not in the Fedora repositories, so the COPR must be enabled
    // before the package is installed.
    g_assert_cmpuint(copr, <, plan.size());
    g_assert_cmpuint(install, <, plan.size());
    g_assert_cmpuint(copr, <, install);
    g_assert_true(plan[copr].command.size() == 5);
    g_assert_true(plan[copr].command[4] == "scottames/ghostty");
}

void bootstrapRunsFirstAndCleanupLast()
{
    SystemInfo system;
    Selection selection;
    selection.set("systools/clean_cache", true);
    selection.set("systools/btop", true);

    const auto plan = buildPlan(selection, system, "/run/lgl-abc123");
    g_assert_true(plan.front().id == "bootstrap");
    g_assert_true(plan.back().id == "clean_all");
}

void estimatesDiskSpace()
{
    Selection selection;
    g_assert_cmpint(estimateDiskMb(selection), ==, 0);

    selection.set("gaming/steam", true);
    selection.set("repos/rpmfusion_nonfree", true);
    g_assert_cmpint(estimateDiskMb(selection), ==, 401);
}

void wiresTheLibraryPrerequisitesInOrder()
{
    SystemInfo system;
    Selection selection;
    selection.set("gaming/heroic", true);
    selection.set("repos/rpmfusion_nonfree", true);

    const auto plan = buildPlan(selection, system, "/run/lgl-abc123");
    std::size_t remote = plan.size();
    std::size_t copr = plan.size();
    std::size_t install = plan.size();
    for (std::size_t index = 0; index < plan.size(); ++index) {
        if (plan[index].id == "flathub_remote")
            remote = index;
        if (plan[index].id == "heroic_copr")
            copr = index;
        if (plan[index].id == "heroic_install")
            install = index;
    }
    g_assert_cmpuint(copr, <, install);
    (void)remote;
}

void reportsUnmetRequirements()
{
    Selection selection;
    selection.set("gaming/steam", true);
    const auto unmet = catalog::unmetRequirements(selection, 44);
    g_assert_cmpuint(unmet.size(), ==, 1);
    g_assert_true(unmet.front().itemKey == "gaming/steam");
    g_assert_true(unmet.front().requiredKey == "repos/rpmfusion_nonfree");

    selection.set("repos/rpmfusion_nonfree", true);
    g_assert_cmpuint(catalog::unmetRequirements(selection, 44).size(), ==, 0);
}

void ignoresVersionGatedRequirements()
{
    Selection selection;
    selection.set("gpu/amd/mesa_va", true);

    g_assert_cmpuint(catalog::unmetRequirements(selection, 43).size(), ==, 0);
    g_assert_cmpuint(catalog::unmetRequirements(selection, 44).size(), ==, 1);
}

void installsTheVmCuratorQemuStack()
{
    SystemInfo system;
    Selection selection;
    selection.set("virt/vmcurator", true);

    const auto plan = buildPlan(selection, system, "/run/lgl-abc123");
    bool qemu = false;
    for (const auto &step : plan)
        for (const auto &argument : step.command)
            qemu = qemu || argument == "qemu-system-x86";
    g_assert_true(qemu);

    SystemInfo arm;
    arm.architecture = "aarch64";
    const auto armPlan = buildPlan(selection, arm, "/run/lgl-abc123");
    bool armQemu = false;
    for (const auto &step : armPlan)
        for (const auto &argument : step.command)
            armQemu = armQemu || argument == "qemu-system-aarch64";
    g_assert_true(armQemu);
}

void installsTheAiCodingToolsAsTheUser()
{
    SystemInfo system;
    system.targetUser = "tester";
    system.targetHome = "/home/tester";

    Selection opencode;
    opencode.set("development/opencode", true);
    const auto opencodePlan = buildPlan(opencode, system, "/run/lgl-abc123");
    bool downloaded = false;
    bool installed = false;
    bool linked = false;
    for (const auto &step : opencodePlan) {
        if (step.id == "opencode_download")
            downloaded = std::any_of(step.command.begin(), step.command.end(), [](const auto &arg) {
                return arg == "https://opencode.ai/install";
            });
        if (step.id == "opencode_install")
            installed = step.local;
        if (step.id == "opencode_link")
            linked = std::any_of(step.command.begin(), step.command.end(), [](const auto &arg) {
                return arg == "/home/tester/.local/bin/opencode";
            });
    }
    g_assert_true(downloaded);
    g_assert_true(installed);
    g_assert_true(linked);

    Selection pi;
    pi.set("development/pi", true);
    const auto piPlan = buildPlan(pi, system, "/run/lgl-abc123");
    bool node = false;
    bool piInstall = false;
    for (const auto &step : piPlan) {
        for (const auto &argument : step.command)
            node = node || argument == "nodejs22";
        piInstall = piInstall || step.id == "pi_install";
    }
    g_assert_true(node);
    g_assert_true(piInstall);
}

void installsAppImagesAsTheUser()
{
    SystemInfo system;
    system.targetUser = "tester";
    Selection selection;
    selection.set("gaming/epic", true);
    selection.set("content/audacity", true);

    const auto plan = buildPlan(selection, system, "/run/lgl-abc123");

    std::size_t epicSteps = 0;
    std::size_t audacitySteps = 0;
    for (const auto &step : plan) {
        if (!step.local)
            continue;
        g_assert_false(step.command.empty());
        const auto &joined = step.command;
        const bool underApplications = std::any_of(joined.begin(), joined.end(), [](const auto &a) {
            return a.rfind("/home/tester/Applications", 0) == 0;
        });
        g_assert_true(underApplications);
        if (step.id.rfind("epic", 0) == 0)
            ++epicSteps;
        if (step.id.rfind("audacity", 0) == 0)
            ++audacitySteps;
    }

    g_assert_cmpuint(epicSteps, ==, 4);       // mkdir, download, publish, chmod
    g_assert_cmpuint(audacitySteps, ==, 4);

    bool checked = false;
    for (const auto &step : plan) {
        if (step.id != "audacity_download")
            continue;
        checked = true;
        g_assert_true(step.verifyPath.size() > 5);
        g_assert_true(step.verifyPath.substr(step.verifyPath.size() - 5) == ".part");
        g_assert_true(step.expectedSha256.size() == 64);
        g_assert_true(std::any_of(step.command.begin(), step.command.end(), [](const auto &arg) {
            return arg == "--max-time";
        }));
    }
    g_assert_true(checked);

    for (const auto &step : plan) {
        if (step.id == "audacity_publish")
            g_assert_true(step.dependsOn == "audacity_download");
        if (step.id == "audacity_executable")
            g_assert_true(step.dependsOn == "audacity_publish");
    }
}

void createsUserScopeDesktopEntries()
{
    SystemInfo system;
    system.targetUser = "tester";
    Selection selection;
    selection.set("gaming/epic", true);
    selection.set("content/audacity", true);
    selection.set("comms/artcraft", true);

    const auto plan = buildPlan(selection, system, "/run/lgl-abc123");
    std::set<std::string> ids;
    for (const auto &step : plan) {
        if (!step.desktop)
            continue;
        ids.insert(step.desktop->id);
        g_assert_true(step.desktop->home == "/home/tester");
        g_assert_true(step.desktop->executable.rfind("/home/tester/Applications/", 0) == 0);
        g_assert_false(step.desktop->iconResource.empty());
        const auto contents = desktopFileContents(*step.desktop);
        g_assert_true(contents.find("Exec=" + step.desktop->executable) != std::string::npos);
        g_assert_true(contents.find("Icon=" + step.desktop->id) != std::string::npos);
    }
    g_assert_true(ids.count("com.epicgames.EpicGamesLauncher") == 1);
    g_assert_true(ids.count("org.audacityteam.Audacity") == 1);
    g_assert_true(ids.count("ai.storyteller.artcraft-launcher") == 1);
}

void usesTheArchitectureSpecificAppImage()
{
    SystemInfo system;
    system.targetUser = "tester";
    system.architecture = "aarch64";
    Selection selection;
    selection.set("content/audacity", true);
    selection.set("comms/artcraft", true);

    const auto plan = buildPlan(selection, system, "/run/lgl-abc123");
    bool audacityArm = false;
    bool artcraftArm = false;
    for (const auto &step : plan) {
        for (const auto &argument : step.command) {
            if (argument.find("audacity-linux-4.0.1-aarch64.AppImage") != std::string::npos)
                audacityArm = true;
            if (argument.find("artcraft-launcher-0.7.0-linux-aarch64.AppImage") != std::string::npos)
                artcraftArm = true;
        }
    }
    g_assert_true(audacityArm);
    g_assert_true(artcraftArm);
}

} // namespace

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/plan/every-item", everyItemHasAnInstallStep);
    g_test_add_func("/plan/allow-list", everyPLannedCommandPassesTheAllowList);
    g_test_add_func("/plan/flathub-first", putsFlathubBeforeAnyFlatpakInstall);
    g_test_add_func("/plan/fedora-44-names", usesFedora44PackageNames);
    g_test_add_func("/plan/fedora-43-names", usesOldPackageNamesBeforeFedora44);
    g_test_add_func("/plan/flatpak-needed", installsFlatpakOnlyWhenNeeded);
    g_test_add_func("/plan/essentials", installsTheChosenEssentials);
    g_test_add_func("/plan/wifi", installsWirelessSupport);
    g_test_add_func("/plan/editors", installsTextEditors);
    g_test_add_func("/plan/ghostty", installsGhosttyFromItsCopr);
    g_test_add_func("/plan/vmcurator", installsTheVmCuratorQemuStack);
    g_test_add_func("/plan/ai-tools", installsTheAiCodingToolsAsTheUser);
    g_test_add_func("/plan/appimages", installsAppImagesAsTheUser);
    g_test_add_func("/plan/appimage-desktop", createsUserScopeDesktopEntries);
    g_test_add_func("/plan/appimage-arch", usesTheArchitectureSpecificAppImage);
    g_test_add_func("/plan/order", bootstrapRunsFirstAndCleanupLast);
    g_test_add_func("/plan/estimate", estimatesDiskSpace);
    g_test_add_func("/plan/prerequisites", wiresTheLibraryPrerequisitesInOrder);
    g_test_add_func("/catalog/unmet", reportsUnmetRequirements);
    g_test_add_func("/catalog/version-gated", ignoresVersionGatedRequirements);
    return g_test_run();
}
