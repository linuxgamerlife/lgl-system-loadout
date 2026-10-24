#include "core/plan.h"

#include "core/catalog.h"

#include <initializer_list>
#include <map>

namespace loadout {
namespace {

using Steps = std::vector<InstallStep>;

int fedora(const SystemInfo &system)
{
    try {
        return std::stoi(system.fedoraVersion);
    } catch (const std::exception &) {
        return 44;
    }
}

InstallStep step(std::string id, std::string description, std::vector<std::string> command,
                 const bool optional = false, std::vector<std::string> check = {},
                 std::set<int> allowed = {})
{
    return InstallStep{std::move(id), std::move(description), std::move(command), optional,
                       std::move(check), std::move(allowed)};
}

InstallStep dnf(const std::string &id, const std::string &description,
                std::initializer_list<const char *> packages)
{
    std::vector<std::string> command{"/usr/bin/dnf", "-y", "install"};
    for (const char *package : packages)
        command.emplace_back(package);
    return step(id, description, std::move(command));
}

InstallStep flathub(const std::string &id, const std::string &label, const std::string &appId)
{
    return step(id, "Install " + label + " (Flatpak)",
                {"/usr/bin/flatpak", "install", "-y", "--system", "flathub", appId});
}

InstallStep pipx(const std::string &id, const std::string &tool, const std::string &user)
{
    return step(id, "Install " + tool + " via pipx",
                {"/usr/bin/sudo", "-u", user, "/usr/bin/pipx", "install", "--include-deps", tool});
}

InstallStep localStep(std::string id, std::string description, std::vector<std::string> command)
{
    return InstallStep{std::move(id), std::move(description), std::move(command), false, {}, {},
                       true};
}

// AppImages live in the user's ~/Applications, so every command here runs as the
// user rather than through the helper.
Steps appImageSteps(const std::string &id, const std::string &filename, const std::string &url,
                    const SystemInfo &system, const std::string &sha256)
{
    const std::string home =
        system.targetHome.empty() ? "/home/" + system.targetUser : system.targetHome;
    const std::string directory = home + "/Applications";
    const std::string target = directory + "/" + filename;
    const std::string part = target + ".part";

    // Download beside the target and publish with a rename, so a failed or
    // partial download never replaces a working AppImage.
    auto download = localStep(
        id + "_download", "Download " + filename,
        {"/usr/bin/curl", "-fsSL", "--retry", "3", "--retry-delay", "2", "--connect-timeout",
         "30", "--max-time", "900", "--proto", "=https", "-o", part, url});
    download.verifyPath = part;
    download.expectedSha256 = sha256;

    auto publish =
        localStep(id + "_publish", "Install " + filename, {"/usr/bin/mv", "-f", part, target});
    publish.dependsOn = id + "_download";

    auto executable = localStep(id + "_executable", "Make " + filename + " executable",
                                {"/usr/bin/chmod", "+x", target});
    executable.dependsOn = id + "_publish";

    return {
        localStep(id + "_directory", "Create " + directory,
                  {"/usr/bin/mkdir", "-p", directory}),
        download,
        publish,
        executable,
    };
}

std::string dnfPluginsPackage(const int version)
{
    return version >= 44 ? "dnf5-plugins" : "python3-dnf5-plugins";
}

std::string mesaVaPackage(const int version)
{
    return version >= 44 ? "mesa-va-drivers-freeworld" : "mesa-va-drivers";
}

std::string mesaVulkanPackage(const int version)
{
    return version >= 44 ? "mesa-vulkan-drivers-freeworld" : "mesa-vulkan-drivers";
}

std::string repoFile(const std::string &tmpDir, const std::string &name)
{
    return tmpDir.empty() ? "/run/lgl-tmp/" + name : tmpDir + "/" + name;
}

Steps catalogSteps(const std::string &key, const SystemInfo &system, const std::string &tmpDir)
{
    const int version = fedora(system);
    const std::string &user = system.targetUser;

    if (key == "repos/rpmfusion_free")
        return {step("rpmfusion_free", "Enable RPM Fusion Free",
                     {"/usr/bin/dnf", "-y", "install",
                      "https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-" +
                          system.fedoraVersion + ".noarch.rpm"})};
    if (key == "repos/rpmfusion_nonfree")
        return {step("rpmfusion_nonfree", "Enable RPM Fusion NonFree",
                     {"/usr/bin/dnf", "-y", "install",
                      "https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-" +
                          system.fedoraVersion + ".noarch.rpm"})};

    if (key == "systools/moonlight")
        return {flathub("flatpak_moonlight", "Moonlight", "com.moonlight_stream.Moonlight")};
    if (key == "systools/sunshine")
        return {flathub("flatpak_sunshine", "Sunshine", "dev.lizardbyte.app.Sunshine")};
    if (key == "systools/parsec")
        return {flathub("flatpak_parsec", "Parsec", "com.parsecgaming.parsec")};
    if (key == "systools/flatseal")
        return {flathub("flatpak_flatseal", "Flatseal", "com.github.tchx84.Flatseal")};
    if (key == "systools/tldr")
        return {pipx("pipx_tldr", "tldr", user)};
    if (key == "systools/nm_wait_online")
        return {step("nm_wait_online", "Disable NetworkManager-wait-online",
                     {"/usr/bin/systemctl", "disable", "--now", "NetworkManager-wait-online"},
                     true)};
    if (key == "systools/clean_cache")
        return {step("clean_all", "Clean the DNF cache", {"/usr/bin/dnf", "clean", "all"})};
    if (key.rfind("systools/", 0) == 0) {
        const std::string package = key.substr(std::string("systools/").size());
        return {dnf("systool_" + package, "Install " + package, {package.c_str()})};
    }

    if (key == "python/pip")
        return {dnf("pip", "Install python3-pip", {"python3-pip"})};
    if (key == "python/pipx")
        return {dnf("pipx_pkg", "Install pipx", {"pipx"})};
    if (key == "python/git")
        return {step("git", "Install git", {"/usr/bin/dnf", "-y", "install", "git"}, false,
                     {"/usr/bin/rpm", "-q", "--quiet", "git"})};
    if (key == "python/zed")
        return {flathub("flatpak_zed", "Zed", "dev.zed.Zed")};
    if (key == "python/github_desktop")
        return {flathub("flatpak_github_desktop", "GitHub Desktop", "io.github.shiftey.Desktop")};

    if (key == "media/ffmpeg")
        return {step("ffmpeg_swap", "Swap ffmpeg-free for the full ffmpeg",
                     {"/usr/bin/dnf", "swap", "-y", "ffmpeg-free", "ffmpeg", "--allowerasing"})};
    if (key == "media/gst_bad_nonfree")
        return {step("gst_bad_nonfree", "Install GStreamer ugly plugins",
                     {"/usr/bin/dnf", "-y", "install", "--skip-unavailable", "--allowerasing",
                      "gstreamer1-plugins-ugly"})};
    if (key == "media/gst_bad_free_extras")
        return {step("gst_bad_free_extras", "Install GStreamer bad-free-extras",
                     {"/usr/bin/dnf", "-y", "install", "--skip-unavailable",
                      "gstreamer1-plugins-bad-free-extras"})};
    if (key == "media/vlc")
        return {dnf("vlc", "Install VLC", {"vlc"})};

    if (key == "content/obs")
        return {dnf("obs", "Install OBS Studio", {"obs-studio"})};
    if (key == "content/audacity") {
        if (system.architecture == "aarch64")
            return appImageSteps(
                "audacity", "audacity-linux-4.0.1-aarch64.AppImage",
                "https://github.com/audacity/audacity/releases/download/Audacity-4.0.1/"
                "audacity-linux-4.0.1-aarch64.AppImage",
                system,
                // SHA-256 from the Audacity-4.0.1 CHECKSUMS.txt.
                "3abc517f26f00197eaac9c4e56b35dc53d8e933764b41077950e8526cd4aa075");
        return appImageSteps(
            "audacity", "audacity-linux-4.0.1-x86_64.AppImage",
            "https://github.com/audacity/audacity/releases/download/Audacity-4.0.1/"
            "audacity-linux-4.0.1-x86_64.AppImage",
            system,
            // SHA-256 from the Audacity-4.0.1 CHECKSUMS.txt.
            "ca2f04f172124d1f18ac608749854c5d31f04ab266a758c348190c05d9b2087c");
    }
    if (key == "content/ytdlp")
        return {pipx("pipx_ytdlp", "yt-dlp", user)};

    if (key == "gpu/amd/mesa_dri")
        return {dnf("amd_mesa_dri", "Install mesa-dri-drivers", {"mesa-dri-drivers"})};
    if (key == "gpu/amd/mesa_vulkan")
        return {dnf("amd_mesa_vulkan", "Install the Mesa Vulkan drivers",
                    {mesaVulkanPackage(version).c_str()})};
    if (key == "gpu/amd/vulkan_loader")
        return {dnf("amd_vulkan_loader", "Install vulkan-loader", {"vulkan-loader"})};
    if (key == "gpu/amd/mesa_va")
        return {dnf("amd_mesa_va", "Install the VA-API driver",
                    {mesaVaPackage(version).c_str()})};
    if (key == "gpu/amd/linux_fw")
        return {dnf("amd_linux_fw", "Install linux-firmware", {"linux-firmware"})};

    if (key == "gaming/kernel_modules_extra")
        return {dnf("gaming_kernel_modules_extra", "Install kernel-modules-extra",
                    {"kernel-modules-extra"})};
    if (key == "gaming/steam")
        return {dnf("gaming_steam", "Install Steam", {"steam"})};
    if (key == "gaming/lutris")
        return {dnf("gaming_lutris", "Install Lutris", {"lutris"})};
    if (key == "gaming/goverlay")
        return {step("goverlay_copr", "Enable the LGL Essentials COPR",
                     {"/usr/bin/dnf", "copr", "enable", "-y", "linuxgamerlife/lgl-essentials"}),
                dnf("gaming_goverlay", "Install GOverlay", {"goverlay"})};
    if (key == "gaming/wine")
        return {step("gaming_wine", "Install wine and wine.i686",
                     {"/usr/bin/dnf", "-y", "install", "wine", "wine.i686"})};
    if (key == "gaming/protontricks")
        return {dnf("gaming_protontricks", "Install Protontricks", {"protontricks"})};
    if (key == "gaming/heroic")
        return {step("heroic_copr", "Enable the Heroic Games Launcher COPR",
                     {"/usr/bin/dnf", "copr", "enable", "-y", "atim/heroic-games-launcher"}),
                dnf("heroic_install", "Install heroic-games-launcher-bin",
                    {"heroic-games-launcher-bin"})};
    if (key == "gaming/epic")
        // Epic serves a moving installer at this URL, so there is no stable
        // checksum to pin. The download is still atomic and the card is hidden
        // on architectures Epic does not support.
        return appImageSteps(
            "epic", "EpicGamesLauncher-x86_64.AppImage",
            "https://launcher-public-service-prod06.ol.epicgames.com/launcher/api/installer/"
            "download/EpicGamesLauncher-x86_64.AppImage",
            system, {});    if (key == "gaming/protonplus")
        return {flathub("flatpak_protonplus", "ProtonPlus", "com.vysp3r.ProtonPlus")};
    if (key == "gaming/protonup")
        return {flathub("flatpak_protonup", "ProtonUp-Qt", "net.davidotek.pupgui2")};
    if (key == "gaming/faugus")
        return {step("faugus_copr", "Enable the Faugus Game Launcher COPR",
                     {"/usr/bin/dnf", "copr", "enable", "-y", "faugus/faugus-launcher"}),
                dnf("faugus_install", "Install faugus-launcher", {"faugus-launcher"})};

    if (key == "virt/virtmanager")
        return {dnf("virt_virtmanager", "Install virt-manager", {"virt-manager"})};
    if (key == "virt/libvirt")
        return {dnf("virt_libvirt", "Install libvirt", {"libvirt"})};
    if (key == "virt/virt_install")
        return {dnf("virt_virt_install", "Install virt-install", {"virt-install"})};
    if (key == "virt/virt_viewer")
        return {dnf("virt_virt_viewer", "Install virt-viewer", {"virt-viewer"})};
    if (key == "virt/vmcurator") {
        const std::vector<std::string> bundle{"vm-curator", "edk2-ovmf", "virt-viewer",
                                              "qemu-ui-sdl", "passt", "swtpm", "dnsmasq",
                                              "iproute", "nftables"};
        std::vector<std::string> command{"/usr/bin/dnf", "-y", "install"};
        std::vector<std::string> check{"/usr/bin/rpm", "-q", "--quiet"};
        command.insert(command.end(), bundle.begin(), bundle.end());
        check.insert(check.end(), bundle.begin(), bundle.end());
        return {step("virt_vmcurator_copr", "Enable the VM Curator COPR",
                     {"/usr/bin/dnf", "copr", "enable", "-y", "linuxgamerlife/lgl-vm-curator"}),
                step("virt_vmcurator_install", "Install VM Curator and its runtime support",
                     std::move(command), false, std::move(check)),
                step("kvm_group", "Add " + user + " to the kvm group",
                     {"/usr/sbin/usermod", "-aG", "kvm", user})};
    }

    if (key == "browsers/firefox")
        return {dnf("firefox", "Install Firefox", {"firefox"})};
    if (key == "browsers/chromium")
        return {dnf("chromium", "Install Chromium", {"chromium"})};
    if (key == "browsers/chrome")
        return {flathub("chrome", "Google Chrome", "com.google.Chrome")};
    if (key == "browsers/brave")
        return {step("brave_repo_dl", "Download the Brave repository file",
                     {"/usr/bin/curl", "-fsSL", "-o", repoFile(tmpDir, "brave-browser.repo"),
                      "https://brave-browser-rpm-release.s3.brave.com/brave-browser.repo"}),
                step("brave_repo_add", "Add the Brave repository",
                     {"/usr/bin/dnf", "config-manager", "addrepo", "--from-repofile",
                      repoFile(tmpDir, "brave-browser.repo")}),
                dnf("brave", "Install Brave", {"brave-browser"})};
    if (key == "browsers/vivaldi")
        return {step("vivaldi_repo_dl", "Download the Vivaldi repository file",
                     {"/usr/bin/curl", "-fsSL", "-o", repoFile(tmpDir, "vivaldi.repo"),
                      "https://repo.vivaldi.com/archive/vivaldi-fedora.repo"}),
                step("vivaldi_repo_add", "Add the Vivaldi repository",
                     {"/usr/bin/dnf", "config-manager", "addrepo", "--from-repofile",
                      repoFile(tmpDir, "vivaldi.repo")}),
                dnf("vivaldi", "Install Vivaldi", {"vivaldi-stable"})};
    if (key == "browsers/librewolf")
        return {flathub("librewolf", "LibreWolf", "io.gitlab.librewolf-community")};
    if (key == "browsers/waterfox")
        return {flathub("waterfox", "Waterfox", "net.waterfox.waterfox")};
    if (key == "browsers/zen")
        return {flathub("zen", "Zen", "app.zen_browser.zen")};
    if (key == "browsers/floorp")
        return {flathub("floorp", "Floorp", "one.ablaze.floorp")};
    if (key == "browsers/edge")
        return {step("edge_repo_dl", "Download the Microsoft Edge repository file",
                     {"/usr/bin/curl", "-fsSL", "-o", repoFile(tmpDir, "microsoft-edge.repo"),
                      "https://packages.microsoft.com/yumrepos/edge/config.repo"}),
                step("edge_repo_add", "Add the Microsoft Edge repository",
                     {"/usr/bin/dnf", "config-manager", "addrepo", "--from-repofile",
                      repoFile(tmpDir, "microsoft-edge.repo")}),
                dnf("edge", "Install Microsoft Edge", {"microsoft-edge-stable"})};
    if (key == "browsers/helium")
        return {step("helium_copr", "Enable the Helium COPR",
                     {"/usr/bin/dnf", "copr", "enable", "-y", "imput/helium"}),
                dnf("helium_install", "Install Helium", {"helium-bin"})};

    if (key == "comms/office_calc")
        return {dnf("office_calc", "Install LibreOffice Calc", {"libreoffice-calc"})};
    if (key == "comms/office_writer")
        return {dnf("office_writer", "Install LibreOffice Writer", {"libreoffice-writer"})};
    if (key == "comms/discord")
        return {flathub("flatpak_discord", "Discord", "com.discordapp.Discord")};
    if (key == "comms/telegram")
        return {flathub("flatpak_telegram", "Telegram", "org.telegram.desktop")};
    if (key == "comms/spotify")
        return {flathub("flatpak_spotify", "Spotify", "com.spotify.Client")};
    if (key == "comms/lgl_cliamp")
        return {step("cliamp_copr", "Enable the LGL CLIamp COPR",
                     {"/usr/bin/dnf", "copr", "enable", "-y", "linuxgamerlife/lgl-cliamp"}),
                dnf("cliamp_install", "Install lgl-cliamp", {"lgl-cliamp"})};

    if (key == "comms/artcraft") {
        if (system.architecture == "aarch64")
            return appImageSteps(
                "artcraft", "artcraft-launcher-0.7.0-linux-aarch64.AppImage",
                "https://github.com/storytold/craft-launcher/releases/download/v0.7.0/"
                "artcraft-launcher-0.7.0-linux-aarch64.AppImage",
                system,
                // SHA-256 computed from the v0.7.0 aarch64 release asset.
                "82fb86a4c078c8dee3e167de2eca704e178a8b8f0d8273aab57cf7ede0fa69b4");
        return appImageSteps(
            "artcraft", "artcraft-launcher-0.7.0-linux-x86_64.AppImage",
            "https://github.com/storytold/craft-launcher/releases/download/v0.7.0/"
            "artcraft-launcher-0.7.0-linux-x86_64.AppImage",
            system,
            // SHA-256 computed from the v0.7.0 x86_64 release asset.
            "f80b9bdcfeb92bc88a54d07e7f2e03f1880f3fcf25c1511864b3d4118ecee1d8");
    }

    if (key == "cachyos/kernel")
        return {step("cachyos_copr1", "Enable the kernel-cachyos COPR",
                     {"/usr/bin/dnf", "copr", "enable", "-y", "bieszczaders/kernel-cachyos"}),
                dnf("cachyos_kernel", "Install kernel-cachyos", {"kernel-cachyos"})};
    if (key == "cachyos/kernel_devel")
        return {dnf("cachyos_devel", "Install kernel-cachyos-devel-matched",
                    {"kernel-cachyos-devel-matched"})};

    if (key == "toolkit/lgl_scheduler_manager")
        return {step("toolkit_scx_scheds", "Install scx-scheds (prerequisite)",
                     {"/usr/bin/dnf", "-y", "install", "--allowerasing", "scx-scheds"}, true,
                     {"/usr/bin/rpm", "-q", "--quiet", "scx-scheds-git"}, {1}),
                step("toolkit_scx_tools", "Install scx-tools (prerequisite)",
                     {"/usr/bin/dnf", "-y", "install", "--allowerasing", "scx-tools"}, false, {},
                     {1}),
                dnf("toolkit_scheduler_install", "Install lgl-scheduler-manager",
                    {"lgl-scheduler-manager"})};
    if (key == "toolkit/lgl_dnf_helper")
        return {dnf("toolkit_lgl_dnf_helper_install", "Install lgl-dnf-helper",
                    {"lgl-dnf-helper"})};
    if (key == "toolkit/lgl_emoji_picker")
        return {dnf("toolkit_lgl_emoji_picker_install", "Install lgl-emoji-picker",
                    {"lgl-emoji-picker"})};
    if (key == "toolkit/lgl_colour_picker")
        return {dnf("toolkit_lgl_colour_picker_install", "Install lgl-colour-picker",
                    {"lgl-colour-picker"})};
    if (key == "toolkit/lgl_papercutter")
        return {dnf("toolkit_lgl_papercutter_install", "Install lgl-papercutter",
                    {"lgl-papercutter"})};
    if (key == "toolkit/lgl_keychron_helper")
        return {dnf("toolkit_lgl_keychron_helper_install", "Install lgl-keychron-helper",
                    {"lgl-keychron-helper"})};
    if (key == "toolkit/lgl_powerprofile_manager")
        return {step("toolkit_lgl_powerprofile_manager_copr",
                     "Enable the lgl-powerprofile-manager COPR",
                     {"/usr/bin/dnf", "copr", "enable", "-y",
                      "linuxgamerlife/lgl-powerprofile-manager"}),
                dnf("toolkit_lgl_powerprofile_manager_install",
                    "Install lgl-powerprofile-manager", {"lgl-powerprofile-manager"})};

    return {};
}

} // namespace

bool needsFlatpak(const Selection &selection)
{
    for (const char *key :
         {"gaming/heroic", "gaming/protonup", "gaming/protonplus", "systools/flatseal",
          "systools/moonlight", "systools/sunshine", "systools/parsec", "comms/discord",
          "comms/telegram",
          "browsers/librewolf", "browsers/chrome", "browsers/waterfox", "browsers/zen",
          "browsers/floorp", "python/zed", "python/github_desktop"})
        if (selection.get(key))
            return true;
    return false;
}

std::vector<InstallStep> buildPlan(const Selection &selection, const SystemInfo &system,
                                   const std::string &tmpDir)
{
    Steps plan;
    const int version = fedora(system);
    const std::string &user = system.targetUser;

    const auto add = [&](const std::string &key) {
        for (auto &entry : catalogSteps(key, system, tmpDir))
            plan.push_back(std::move(entry));
    };
    const auto selected = [&](const std::string &key) { return selection.get(key); };

    const std::string plugins = dnfPluginsPackage(version);
    plan.push_back(step("bootstrap", "Ensure core system tools are present",
                        {"/usr/bin/dnf", "-y", "install", "curl", "wget2-wget", "git", plugins},
                        false, {"/usr/bin/rpm", "-q", "--quiet", "curl", "wget2-wget", "git",
                                plugins}));

    if (needsFlatpak(selection)) {
        plan.push_back(dnf("flatpak_pkg", "Install Flatpak", {"flatpak"}));
        plan.push_back(step("flathub_remote", "Add the Flathub remote",
                            {"/usr/bin/flatpak", "remote-add", "--if-not-exists", "--system",
                             "flathub", "https://flathub.org/repo/flathub.flatpakrepo"}));
    }

    for (const char *key : {"repos/rpmfusion_free", "repos/rpmfusion_nonfree"})
        if (selected(key))
            add(key);

    // Essentials: one file manager and one terminal, if chosen.
    for (const auto &group : catalog::choiceGroups()) {
        const auto &chosen = selection.choice(group.id);
        for (const auto &option : group.options)
            if (option.value == chosen) {
                if (!option.copr.empty())
                    plan.push_back(step("copr_" + option.value,
                                        "Enable the " + option.label + " repository",
                                        {"/usr/bin/dnf", "copr", "enable", "-y", option.copr}));
                plan.push_back(dnf("essential_" + option.value, "Install " + option.label,
                                   {option.package.c_str()}));
            }
    }

    for (const char *key :
         {"systools/fastfetch", "systools/btop", "systools/cmatrix", "systools/distrobox",
          "systools/timeshift", "systools/cbonsai", "systools/podman", "systools/moonlight",
          "systools/sunshine", "systools/tailscale", "systools/parsec"})
        if (selected(key))
            add(key);
    if (selected("systools/flatseal"))
        add("systools/flatseal");

    if (selected("python/pip"))
        add("python/pip");
    if (selected("python/git"))
        add("python/git");
    if (selected("python/pipx") || selected("systools/tldr") || selected("content/ytdlp"))
        plan.push_back(dnf("pipx_pkg", "Install pipx", {"pipx"}));
    if (selected("systools/tldr"))
        add("systools/tldr");
    if (selected("python/zed"))
        add("python/zed");
    if (selected("python/github_desktop"))
        add("python/github_desktop");

    for (const char *key :
         {"media/ffmpeg", "media/gst_bad_nonfree", "media/gst_bad_free_extras", "media/vlc"})
        if (selected(key))
            add(key);

    for (const char *key : {"content/obs", "content/audacity"})
        if (selected(key))
            add(key);
    if (selected("content/ytdlp"))
        add("content/ytdlp");

    if (selection.choice("gpu") == "amd")
        for (const char *key : {"gpu/amd/mesa_dri", "gpu/amd/mesa_vulkan",
                                "gpu/amd/vulkan_loader", "gpu/amd/mesa_va",
                                "gpu/amd/linux_fw"})
            if (selected(key))
                add(key);

    for (const char *key :
         {"gaming/kernel_modules_extra", "gaming/steam", "gaming/lutris", "gaming/goverlay", "gaming/wine", "gaming/protontricks"})
        if (selected(key))
            add(key);
    for (const char *key : {"gaming/heroic", "gaming/epic", "gaming/protonplus",
                            "gaming/protonup", "gaming/faugus"})
        if (selected(key))
            add(key);

    const bool libvirt = selected("virt/virtmanager") || selected("virt/libvirt") ||
                         selected("virt/virt_install");
    if (libvirt)
        plan.push_back(step("virt_libvirt_runtime", "Install the local libvirt QEMU runtime",
                            {"/usr/bin/dnf", "-y", "install", "libvirt-daemon-kvm"}, false,
                            {"/usr/bin/rpm", "-q", "--quiet", "libvirt-daemon-kvm"}));
    for (const char *key : {"virt/virtmanager", "virt/libvirt", "virt/virt_install",
                            "virt/virt_viewer"})
        if (selected(key))
            add(key);
    if (selected("virt/vmcurator"))
        add("virt/vmcurator");
    if (libvirt) {
        plan.push_back(step("libvirtd_enable", "Enable the libvirtd service",
                            {"/usr/bin/systemctl", "enable", "--now", "libvirtd"}));
        plan.push_back(step("libvirt_group", "Add " + user + " to the libvirt group",
                            {"/usr/sbin/usermod", "-aG", "libvirt", user}));
    }

    for (const char *key :
         {"browsers/firefox", "browsers/chromium", "browsers/chrome", "browsers/brave",
          "browsers/vivaldi", "browsers/librewolf", "browsers/edge", "browsers/helium",
          "browsers/waterfox", "browsers/zen", "browsers/floorp"})
        if (selected(key))
            add(key);

    for (const char *key : {"comms/office_calc", "comms/office_writer", "comms/discord",
                            "comms/telegram", "comms/spotify", "comms/lgl_cliamp",
                            "comms/artcraft"})
        if (selected(key))
            add(key);

    if (selected("cachyos/kernel") || selected("toolkit/lgl_scheduler_manager"))
        plan.push_back(step("cachyos_copr2", "Enable the kernel-cachyos-addons COPR",
                            {"/usr/bin/dnf", "copr", "enable", "-y",
                             "bieszczaders/kernel-cachyos-addons"}));
    for (const char *key : {"cachyos/kernel", "cachyos/kernel_devel"})
        if (selected(key))
            add(key);
    if (selected("cachyos/kernel") || selected("cachyos/kernel_devel"))
        plan.push_back(step("selinux_bool", "Set the SELinux domain_kernel_load_modules boolean",
                            {"/usr/sbin/setsebool", "-P", "domain_kernel_load_modules", "on"}));

    bool toolkitCopr = selected("toolkit/lgl_scheduler_manager");
    for (const char *key :
         {"toolkit/lgl_dnf_helper", "toolkit/lgl_emoji_picker", "toolkit/lgl_colour_picker",
          "toolkit/lgl_papercutter", "toolkit/lgl_keychron_helper"})
        toolkitCopr = toolkitCopr || selected(key);
    if (toolkitCopr)
        plan.push_back(step("toolkit_copr", "Enable the LGL Toolkit COPR",
                            {"/usr/bin/dnf", "copr", "enable", "-y",
                             "linuxgamerlife/lgl-toolkit"}));
    for (const char *key :
         {"toolkit/lgl_scheduler_manager", "toolkit/lgl_dnf_helper",
          "toolkit/lgl_emoji_picker", "toolkit/lgl_colour_picker", "toolkit/lgl_papercutter",
          "toolkit/lgl_keychron_helper", "toolkit/lgl_powerprofile_manager"})
        if (selected(key))
            add(key);

    if (selected("systools/nm_wait_online"))
        add("systools/nm_wait_online");
    if (selected("systools/clean_cache"))
        add("systools/clean_cache");

    return plan;
}

int estimateDiskMb(const Selection &selection)
{
    int megabytes = 0;
    for (const auto *entry : catalog::allItems())
        if (selection.get(entry->key))
            megabytes += entry->diskMb;

    for (const auto &group : catalog::choiceGroups()) {
        const auto &chosen = selection.choice(group.id);
        for (const auto &option : group.options)
            if (option.value == chosen)
                megabytes += option.diskMb;
    }

    if (needsFlatpak(selection))
        megabytes += 200;
    return megabytes;
}

Check checkFor(const std::string &key, const SystemInfo &system)
{
    const int version = fedora(system);

    static const std::map<std::string, std::string> rpmPackages{
        {"repos/rpmfusion_free", "rpmfusion-free-release"},
        {"repos/rpmfusion_nonfree", "rpmfusion-nonfree-release"},
        {"systools/fastfetch", "fastfetch"},
        {"systools/btop", "btop"},
        {"systools/cmatrix", "cmatrix"},
        {"systools/distrobox", "distrobox"},
        {"systools/timeshift", "timeshift"},
        {"systools/cbonsai", "cbonsai"},
        {"systools/podman", "podman"},
        {"systools/tailscale", "tailscale"},
        {"python/pip", "python3-pip"},
        {"python/pipx", "pipx"},
        {"python/git", "git"},
        {"media/ffmpeg", "ffmpeg"},
        {"media/gst_bad_free_extras", "gstreamer1-plugins-bad-free-extras"},
        {"media/gst_bad_nonfree", "gstreamer1-plugins-ugly"},
        {"media/vlc", "vlc"},
        {"content/obs", "obs-studio"},
        {"gpu/amd/mesa_dri", "mesa-dri-drivers"},
        {"gpu/amd/vulkan_loader", "vulkan-loader"},
        {"gpu/amd/linux_fw", "linux-firmware"},
        {"gaming/kernel_modules_extra", "kernel-modules-extra"},
        {"gaming/steam", "steam"},
        {"gaming/lutris", "lutris"},
        {"gaming/goverlay", "goverlay"},
        {"gaming/wine", "wine"},
        {"gaming/protontricks", "protontricks"},
        {"gaming/heroic", "heroic-games-launcher-bin"},
        {"gaming/faugus", "faugus-launcher"},
        {"virt/virtmanager", "virt-manager"},
        {"virt/libvirt", "libvirt"},
        {"virt/virt_install", "virt-install"},
        {"virt/virt_viewer", "virt-viewer"},
        {"virt/vmcurator", "vm-curator"},
        {"browsers/firefox", "firefox"},
        {"browsers/chromium", "chromium"},
        {"browsers/brave", "brave-browser"},
        {"browsers/vivaldi", "vivaldi-stable"},
        {"browsers/edge", "microsoft-edge-stable"},
        {"browsers/helium", "helium-bin"},
        {"comms/office_calc", "libreoffice-calc"},
        {"comms/office_writer", "libreoffice-writer"},
        {"cachyos/kernel", "kernel-cachyos"},
        {"cachyos/kernel_devel", "kernel-cachyos-devel-matched"},
        {"toolkit/lgl_dnf_helper", "lgl-dnf-helper"},
        {"toolkit/lgl_emoji_picker", "lgl-emoji-picker"},
        {"toolkit/lgl_colour_picker", "lgl-colour-picker"},
        {"toolkit/lgl_papercutter", "lgl-papercutter"},
        {"toolkit/lgl_keychron_helper", "lgl-keychron-helper"},
        {"toolkit/lgl_powerprofile_manager", "lgl-powerprofile-manager"},
        {"comms/lgl_cliamp", "lgl-cliamp"},
    };

    static const std::map<std::string, std::string> flatpakIds{
        {"systools/flatseal", "com.github.tchx84.Flatseal"},
        {"python/zed", "dev.zed.Zed"},
        {"python/github_desktop", "io.github.shiftey.Desktop"},
        {"gaming/protonplus", "com.vysp3r.ProtonPlus"},
        {"gaming/protonup", "net.davidotek.pupgui2"},
        {"browsers/chrome", "com.google.Chrome"},
        {"browsers/librewolf", "io.gitlab.librewolf-community"},
        {"browsers/waterfox", "net.waterfox.waterfox"},
        {"browsers/zen", "app.zen_browser.zen"},
        {"browsers/floorp", "one.ablaze.floorp"},
        {"comms/discord", "com.discordapp.Discord"},
        {"systools/parsec", "com.parsecgaming.parsec"},
        {"comms/telegram", "org.telegram.desktop"},
        {"comms/spotify", "com.spotify.Client"},
    };

    if (key == "systools/tldr" || key == "content/ytdlp") {
        Check check;
        check.any = {{"/usr/bin/pipx", "list", "--short"}};
        check.outputContains = key == "systools/tldr" ? "tldr" : "yt-dlp";
        return check;
    }
    if (key == "gaming/epic" || key == "comms/artcraft" || key == "content/audacity") {
        // AppImage filenames carry the release version, so match by pattern
        // rather than by an exact name. find prints the match, and the checker
        // treats any output as installed.
        const char *pattern = key == "gaming/epic"        ? "*epic*launcher*.AppImage"
                              : key == "comms/artcraft"   ? "*artcraft*.AppImage"
                                                          : "*audacity*.AppImage";
        const char *binary = key == "gaming/epic"      ? "*epic*launcher*"
                             : key == "comms/artcraft" ? "*artcraft*"
                                                       : "*audacity*";

        const std::string applications =
            (system.targetHome.empty() ? "/home/" + system.targetUser : system.targetHome) +
            "/Applications/";
        Check check;
        check.any.push_back({"/usr/bin/find", applications, "-maxdepth", "1", "-iname", pattern,
                             "-executable", "-print", "-quit"});
        check.any.push_back(
            {"/usr/bin/find", "/usr/bin", "-maxdepth", "1", "-iname", binary, "-print", "-quit"});
        check.any.push_back({"/usr/bin/find", "/usr/local/bin", "-maxdepth", "1", "-iname", binary,
                             "-print", "-quit"});
        check.outputContains = "/";
        return check;
    }
    if (key == "systools/cbonsai") {
        // cbonsai is often built from source or installed outside rpm.
        Check check;
        check.any.push_back({"/usr/bin/rpm", "-q", "--quiet", "cbonsai"});
        check.any.push_back({"/usr/bin/test", "-x", "/usr/bin/cbonsai"});
        check.any.push_back({"/usr/bin/test", "-x", "/usr/local/bin/cbonsai"});
        check.any.push_back(
            {"/usr/bin/test", "-x", "/home/" + system.targetUser + "/.local/bin/cbonsai"});
        return check;
    }
    if (key == "systools/moonlight" || key == "systools/sunshine") {
        const char *id = key == "systools/moonlight" ? "com.moonlight_stream.Moonlight"
                                                     : "dev.lizardbyte.app.Sunshine";
        const char *package = key == "systools/moonlight" ? "moonlight" : "sunshine";
        Check check;
        check.any.push_back({"/usr/bin/rpm", "-q", "--quiet", package});
        check.any.push_back({"/usr/bin/flatpak", "info", "--system", id});
        check.any.push_back({"/usr/bin/flatpak", "info", "--user", id});
        return check;
    }

    if (key == "toolkit/lgl_scheduler_manager") {
        Check check;
        for (const char *package : {"lgl-scheduler-manager", "lgl-scxctl-manager"})
            check.any.push_back({"/usr/bin/rpm", "-q", "--quiet", package});
        return check;
    }
    if (key == "gpu/amd/mesa_vulkan") {
        Check check;
        check.any.push_back({"/usr/bin/rpm", "-q", "--quiet", mesaVulkanPackage(version)});
        return check;
    }
    if (key == "gpu/amd/mesa_va") {
        Check check;
        check.any.push_back({"/usr/bin/rpm", "-q", "--quiet", mesaVaPackage(version)});
        return check;
    }

    if (key == "systools/nm_wait_online") {
        // The tweak is applied when the unit is disabled, so invert the result.
        Check check;
        check.any = {{"/usr/bin/systemctl", "is-enabled", "--quiet",
                      "NetworkManager-wait-online"}};
        check.negate = true;
        return check;
    }
    if (key == "systools/clean_cache")
        return {};   // An action, not a state; there is nothing to detect.

    Check check;
    if (const auto found = rpmPackages.find(key); found != rpmPackages.end()) {
        check.any.push_back({"/usr/bin/rpm", "-q", "--quiet", found->second});
        return check;
    }
    if (const auto found = flatpakIds.find(key); found != flatpakIds.end()) {
        check.any.push_back({"/usr/bin/flatpak", "info", "--system", found->second});
        check.any.push_back({"/usr/bin/flatpak", "info", "--user", found->second});
        return check;
    }
    return check;
}

} // namespace loadout
