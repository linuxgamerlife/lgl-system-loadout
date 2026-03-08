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

## Overview

LGL System Loadout is a graphical wizard that gets a fresh Fedora install ready in minutes. Choose exactly what you want from a curated list of packages across gaming, multimedia, content creation, development tools, browsers, communication apps, GPU drivers, virtualisation, KDE theming, and the CachyOS kernel.

- No defaults — nothing is pre-selected
- Every item shows its current installed state before you commit
- All checks run concurrently so pages load instantly
- Installs only — nothing is removed without your knowledge

---

## Requirements

| | |
|---|---|
| **OS** | Fedora 43+ (developed and tested on Fedora 43) |
| **Desktop** | KDE Plasma (some items are KDE-specific) |
| **Connection** | Internet required |

---

## Building from source

### 1. Install build dependencies

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel qt6-qtbase-concurrent
```

### 2. Extract and enter the project folder

```bash
mkdir -p ~/projects
mv ~/Downloads/lgl-system-loadout.zip ~/projects/
cd ~/projects
unzip lgl-system-loadout.zip
cd lgl-gui-installer
```

### 3. Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 4. Install

```bash
sudo make install
```

### 5. Run

```bash
pkexec lgl-system-loadout
```

---

## What's included

| Category | Highlights |
|---|---|
| **Repositories** | RPM Fusion Free & NonFree, full system upgrade |
| **System Tools** | git, btop, fastfetch, distrobox, timeshift, and more |
| **Python** | python3, pip, pipx, yt-dlp, tldr |
| **Multimedia** | ffmpeg, GStreamer plugins, LAME, VA-API, VLC |
| **Content Creation** | OBS Studio, Kdenlive, GIMP, Inkscape, Audacity, Blender |
| **GPU Drivers** | AMD (Mesa, Vulkan, VA-API, ROCm) · NVIDIA (guided) |
| **Gaming** | Steam, Lutris, Wine, MangoHud, gamemode, gamescope, Heroic, ProtonUp-Qt |
| **Virtualisation** | virt-manager, QEMU/KVM, edk2-ovmf, swtpm |
| **Browsers** | Firefox, Chromium, Chrome, Brave, Vivaldi |
| **Communication** | Thunderbird, Discord, Vesktop, Spotify |
| **KDE Theming** | Kvantum, KZones, Panel Colorizer |
| **CachyOS Kernel** | kernel-cachyos, scx-scheds, scx-manager, scx-tools |

---

## License

MIT — see [LICENSE](LICENSE) for details.

---

<div align="center">
Made for <a href="https://fedoraproject.org">Fedora</a> · by <a href="https://www.youtube.com/@linuxgamerlife">LinuxGamerLife</a>
</div>
