#include "core/catalog.h"

#include <map>
#include <set>

namespace loadout::catalog {
namespace {

constexpr const char *kFree = "repos/rpmfusion_free";
constexpr const char *kNonFree = "repos/rpmfusion_nonfree";

Item entry(std::string key, std::string label, std::string description, const int diskMb,
           std::vector<Requirement> requirements = {}, const bool showBadge = true)
{
    return Item{std::move(key), std::move(label), std::move(description), diskMb, showBadge,
                std::move(requirements)};
}

const std::vector<Page> &build()
{
    static const std::vector<Page> pages{
        {"repositories",
         "Repositories",
         "Select which repositories to enable. RPM Fusion is required for most multimedia, "
         "gaming, and codec packages.",
         {{"Repositories",
           {
               entry("repos/rpmfusion_free", "RPM Fusion Free",
                     "Free and open-source software not in the official Fedora repositories. "
                     "Required for ffmpeg, VLC, codecs, and more.",
                     1),
               entry("repos/rpmfusion_nonfree", "RPM Fusion NonFree",
                     "Proprietary and non-free software, including Steam and certain codecs.",
                     1),
           }}}},

        {"system-tools",
         "System Tools & Utilities",
         "Essential command-line and system utilities.",
         {{"CLI",
           {
               entry("systools/fastfetch", "fastfetch",
                     "Fast system information tool (neofetch alternative).", 10),
               entry("systools/btop", "btop", "Resource monitor with a rich terminal UI.", 10),
               entry("systools/cmatrix", "cmatrix",
                     "Terminal screensaver in the style of The Matrix.", 10),
               entry("systools/tldr", "tldr",
                     "Simplified man pages with quick examples. Installs pipx first if needed.",
                     5),
               entry("systools/distrobox", "distrobox",
                     "Run other Linux distributions in containers, integrated with your desktop.",
                     10),
               entry("systools/cbonsai", "cbonsai",
                     "Grow procedurally generated bonsai trees in your terminal.", 10),
               entry("systools/podman", "podman",
                     "Daemonless container engine for building and running OCI containers.", 80),
           }},
          {"GUI",
           {
               entry("systools/timeshift", "timeshift",
                     "System restore and snapshot utility.", 10),
               entry("systools/flatseal", "Flatseal",
                     "Graphical tool for managing Flatpak application permissions.", 10),
           }},
          {"System Tweaks",
           {
               entry("systools/nm_wait_online", "Disable NetworkManager-wait-online",
                     "Disables the NetworkManager-wait-online service, which can add several "
                     "seconds to boot time on systems that do not need it.",
                     0),
               entry("systools/clean_cache", "Clean the DNF cache after install",
                     "Runs dnf clean all at the end of the installation to free cached package "
                     "data.",
                     0, {}, false),
           }}}},

        {"remote-access",
         "Remote Access",
         "Stream games, control another machine, or reach your systems from anywhere.",
         {{"Remote Access",
           {
               entry("systools/moonlight", "Moonlight",
                     "Stream games from a Sunshine host on your network.", 60),
               entry("systools/sunshine", "Sunshine",
                     "Game-streaming host. Encodes the desktop for Moonlight clients.", 80),
               entry("systools/parsec", "Parsec",
                     "Low-latency remote desktop for gaming and creative work. Create or join a "
                     "session after signing in.",
                     120),
               entry("systools/tailscale", "Tailscale",
                     "Mesh VPN for reaching your machines from anywhere. Install from the Fedora "
                     "repositories; sign in with tailscale up afterwards.",
                     30),
           }}}},

        {"development",
         "Development Tools",
         "Python, editors, and version control.",
         {{"Python & Package Managers",
           {
               entry("python/pip", "python3-pip", "pip, the Python package installer.", 10),
               entry("python/pipx", "pipx",
                     "Installs Python command-line applications in isolated environments.", 5),
           }},
          {"Zed",
           {
               entry("python/zed", "Zed",
                     "The Flathub build. Simple to manage, but it lags behind zed.dev.", 150),
           },
           {ManualOption{"Zed",
                         "The newest build, installed to ~/.local/bin. It updates itself, and it "
                         "is not managed by dnf or Flatpak.",
                         "curl -f https://zed.dev/install.sh | sh",
                         {{"/usr/bin/test", "-x", "{home}/.local/bin/zed"},
                          {"/usr/bin/test", "-x", "/usr/local/bin/zed"},
                          {"/usr/bin/test", "-x", "/usr/bin/zed"}},
                         true, "zed.dev"}}},
          {"Git",
           {
               entry("python/git", "git",
                     "Distributed version control. Already part of the core tools installed before "
                     "anything else.",
                     10),
               entry("python/github_desktop", "GitHub Desktop",
                     "Graphical client for managing GitHub repositories.", 150),
           }},
          {"AI coding tools",
           {},
           {ManualOption{"opencode",
                         "Terminal-based AI coding agent. Installs to ~/.local/bin and updates "
                         "itself.",
                         "curl -fsSL https://opencode.ai/install | bash",
                         {{"/usr/bin/test", "-x", "{home}/.local/bin/opencode"},
                          {"/usr/bin/test", "-x", "{home}/.opencode/bin/opencode"},
                          {"/usr/bin/test", "-x", "/usr/local/bin/opencode"},
                          {"/usr/bin/test", "-x", "/usr/bin/opencode"}},
                         true, "opencode.ai"},
            ManualOption{"pi",
                         "Minimal agent harness from pi.dev. Installs to ~/.local/bin and "
                         "updates itself.",
                         "curl -fsSL https://pi.dev/install.sh | sh",
                         {{"/usr/bin/test", "-x", "{home}/.local/bin/pi"},
                          {"/usr/bin/test", "-x", "/usr/local/bin/pi"},
                          {"/usr/bin/test", "-x", "/usr/bin/pi"}},
                         false, "pi.dev"}}}}},

        {"multimedia",
         "Multimedia & Codecs",
         "Video, audio, and codec support. RPM Fusion must be enabled for most of these.",
         {{"Codecs & Players",
           {
               entry("media/ffmpeg", "ffmpeg (full, replacing ffmpeg-free)",
                     "Replaces the restricted ffmpeg-free with the full build including all "
                     "codecs. The swap may remove packages that depend on ffmpeg-free.",
                     50, {{kFree, 0}}),
               entry("media/gst_bad_free_extras", "GStreamer bad-free-extras",
                     "Additional bad-free plugins including extra format support.", 10,
                     {{kFree, 0}}),
               entry("media/gst_bad_nonfree", "GStreamer Ugly Plugins",
                     "Non-free GStreamer plugins with additional codec support. Installing "
                     "them may replace conflicting GStreamer packages.",
                     10, {{kFree, 0}}),
               entry("media/vlc", "VLC media player",
                     "Plays virtually any audio or video format.", 80, {{kFree, 0}}),
           }}}},

        {"content",
         "Content Creation",
         "Tools for video, audio, graphics, and 3D creation.",
         {{"From Fedora Repositories",
           {
               entry("content/obs", "OBS Studio",
                     "Screen recording and live streaming.", 120),
               entry("content/audacity", "Audacity 4.0.1",
                     "The official Audacity 4 AppImage, installed to ~/Applications.", 120),
           }},
          {"Command-line",
           {
               entry("content/ytdlp", "yt-dlp",
                     "Audio and video downloader. Installs pipx first if needed.", 20),
           }},
          {"ArtCraft",
           {},
           {ManualOption{"FilmCraft",
                         "For FilmCraft, install the ArtCraft launcher and install it from "
                         "there.",
                         "",
                         {{"/usr/bin/find", "{home}/Applications/", "-maxdepth", "1",
                           "-iname", "*filmcraft*.AppImage", "-executable", "-print", "-quit"}},
                         false, "AppImage", "/"},
            ManualOption{"SoundCraft",
                         "For SoundCraft, install the ArtCraft launcher and install it from "
                         "there.",
                         "",
                         {{"/usr/bin/find", "{home}/Applications/", "-maxdepth", "1",
                           "-iname", "*soundcraft*.AppImage", "-executable", "-print", "-quit"}},
                         false, "AppImage", "/"}}}}},

        {"graphics",
         "GPU Drivers",
         "Select your GPU manufacturer to see the appropriate driver options.",
         {{"AMD",
           {
               entry("gpu/amd/mesa_dri", "mesa-dri-drivers",
                     "Core Mesa DRI drivers, providing OpenGL support for AMD GPUs.", 80),
               entry("gpu/amd/mesa_vulkan", "Mesa Vulkan drivers",
                     "AMD's open-source Vulkan driver. Required for DXVK and VKD3D-Proton.", 50,
                     {{kFree, 44}}),
               entry("gpu/amd/vulkan_loader", "vulkan-loader",
                     "Connects applications to the Vulkan driver.", 5),
               entry("gpu/amd/mesa_va", "VA-API driver",
                     "GPU-accelerated video decode in VLC, Firefox, mpv, and OBS.", 20,
                     {{kFree, 44}}),
               entry("gpu/amd/linux_fw", "linux-firmware",
                     "Firmware for AMD GPUs. Without it the GPU may run at reduced clocks.", 500),
           }}}},

        {"gaming",
         "Gaming",
         "Game launchers, compatibility tools, performance overlays, and utilities.",
         {{"Controller Support",
           {
               entry("gaming/kernel_modules_extra", "Kernel Modules Extra",
                     "Kernel-level support for controllers and input devices. Recommended if "
                     "your controller is not detected in games.",
                     50),
           }},
          {"Game Launchers",
           {
               entry("gaming/steam", "Steam",
                     "Valve's game distribution platform, including Proton for Windows games.",
                     400, {{kNonFree, 0}}),
               entry("gaming/faugus", "Faugus Game Launcher",
                     "Simple launcher for running Windows games through Wine or Proton outside "
                     "Steam.",
                     50),
               entry("gaming/lutris", "Lutris",
                     "Open gaming platform for managing games from GOG, itch.io, and more.", 50),
               entry("gaming/heroic", "Heroic Games Launcher",
                     "Open-source launcher for the Epic Games Store and GOG.", 200),
               entry("gaming/epic", "Epic Games Launcher",
                     "The official Epic Games Launcher AppImage, installed to ~/Applications. "
                     "Available for x86_64 only.",
                     250),
           }},
          {"Windows Compatibility",
           {
               entry("gaming/wine", "Wine",
                     "Compatibility layer for Windows applications. Installs both 64-bit and "
                     "32-bit packages.",
                     300),
               entry("gaming/protontricks", "Protontricks",
                     "Winetricks wrapper for Proton-managed Steam games. Requires Steam to have "
                     "been launched and set up once.",
                     0, {{"gaming/steam", 0}}),
           }},
          {"Proton & Compatibility Tools",
           {
               entry("gaming/protonplus", "ProtonPlus",
                     "Graphical tool for managing Proton-GE, Wine-GE, and other compatibility "
                     "versions.",
                     100),
               entry("gaming/protonup", "ProtonUp-Qt",
                     "Alternative tool for managing Proton-GE and other compatibility versions.",
                     100),
           }},
          {"Performance & Monitoring",
           {
               entry("gaming/goverlay", "GOverlay",
                     "A forked build of GOverlay, added to the LGL Essentials COPR purely for "
                     "convenience. Pulls in MangoHud.",
                     20),
           }}}},

        {"virtualisation",
         "Virtualisation",
         "The KVM/QEMU virtualisation stack and management tools.",
         {{"Virtualisation",
           {
               entry("virt/virtmanager", "virt-manager",
                     "Graphical interface for managing KVM virtual machines.", 30),
               entry("virt/libvirt", "libvirt", "Virtualisation API and daemon.", 50),
               entry("virt/virt_install", "virt-install",
                     "Command-line tool for creating new virtual machines.", 0),
               entry("virt/virt_viewer", "virt-viewer",
                     "Lightweight viewer for VM consoles over SPICE or VNC.", 0),
               entry("virt/vmcurator", "LGL VM Curator",
                     "A fast Rust TUI for managing QEMU/KVM machines. Upstream by Mark Roboff; "
                     "LinuxGamerLife packages it for Fedora.",
                     300),
           }}}},

        {"browsers",
         "Browsers",
         "Web browsers, grouped by engine. Third-party browsers have their repositories added "
         "automatically.",
         {{"Chromium-based",
           {
               entry("browsers/vivaldi", "Vivaldi",
                     "Highly customisable Chromium-based browser.", 423),
               entry("browsers/helium", "Helium",
                     "Privacy-focused, ungoogled Chromium-based browser.", 200),
               entry("browsers/chromium", "Chromium",
                     "Open-source Chrome base. No Google account required.", 300),
               entry("browsers/edge", "Microsoft Edge",
                     "Chromium-based browser with Microsoft account sync.", 400),
               entry("browsers/brave", "Brave",
                     "Privacy-focused browser with built-in ad blocking.", 415),
               entry("browsers/chrome", "Google Chrome",
                     "Full Chrome with Widevine DRM and Google sync.", 200),
           }},
          {"Mozilla-based",
           {
               entry("browsers/firefox", "Firefox",
                     "Mozilla Firefox, ships with Fedora.", 250),
               entry("browsers/librewolf", "LibreWolf",
                     "Firefox fork focused on privacy, security, and freedom.", 300),
               entry("browsers/waterfox", "Waterfox",
                     "Firefox fork focused on speed and privacy.", 300),
               entry("browsers/zen", "Zen",
                     "Firefox-based browser built around vertical tabs and workspaces.", 300),
               entry("browsers/floorp", "Floorp",
                     "Firefox-based browser with extra customisation and workspaces.", 300),
           }}}},

        {"communication",
         "Communication & Productivity",
         "Chat, office, and music applications.",
         {{"Recommended",
           {
               entry("comms/artcraft", "ArtCraft Launcher",
                     "Launcher for the ArtCraft apps: FilmCraft, SoundCraft, and the office apps. "
                     "Installed as an AppImage in ~/Applications.",
                     200),
           }},
          {"Office",
           {
               entry("comms/office_calc", "LibreOffice Calc",
                     "Spreadsheet application, part of LibreOffice.", 250),
               entry("comms/office_writer", "LibreOffice Writer",
                     "Word processor, part of LibreOffice.", 250),
           }},
          {"Chat",
           {
               entry("comms/discord", "Discord",
                     "Voice, video, and text chat for gaming communities.", 200),
               entry("comms/telegram", "Telegram Desktop",
                     "Telegram messaging client.", 150),
           }},
          {"Music",
           {
               entry("comms/spotify", "Spotify",
                     "Spotify music streaming client.", 200),
               entry("comms/lgl_cliamp", "LGL CLIamp",
                     "Customised build of cliamp with a minimal visualizer view and a mirrored "
                     "Classic Peak visualizer. Installs alongside upstream as lgl-cliamp.",
                     20),
           }}}},

        {"kernel",
         "CachyOS Kernel",
         "A performance-optimised kernel with improved scheduling, lower latency, and better "
         "gaming support.",
         {{"Kernel",
           {
               entry("cachyos/kernel", "kernel-cachyos",
                     "The CachyOS performance-optimised kernel. Secure Boot must be disabled. "
                     "Selecting it also enables the SELinux domain_kernel_load_modules boolean "
                     "permanently.",
                     120),
               entry("cachyos/kernel_devel", "kernel-cachyos-devel-matched",
                     "Matching headers, required for DKMS and external modules. Selecting this "
                     "or the kernel also enables the SELinux domain_kernel_load_modules boolean "
                     "permanently.",
                     80),
           }}}},

        {"toolkit",
         "LGL Tool Kit",
         "Small utilities from LinuxGamerLife, distributed through the LGL Toolkit COPR.",
         {{"LGL Tool Kit",
           {
               entry("toolkit/lgl_scheduler_manager", "LGL Scheduler Manager",
                     "Manage sched-ext BPF schedulers: start, stop, switch schedulers, and "
                     "browse per-scheduler flags. Installs scx-tools and scx-scheds if needed; "
                     "that prerequisite may replace conflicting packages.",
                     60),
               entry("toolkit/lgl_dnf_helper", "LGL DNF Helper",
                     "Inspect installed RPM packages and DNF5 dependencies: what a package is, "
                     "why it is installed, and what depends on it.",
                     15),
               entry("toolkit/lgl_emoji_picker", "LGL Emoji Picker",
                     "Emoji picker with search, recent history, and Wayland and X11 clipboard "
                     "support.",
                     15),
               entry("toolkit/lgl_colour_picker", "LGL Colour Picker",
                     "Sample a colour from the screen and copy it in ready-to-paste formats.", 15),
               entry("toolkit/lgl_powerprofile_manager", "LGL Power Profile Manager",
                     "Switch between tuned and power-profiles-daemon profiles.", 15),
               entry("toolkit/lgl_papercutter", "LGL Papercutter",
                     "Resize, position, zoom, and crop wallpapers to fit your display.", 15),
               entry("toolkit/lgl_keychron_helper", "LGL Keychron Helper",
                     "Configure and update supported Keychron devices, including the udev rules.",
                     15),
           }}}},
    };
    return pages;
}

} // namespace

const std::vector<Page> &pages()
{
    return build();
}

const std::vector<ChoiceGroup> &choiceGroups()
{
    static const std::vector<ChoiceGroup> groups{
        {"file-manager",
         "File manager",
         "The desktop's file manager. Pick the one that suits the desktop you use.",
         {
             {"dolphin", "Dolphin", "KDE's file manager, with split views and an embedded "
                                    "terminal panel.", "dolphin", "system-file-manager-symbolic",
              20, true},
             {"nemo", "Nemo", "Cinnamon's file manager, with a tree sidebar and a familiar "
                              "layout.", "nemo", "system-file-manager-symbolic", 15},
         }},
        {"terminal",
         "Terminal",
         "The terminal used for every command-line task on this system.",
         {
             {"konsole", "Konsole", "KDE's terminal emulator, with profiles and split views.",
              "konsole", "utilities-terminal-symbolic", 15, true},
             {"kitty", "Kitty", "GPU-accelerated terminal, fast and scriptable.", "kitty",
              "utilities-terminal-symbolic", 20, true},
             {"foot", "Foot", "Lightweight terminal for Wayland, with very low overhead.",
              "foot", "utilities-terminal-symbolic", 5},
             {"ghostty", "Ghostty", "Fast, native terminal with a modern feature set.",
              "ghostty", "utilities-terminal-symbolic", 20, false, "COPR",
              "scottames/ghostty"},
         }},
    };
    return groups;
}

const Page *page(const std::string &id)
{
    for (const auto &entryPage : pages())
        if (entryPage.id == id)
            return &entryPage;
    return nullptr;
}

const Item *item(const std::string &key)
{
    for (const auto &entryPage : pages())
        for (const auto &section : entryPage.sections)
            for (const auto &candidate : section.items)
                if (candidate.key == key)
                    return &candidate;
    return nullptr;
}

std::vector<const Item *> allItems()
{
    std::vector<const Item *> items;
    for (const auto &entryPage : pages())
        for (const auto &section : entryPage.sections)
            for (const auto &candidate : section.items)
                items.push_back(&candidate);
    return items;
}

std::string labelFor(const std::string &key)
{
    if (const Item *found = item(key))
        return found->label;
    if (key == "repos/rpmfusion_free")
        return "RPM Fusion Free";
    if (key == "repos/rpmfusion_nonfree")
        return "RPM Fusion NonFree";
    return key;
}

std::vector<std::string> iconNamesFor(const std::string &key)
{    static const std::map<std::string, std::vector<std::string>> icons{
        {"choice:file-manager:dolphin", {"dolphin"}},
        {"choice:file-manager:nemo", {"nemo"}},
        {"choice:terminal:konsole", {"konsole"}},
        {"choice:terminal:kitty", {"kitty"}},
        {"choice:terminal:foot", {"foot"}},
        {"choice:terminal:ghostty", {"com.mitchellh.ghostty", "ghostty"}},

        {"manual:Zed", {"dev.zed.Zed", "zed"}},
        {"manual:opencode", {"opencode"}},
        {"manual:pi", {"pi"}},

        {"systools/fastfetch", {"fastfetch"}},
        {"systools/btop", {"btop"}},
        {"systools/cmatrix", {"cmatrix"}},
        {"systools/tldr", {"tldr"}},
        {"systools/distrobox", {"distrobox"}},
        {"systools/cbonsai", {"cbonsai"}},
        {"systools/podman", {"podman"}},
        {"systools/moonlight", {"com.moonlight_stream.Moonlight", "moonlight"}},
        {"systools/sunshine", {"dev.lizardbyte.app.Sunshine", "sunshine"}},
        {"systools/tailscale", {"tailscale"}},
        {"systools/parsec", {"com.parsecgaming.parsec", "parsec"}},
        {"systools/timeshift", {"timeshift"}},
        {"systools/flatseal", {"com.github.tchx84.Flatseal", "flatseal"}},

        {"python/pip", {"python3", "python"}},
        {"python/pipx", {"pipx"}},
        {"python/git", {"git"}},
        {"python/zed", {"dev.zed.Zed", "zed"}},
        {"python/github_desktop", {"io.github.shiftey.Desktop", "github-desktop"}},

        {"media/ffmpeg", {"ffmpeg"}},
        {"media/vlc", {"vlc"}},

        {"content/obs", {"com.obsproject.Studio", "obs"}},
        {"content/audacity", {"audacity"}},
        {"content/ytdlp", {"yt-dlp"}},

        {"gaming/steam", {"steam"}},
        {"gaming/lutris", {"net.lutris.Lutris", "lutris"}},
        {"gaming/wine", {"wine"}},
        {"gaming/protontricks", {"protontricks"}},
        {"gaming/goverlay", {"goverlay"}},
        {"gaming/heroic", {"com.heroicgameslauncher.hgl", "heroic"}},
        {"gaming/epic", {"EpicGamesLauncher", "epicgameslauncher"}},
        {"gaming/protonplus", {"com.vysp3r.ProtonPlus"}},
        {"gaming/protonup", {"net.davidotek.pupgui2"}},
        {"gaming/faugus", {"io.github.Faugus.faugus-launcher", "faugus-launcher"}},

        {"virt/virtmanager", {"virt-manager", "org.virt-manager.virt-manager"}},
        {"virt/virt_viewer", {"virt-viewer", "remote-viewer"}},
        {"virt/vmcurator", {"vm-curator"}},

        {"browsers/firefox", {"firefox"}},
        {"browsers/chromium", {"chromium-browser", "chromium"}},
        {"browsers/chrome", {"google-chrome", "com.google.Chrome"}},
        {"browsers/brave", {"brave-browser", "com.brave.Browser"}},
        {"browsers/vivaldi", {"vivaldi", "vivaldi-stable"}},
        {"browsers/edge", {"microsoft-edge", "microsoft-edge-stable"}},
        {"browsers/helium", {"helium", "helium-bin"}},
        {"browsers/librewolf", {"io.gitlab.librewolf-community", "librewolf"}},
        {"browsers/waterfox", {"net.waterfox.waterfox", "waterfox"}},
        {"browsers/zen", {"app.zen_browser.zen", "zen-browser"}},
        {"browsers/floorp", {"one.ablaze.floorp", "floorp"}},

        {"comms/office_calc", {"libreoffice-calc", "libreoffice"}},
        {"comms/office_writer", {"libreoffice-writer", "libreoffice"}},
        {"comms/discord", {"com.discordapp.Discord", "discord"}},
        {"comms/telegram", {"org.telegram.desktop", "telegram"}},
        {"comms/spotify", {"com.spotify.Client", "spotify"}},
        {"comms/lgl_cliamp", {"lgl-cliamp"}},

        {"cachyos/kernel", {"cachyos"}},
        {"cachyos/kernel_devel", {"cachyos"}},

        {"toolkit/lgl_scheduler_manager", {"lgl-scheduler-manager"}},
        {"toolkit/lgl_dnf_helper", {"lgl-dnf-helper"}},
        {"toolkit/lgl_emoji_picker", {"lgl-emoji-picker"}},
        {"toolkit/lgl_colour_picker", {"lgl-colour-picker"}},
        {"toolkit/lgl_powerprofile_manager", {"lgl-powerprofile-manager"}},
        {"toolkit/lgl_papercutter", {"lgl-papercutter"}},
        {"toolkit/lgl_keychron_helper", {"lgl-keychron-helper"}},
    };

    if (const auto found = icons.find(key); found != icons.end())
        return found->second;
    return {};
}

std::string iconResourceFor(const std::string &key)
{
    if (key == "gaming/epic")
        return "/com/linuxgamerlife/loadout/icons/epic.png";
    if (key == "content/audacity")
        return "/com/linuxgamerlife/loadout/icons/audacity.png";
    if (key == "manual:FilmCraft")
        return "/com/linuxgamerlife/loadout/icons/filmcraft.png";
    if (key == "manual:SoundCraft")
        return "/com/linuxgamerlife/loadout/icons/soundcraft.png";
    if (key == "virt/vmcurator")
        return "/com/linuxgamerlife/loadout/icons/vm-curator.png";
    if (key == "comms/artcraft")
        return "/com/linuxgamerlife/loadout/icons/artcraft.png";
    if (key == "choice:file-manager:dolphin")
        return "/com/linuxgamerlife/loadout/icons/dolphin.png";
    if (key == "choice:terminal:konsole")
        return "/com/linuxgamerlife/loadout/icons/konsole.png";
    if (key == "choice:terminal:kitty")
        return "/com/linuxgamerlife/loadout/icons/kitty.png";
    if (key == "choice:terminal:foot")
        return "/com/linuxgamerlife/loadout/icons/foot.png";
    if (key == "choice:terminal:ghostty")
        return "/com/linuxgamerlife/loadout/icons/ghostty.png";
    if (key == "systools/distrobox")
        return "/com/linuxgamerlife/loadout/icons/distrobox.png";
    if (key == "systools/podman")
        return "/com/linuxgamerlife/loadout/icons/podman.png";
    if (key == "systools/tailscale")
        return "/com/linuxgamerlife/loadout/icons/tailscale.png";
    if (key == "systools/sunshine")
        return "/com/linuxgamerlife/loadout/icons/sunshine.png";
    if (key == "manual:Zed")
        return "/com/linuxgamerlife/loadout/icons/zed.png";
    if (key == "manual:opencode")
        return "/com/linuxgamerlife/loadout/icons/opencode.png";
    if (key == "manual:pi")
        return "/com/linuxgamerlife/loadout/icons/pi.png";
    if (key == "cachyos/kernel")
        return "/com/linuxgamerlife/loadout/icons/cachyos.png";
    if (key == "cachyos/kernel_devel")
        return "/com/linuxgamerlife/loadout/icons/cachyos.png";
    return {};
}

std::string sourceFor(const std::string &key)
{
    static const std::map<std::string, std::string> sources{
        {"systools/fastfetch", "Fedora"},        {"systools/btop", "Fedora"},
        {"systools/cmatrix", "Fedora"},          {"systools/cbonsai", "Fedora"},
        {"systools/distrobox", "Fedora"},        {"systools/timeshift", "Fedora"},
        {"systools/podman", "Fedora"},           {"systools/tailscale", "Fedora"},
        {"systools/tldr", "pipx"},               {"systools/flatseal", "Flatpak"},
        {"systools/moonlight", "Flatpak"},       {"systools/sunshine", "Flatpak"},
        {"systools/parsec", "Flatpak"},

        {"python/pip", "Fedora"},                {"python/pipx", "Fedora"},
        {"python/git", "Fedora"},                {"python/zed", "Flatpak"},
        {"python/github_desktop", "Flatpak"},

        {"media/ffmpeg", "RPM Fusion"},          {"media/gst_bad_free_extras", "RPM Fusion"},
        {"media/gst_bad_nonfree", "RPM Fusion"}, {"media/vlc", "RPM Fusion"},

        {"content/obs", "Fedora"},               {"content/audacity", "AppImage"},
        {"content/ytdlp", "pipx"},

        {"gpu/amd/mesa_dri", "Fedora"},          {"gpu/amd/mesa_vulkan", "Fedora / RPM Fusion"},
        {"gpu/amd/vulkan_loader", "Fedora"},     {"gpu/amd/mesa_va", "Fedora / RPM Fusion"},
        {"gpu/amd/linux_fw", "Fedora"},

        {"gaming/kernel_modules_extra", "Fedora"},
        {"gaming/steam", "RPM Fusion"},          {"gaming/lutris", "Fedora"},
        {"gaming/faugus", "COPR"},               {"gaming/heroic", "COPR"},
        {"gaming/epic", "AppImage"},             {"gaming/wine", "Fedora"},
        {"gaming/protontricks", "Fedora"},       {"gaming/protonplus", "Flatpak"},
        {"gaming/protonup", "Flatpak"},          {"gaming/goverlay", "LGL Essentials COPR"},

        {"virt/virtmanager", "Fedora"},          {"virt/libvirt", "Fedora"},
        {"virt/virt_install", "Fedora"},         {"virt/virt_viewer", "Fedora"},
        {"virt/vmcurator", "COPR"},

        {"browsers/firefox", "Fedora"},          {"browsers/chromium", "Fedora"},
        {"browsers/chrome", "Flatpak"},          {"browsers/brave", "Brave repo"},
        {"browsers/vivaldi", "Vivaldi repo"},    {"browsers/edge", "Microsoft repo"},
        {"browsers/helium", "COPR"},             {"browsers/librewolf", "Flatpak"},
        {"browsers/waterfox", "Flatpak"},        {"browsers/zen", "Flatpak"},
        {"browsers/floorp", "Flatpak"},

        {"comms/office_calc", "Fedora"},         {"comms/office_writer", "Fedora"},
        {"comms/discord", "Flatpak"},            {"comms/telegram", "Flatpak"},
        {"comms/spotify", "Flatpak"},            {"comms/lgl_cliamp", "COPR"},
        {"comms/artcraft", "AppImage"},

        {"cachyos/kernel", "COPR"},              {"cachyos/kernel_devel", "COPR"},

        {"toolkit/lgl_scheduler_manager", "LGL Toolkit COPR"},
        {"toolkit/lgl_dnf_helper", "LGL Toolkit COPR"},
        {"toolkit/lgl_emoji_picker", "LGL Toolkit COPR"},
        {"toolkit/lgl_colour_picker", "LGL Toolkit COPR"},
        {"toolkit/lgl_papercutter", "LGL Toolkit COPR"},
        {"toolkit/lgl_keychron_helper", "LGL Toolkit COPR"},
        {"toolkit/lgl_powerprofile_manager", "COPR"},
    };

    const auto found = sources.find(key);
    return found == sources.end() ? std::string{} : found->second;
}

bool isLglChoice(const std::string &key)
{
    static const std::set<std::string> choices{
        "systools/fastfetch",       "systools/btop",
        "systools/cmatrix",         "systools/cbonsai",
        "systools/podman",          "systools/timeshift",
        "systools/distrobox",
        "systools/flatseal",        "systools/moonlight",
        "systools/sunshine",        "systools/tailscale",
        "systools/nm_wait_online",  "systools/clean_cache",

        "python/git",
        "python/pip",               "python/pipx",

        "media/ffmpeg",             "media/gst_bad_free_extras",
        "media/gst_bad_nonfree",    "media/vlc",

        "content/audacity",         "content/obs",
        "content/ytdlp",

        "gpu/amd/mesa_dri",         "gpu/amd/mesa_vulkan",
        "gpu/amd/vulkan_loader",    "gpu/amd/mesa_va",
        "gpu/amd/linux_fw",

        "gaming/kernel_modules_extra", "gaming/steam",
        "gaming/faugus",
        "gaming/wine",              "gaming/protontricks",
        "gaming/protonplus",        "gaming/goverlay",

        "virt/virtmanager",         "virt/virt_viewer",
        "virt/vmcurator",

        "browsers/vivaldi",

        "comms/discord",            "comms/lgl_cliamp",
        "comms/artcraft",

        "toolkit/lgl_scheduler_manager", "toolkit/lgl_dnf_helper",
        "toolkit/lgl_emoji_picker",      "toolkit/lgl_colour_picker",
        "toolkit/lgl_powerprofile_manager", "toolkit/lgl_papercutter",
        "toolkit/lgl_keychron_helper",
    };
    return choices.count(key) > 0;
}

std::vector<Unmet> unmetRequirements(const Selection &selection, const int fedoraVersion)
{
    std::vector<Unmet> unmet;
    for (const Item *candidate : allItems()) {
        if (!selection.get(candidate->key))
            continue;
        for (const auto &requirement : candidate->requirements) {
            if (fedoraVersion < requirement.minFedora)
                continue;
            if (selection.get(requirement.key))
                continue;
            unmet.push_back({candidate->key, requirement.key});
        }
    }
    return unmet;
}

PageExtras extrasFor(const std::string &pageId, const SystemInfo &system)
{
    PageExtras extras;

    if (pageId == "virtualisation") {
        extras.notes.emplace_back(
            "ENABLE VIRTUALISATION IN YOUR BIOS/UEFI",
            "For the best performance, enable hardware virtualisation (Intel VT-x/VT-d or "
            "AMD-V/AMD-Vi) in your firmware, usually by pressing F2, F10, F12 or Del at startup. "
            "Without it, virtual machines run significantly slower or fail to start.",
            true);
        extras.notes.emplace_back(
            "AUTOMATIC SETUP",
            "virt-manager, libvirt and virt-install enable libvirtd and add " +
                system.targetUser +
                " to the libvirt group. VM Curator installs its runtime support and adds " +
                system.targetUser +
                " to the kvm group. virt-viewer installs independently and starts no service. "
                "Log out and back in for group changes to take effect.");
    }

    if (pageId == "kernel") {
        extras.notes.emplace_back(
            "ABOUT THE CACHYOS KERNEL",
            "A performance-optimised kernel with the BORE scheduler, LATENCY-NICE patches, BBRv3 "
            "TCP, FUTEX2/winesync and upstream performance patches. The Fedora kernel stays "
            "installed; CachyOS appears as an additional GRUB entry.");
        extras.notes.emplace_back(
            "SECURE BOOT MUST BE DISABLED",
            "The CachyOS kernel is not signed with a Microsoft-trusted key. If Secure Boot is "
            "enabled, the system will refuse to boot the new kernel. After installing and "
            "rebooting, enter your BIOS/UEFI settings and disable Secure Boot, then choose the "
            "CachyOS kernel from the GRUB menu.",
            true);
    }

    if (pageId == "gaming") {
        extras.notes.emplace_back(
            "FLATPAK",
            "Flatpak and the Flathub remote are configured automatically if any Flatpak item is "
            "selected here or elsewhere in this wizard.");
        if (!system.architecture.empty() && system.architecture != "x86_64")
            extras.notes.emplace_back(
                "EPIC GAMES LAUNCHER",
                "The Epic Games Launcher AppImage is available for x86_64 only, so it is not "
                "offered on this system.");
    }

    if (pageId == "content")
        extras.notes.emplace_back(
            "PACKAGE SOURCES",
            "yt-dlp is installed with pipx, which is installed first if needed.");

    return extras;
}

} // namespace loadout::catalog
