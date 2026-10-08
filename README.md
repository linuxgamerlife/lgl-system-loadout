<div align="center">

# The LGL System Loadout

**Get a fresh Fedora install ready for gaming, content creation, and development without the terminal.**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Fedora](https://img.shields.io/badge/Fedora-43%20%7C%2044-blue?logo=fedora&logoColor=white)](https://fedoraproject.org)
[![Qt](https://img.shields.io/badge/Qt-6-green?logo=qt&logoColor=white)](https://www.qt.io)
<p align="center">
  <a href="https://ko-fi.com/G2G3V70LW">
    <img src="https://storage.ko-fi.com/cdn/kofi6.png?v=6" height="36" alt="Buy Me a Coffee at ko-fi.com" />
  </a>
</p>
</div>

---

## Overview

LGL System Loadout is a graphical setup wizard for Fedora. Pick exactly what you want from a curated list of software across gaming, multimedia, content creation, development, browsers, communication, GPU drivers, virtualisation, and the CachyOS kernel. One password prompt covers the entire installation.

- Nothing is selected by default; every choice is yours
- Every item shows its current installed state before you commit
- Installs only; nothing is removed without your knowledge

---

## Install

### Recommended: COPR

```bash
sudo dnf copr enable linuxgamerlife/lgl-toolkit
sudo dnf install lgl-system-loadout
```

Launch **LGL System Loadout** from your application menu.

> **COPR repository move:** LGL System Loadout has moved from the dedicated
> `linuxgamerlife/lgl-system-loadout` COPR to `linuxgamerlife/lgl-toolkit` so
> the LGL applications can be maintained and distributed together from one
> repository. Existing installations continue to update after enabling the new
> repository. The old repository can then be disabled with
> `sudo dnf copr disable linuxgamerlife/lgl-system-loadout`.

### No Terminal: RPM from Releases

Download the Fedora 44 RPM from the [latest GitHub release](https://github.com/linuxgamerlife/lgl-system-loadout/releases/latest) and double-click it to install via Discover.

- `lgl-system-loadout-2.1.3-1.fc44.x86_64.rpm` for Fedora 44

> After installing from Discover, close it and launch the app from your application menu rather than from the Discover install screen.

### Polkit
If you are using this in KDE, Workstation or another spin, then authentication will be baked in. If you are building out your Fedora install without a Desktop Environment to start and using Noctalia as your shell, you will need a polkit service. I would recommend the Noctalia built in one which is in in Noctalia  `Settings > Security > Polkit`

---

## Build from Source

```bash
# Install the Qt runtime
sudo dnf install qt6-qtbase

# Install build dependencies
sudo dnf install cmake gcc-c++ qt6-qtbase-devel

# Clone and build
git clone https://github.com/linuxgamerlife/lgl-system-loadout.git
cd lgl-system-loadout
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## What's Included

| Category | Highlights |
|---|---|
| **System Update** | Optional `dnf upgrade --refresh` before installing |
| **Repositories** | RPM Fusion Free & NonFree |
| **System Tools** | btop, fastfetch, htop, xrdp, cmatrix, cbonsai, podman, tldr, distrobox, timeshift, Flatseal |
| **System Tweaks** | Disable NetworkManager-wait-online · Clean DNF cache |
| **Development Tools** | pip, pipx, Zed, GitHub Desktop |
| **Multimedia** | ffmpeg, GStreamer plugins, VLC |
| **Content Creation** | OBS Studio, Kdenlive, GIMP, Inkscape, Audacity, Tenacity, Blender, yt-dlp |
| **GPU Drivers** | AMD (Mesa, Vulkan, VA-API) |
| **Gaming** | Steam, Lutris, Wine, Protontricks, MangoHud, vkBasalt, GOverlay, Controller Support, Heroic, Faugus, ProtonPlus, ProtonUp-Qt |
| **Virtualisation** | virt-manager, libvirt, virt-install, virt-viewer, VM Curator |
| **Browsers** | Chromium, Firefox, Chrome, Brave, Vivaldi, Microsoft Edge, Helium, LibreWolf |
| **Communication & Productivity** | LibreOffice Calc, LibreOffice Writer, Thunderbird, Discord, Vesktop, Spotify |
| **CachyOS Kernel** | kernel-cachyos, kernel-cachyos-devel-matched |
| **LGL Tool Kit** | LGL Scheduler Manager, LGL DNF Helper, LGL Emoji Picker, LGL Colour Picker, LGL Power Profile Manager, LGL Papercutter, LGL Keychron Helper |

---

## License

MIT. See [LICENSE](LICENSE) for details.

---

<div align="center">
Made for <a href="https://fedoraproject.org">Fedora</a> · by <a href="https://www.youtube.com/@linuxgamerlife">LinuxGamerLife</a>
</div>
