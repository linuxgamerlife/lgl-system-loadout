<div align="center">

# LGL System Loadout

**Get a fresh Fedora install ready for gaming, content creation, and development — without the terminal.**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Fedora](https://img.shields.io/badge/Fedora-43%2B-blue?logo=fedora&logoColor=white)](https://fedoraproject.org)
[![Qt](https://img.shields.io/badge/Qt-6-green?logo=qt&logoColor=white)](https://www.qt.io)

</div>

---

## Overview

LGL System Loadout is a graphical setup wizard for Fedora. Pick exactly what you want from a curated list of software across gaming, multimedia, content creation, development, browsers, communication, GPU drivers, virtualisation, and the CachyOS kernel. One password prompt covers the entire installation.

- Nothing is selected by default — every choice is yours
- Every item shows its current installed state before you commit
- Installs only — nothing is removed without your knowledge

---

## Install

### Recommended — COPR

```bash
sudo dnf copr enable linuxgamerlife/lgl-system-loadout
sudo dnf install lgl-system-loadout
```

Launch **LGL System Loadout** from your application menu.

### No Terminal — RPM from Releases

Download the latest `.rpm` from [GitHub Releases](https://github.com/linuxgamerlife/lgl-system-loadout/releases) and double-click to install via Discover.

> After installing from Discover, close it and launch the app from your application menu rather than from the Discover install screen.

---

## Build from Source

```bash
# Install build dependencies
sudo dnf install cmake gcc-c++ qt6-qtbase-devel

# Clone and build
git clone https://github.com/linuxgamerlife/lgl-system-loadout.git
cd lgl-system-loadout
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

> Always build on the same machine you intend to run on. If you see `version 'Qt_6.x' not found`, run `sudo dnf upgrade qt6-qtbase` first.

---

## What's Included

| Category | Highlights |
|---|---|
| **System Update** | Optional `dnf upgrade --refresh` before installing |
| **Repositories** | RPM Fusion Free & NonFree |
| **System Tools** | btop, fastfetch, distrobox, timeshift, xrdp |
| **System Tweaks** | Disable NetworkManager-wait-online · Clean DNF cache |
| **Python** | pip, pipx, yt-dlp, tldr |
| **Multimedia** | ffmpeg, GStreamer plugins, VLC |
| **Content Creation** | OBS Studio, Kdenlive, GIMP, Inkscape, Audacity, Blender |
| **GPU Drivers** | AMD (Mesa, Vulkan, VA-API) |
| **Gaming** | Steam, Lutris, Wine, Protontricks, MangoHud, vkBasalt, GOverlay, Heroic, ProtonUp-Qt, ProtonPlus, Flatseal |
| **Virtualisation** | virt-manager, libvirt |
| **Browsers** | Firefox, Chromium, Chrome, Brave, Vivaldi, LibreWolf |
| **Communication** | Thunderbird, Discord, Vesktop, Spotify |
| **CachyOS Kernel** | kernel-cachyos, kernel-cachyos-devel-matched |
| **SCX Scheduler Tools** | scx-scheds, scx-manager, scx-tools |

---

## License

MIT — see [LICENSE](LICENSE) for details.

---

<div align="center">
Made for <a href="https://fedoraproject.org">Fedora</a> · by <a href="https://www.youtube.com/@linuxgamerlife">LinuxGamerLife</a>
</div>
