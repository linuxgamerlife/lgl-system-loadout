<div align="center">

# LGL System Loadout

**A curated Fedora setup wizard for gaming, content creation, and development.**  
Pick your loadout. Hit install. Your system is ready.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Fedora](https://img.shields.io/badge/Fedora-43%2B-blue?logo=fedora&logoColor=white)](https://fedoraproject.org)
[![Qt](https://img.shields.io/badge/Qt-6-green?logo=qt&logoColor=white)](https://www.qt.io)
[![KDE Plasma](https://img.shields.io/badge/KDE-Plasma-1d99f3?logo=kde&logoColor=white)](https://kde.org)

</div>

---

## What's new in 1.0.1

Special thanks to **Mojibake.d** for testing 1.0.0 and reporting the bugs that made this release happen 🙌

### Fixed
- **Panel Colorizer download failing** — the temp directory path contained spaces which broke the bash download step. Fixed to use a clean `/tmp` path.
- **Panel Colorizer log showing garbled text** — binary output from the `file` command was being written to the log window. Removed; the log now shows only download progress and file size.
- **KZones download failing** — same space-in-path issue as Panel Colorizer. Fixed.
- **Kernel warning box overlapping log output** — on the System Update page, the kernel update warning box rendered on top of the log when a new kernel was detected. Fixed by correcting the layout order.
- **Reboot confirmation dialog** — "Unsaved work will be lost" was unclear. Now explicitly states to save open files in other applications before rebooting, and that the wizard will need to be run again after.

### Added
- **Secure Boot warning on the CachyOS Kernel page** — a clear warning that Secure Boot must be disabled in BIOS/UEFI after installing the CachyOS kernel, with guidance on how to do it.

---

## Installation

### Recommended — COPR (Fedora 43)

```bash
sudo dnf copr enable linuxgamerlife/lgl-system-loadout
sudo dnf install lgl-system-loadout
```

After installation the app appears in your KDE launcher under **Utilities** as **LGL System Loadout**. Launch it and a single password prompt will appear — the wizard then runs fully elevated.

### Build from source

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel
unzip lgl-system-loadout-1.0.1.zip
cd lgl-system-loadout-1.0.1
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo ./lgl-system-loadout
```

> **Important:** Build on the machine you intend to run it on. A binary built against a newer Qt6 than your system has will fail with `version 'Qt_6.10' not found`. If you see this, run `sudo dnf upgrade qt6-qtbase` first, or build from source locally.

---

## Requirements

| | |
|---|---|
| **OS** | Fedora 43+ |
| **Desktop** | KDE Plasma |
| **Connection** | Internet required during install |

---

## What's included

| Category | Highlights |
|---|---|
| **System Update** | Guided `dnf upgrade --refresh` with kernel detection and reboot prompt |
| **Repositories** | RPM Fusion Free & NonFree |
| **System Tools** | btop, fastfetch, distrobox, timeshift, xrdp, and more |
| **System Tweaks** | Disable NetworkManager-wait-online · Clean DNF cache after install |
| **Python** | pip, pipx, yt-dlp, tldr |
| **Multimedia** | ffmpeg, GStreamer plugins, VLC |
| **Content Creation** | OBS Studio, Kdenlive, GIMP, Inkscape, Audacity, Blender |
| **GPU Drivers** | AMD (Mesa, Vulkan, VA-API, firmware) |
| **Gaming** | Steam, Lutris, Wine, Protontricks, MangoHud, vkBasalt, GOverlay, Heroic, ProtonUp-Qt, ProtonPlus, Flatseal |
| **Virtualisation** | virt-manager, libvirt, virt-install, virt-viewer |
| **Browsers** | Firefox, Chromium, Chrome, Brave, Vivaldi, LibreWolf |
| **Communication** | Thunderbird, Discord, Vesktop, Spotify |
| **KDE Theming** | KZones, Panel Colorizer |
| **CachyOS Kernel** | kernel-cachyos, scx-scheds, scx-manager, scx-tools |

---

## Release assets

| File | Description |
|---|---|
| `lgl-system-loadout-1.0.1.zip` | Source code |

---

<div align="center">
Made for <a href="https://fedoraproject.org">Fedora</a> · by <a href="https://www.youtube.com/@linuxgamerlife">LinuxGamerLife</a>
</div>
