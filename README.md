<div align="center">

# The LGL System Loadout

**Get a fresh Fedora install ready for gaming, content creation, and development without the terminal.**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Fedora](https://img.shields.io/badge/Fedora-43%20%7C%2044%20%7C%2045-blue?logo=fedora&logoColor=white)](https://fedoraproject.org)
[![GTK](https://img.shields.io/badge/GTK-4-blue?logo=gtk&logoColor=white)](https://www.gtk.org)
<p align="center">
  <a href="https://ko-fi.com/G2G3V70LW">
    <img src="https://storage.ko-fi.com/cdn/kofi6.png?v=6" height="36" alt="Buy Me a Coffee at ko-fi.com" />
  </a>
</p>
</div>

---

## Overview

A step-by-step setup wizard for Fedora. Choose what you want from curated lists
and one password prompt covers the package installation. The system update and
the Flatpak update are separate sessions and ask once each.

- Nothing is selected by default, and every step is listed before you start. The
  app never uninstalls anything it added; a selected step may replace a
  conflicting package.
- Every card shows whether that package is already installed before you commit.
- Sources are visible on every card: Fedora, RPM Fusion, a COPR, Flatpak, pipx
  or AppImage. **LGL'S CHOICE** marks the options LinuxGamerLife recommends.
- A Continue button at the bottom left of the content area moves to the next
  step as soon as the current step is ready, including after the installation
  completes. It is hidden on Welcome and Setup Complete.
- Installs only: dnf, Flatpak, COPRs, pipx, and AppImages in `~/Applications`.

The System Update step comes first, and the remaining steps stay hidden until it
has run or you have confirmed skipping it. The current package release is
**3.0.0-1**.

---

## Install

### COPR (recommended)

```bash
sudo dnf copr enable linuxgamerlife/lgl-toolkit
sudo dnf install lgl-system-loadout
```

Launch **LGL System Loadout** from your application menu.

> **COPR move:** the app moved from the dedicated
> `linuxgamerlife/lgl-system-loadout` COPR to `linuxgamerlife/lgl-toolkit`.
> Existing installations continue to update after enabling the new repository;
> the old one can then be removed with
> `sudo dnf copr disable linuxgamerlife/lgl-system-loadout`.

### RPM from Releases (no terminal)

Download the RPM matching your Fedora release from the
[latest GitHub release](https://github.com/linuxgamerlife/lgl-system-loadout/releases/latest)
and open it in Discover.

- `lgl-system-loadout-3.0.0-1.fc45.x86_64.rpm` (Fedora 45)
- Use the matching `fc43` or `fc44` build on older releases.

After installing from Discover, close it and launch the app from your
application menu rather than from the Discover install screen.

### Polkit

KDE, Workstation and the spins bring their own polkit agent. A minimal install
without a desktop environment needs one: Noctalia's built-in agent lives under
`Settings → Security → Polkit`.

---

## What's included

| Step | Highlights |
|---|---|
| **Welcome and System Update** | Fedora version, target account, and the recommended full update before anything else |
| **Repositories** | RPM Fusion Free and NonFree |
| **Essentials** | One file manager (Dolphin or Nemo) and one terminal (Konsole, Kitty, Foot or Ghostty) |
| **System Tools** | btop, fastfetch, cmatrix, cbonsai, podman, tldr, distrobox, timeshift, Flatseal, plus two tweaks |
| **Remote Access** | Parsec, Moonlight, Sunshine (Flatpak), Tailscale |
| **Development Tools** | pip, pipx, git, Zed (Flatpak or the zed.dev build), GitHub Desktop, opencode, pi |
| **Multimedia** | ffmpeg, GStreamer plugins, VLC |
| **Content Creation** | OBS Studio, Audacity 4.0.1 (AppImage), yt-dlp, FilmCraft and SoundCraft through the ArtCraft launcher |
| **GPU Drivers** | AMD: Mesa, Vulkan, VA-API and firmware |
| **Gaming** | Steam, Lutris, Wine, Protontricks, GOverlay, controller support, Heroic, Epic, Faugus, ProtonPlus, ProtonUp-Qt |
| **Virtualisation** | virt-manager, libvirt, virt-install, virt-viewer, LGL VM Curator |
| **Browsers** | Vivaldi, Helium, Chromium, Edge, Brave, Chrome · Firefox, LibreWolf, Waterfox, Zen, Floorp |
| **Communication** | ArtCraft Launcher, LibreOffice Calc and Writer, Discord, Telegram, Spotify, LGL CLIamp |
| **CachyOS Kernel** | kernel-cachyos, kernel-cachyos-devel-matched |
| **LGL Tool Kit** | Scheduler Manager, DNF Helper, Emoji Picker, Colour Picker, Power Profile Manager, Papercutter, Keychron Helper |
| **Review Selections** | The complete plan and a disk-space check |
| **Install Now** | Explicit Start Installation gate, progress, output, and per-step results |
| **Setup Complete** | Summary, failed steps, and the reboot option |

AppImages are downloaded atomically as your normal user into `~/Applications`,
verified against pinned SHA-256 checksums where the vendor provides one, and
made executable. Audacity and ArtCraft have x86_64 and aarch64 assets; Epic is
x86_64 only. They are not managed by dnf, so they update themselves.

---

## Disable NetworkManager-wait-online

The System Tools step can disable `NetworkManager-wait-online`, which otherwise
adds several seconds to every boot while the network comes up. It is safe for
most desktops, but **keep it enabled** in these cases:

| Scenario | Keep enabled? |
|---|---|
| Normal gaming desktop | No |
| Ethernet or Wi-Fi | No |
| NAS mounts (NFS/CIFS) at boot | Yes |
| Remote home directories | Yes |
| Network-based authentication at login | Yes |
| Servers with network-dependent startup services | Yes |
| Docker/Podman containers | No |
| VPN connections at boot | Yes |

---

## Build from source

The application is written with GTK 4 (gtkmm) and libadwaita.

```bash
# Build dependencies
sudo dnf install cmake gcc-c++ ninja-build pkgconf-pkg-config \
    gtkmm4.0-devel libadwaita-devel glibmm2.68-devel json-glib-devel glib2-devel \
    desktop-file-utils appstream

`appstream` and `desktop-file-utils` are only needed for the validation steps;
the app itself needs the GTK, GLib and JSON-GLib development packages.

# Clone and build
git clone https://github.com/linuxgamerlife/lgl-system-loadout.git
cd lgl-system-loadout
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) covers the source layout, the tests,
source archives and the privileged helper, and records the app's deviations from
the LGL GTK standard.

The Qt implementation that shipped up to 2.1.3 is kept in `legacy/qt/` for
reference and is not built.

---

## License

MIT. See [LICENSE](LICENSE) for details. Product names and logos belong to their
respective owners and are used only to identify the applications this wizard
installs. Bundled application icons and their sources are listed in
[data/icons/ATTRIBUTION.md](data/icons/ATTRIBUTION.md).

---

<div align="center">
Made for <a href="https://fedoraproject.org">Fedora</a> · by <a href="https://www.youtube.com/@linuxgamerlife">LinuxGamerLife</a>
</div>
