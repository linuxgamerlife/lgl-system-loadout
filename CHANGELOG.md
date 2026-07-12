# Changelog

---

## [2.0.0] — 2026-07-12

### Added
- **LGL Tool Kit page** — LGL SCXCTL Manager, LGL DNF Helper, LGL Emoji Picker, LGL Colour Picker, LGL Power Profile Manager, all installed via their own COPR repos
- **KineticWE page** — tiling KWin Wayland compositor via COPR, with an IMPORTANT notice that it obsoletes stock `kwin`/`kwin-common`/`kwin-libs` and installs alongside your current desktop environment
- BIOS/UEFI virtualisation warning on the Virtualisation page
- Zed and GitHub Desktop added to Development Tools (Flatpak) — Zed includes a copy-pasteable official install command and detects a dev-site (`~/.local/bin/zed`) install separately from the Flatpak
- Faugus Game Launcher added to Gaming (COPR)
- Tenacity added to Content Creation (Flatpak, Audacity fork)
- LibreOffice Calc and Writer added to Communication & Productivity
- cmatrix added to System Tools
- Flatpak update step added to the System Update page, with a Yes/No prompt before running it

### Changed
- "Python & CLI Dev Tools" page renamed to "Development Tools"
- tldr moved to System Tools (CLI section); yt-dlp moved to Content Creation — pipx now installs automatically if either is selected, even without its own checkbox
- System Tools split into CLI and GUI sections; Flatseal moved here from the Gaming page
- ProtonPlus is now the primary Proton management tool, with ProtonUp-Qt as the alternative
- Chromium moved above Firefox on the Browsers page
- Removed the standalone "SCX Scheduler Tools" page — `scx-tools`/`scx-scheds` are now only installed as a prerequisite of LGL SCXCTL Manager
- Default window size increased to 1100x1000
- App icon regenerated at all sizes; desktop file and spec icon references cleaned up

### Fixed
- Reboot Now button on the Done page did nothing due to a silent `pkexec` failure
- `mesa-vulkan-drivers` now correctly uses the RPM Fusion `mesa-vulkan-drivers-freeworld` variant on Fedora 44+, matching `mesa-va-drivers`
- Newly added Flatpak items were not wired into the Flatpak-infrastructure/disk-estimate checks
- Removed a dead CMake test target referencing a deleted test file

---

## [1.1.2] — 2026-03-30

### Changed
- Bootstrap step now installs `python3-dnf5-plugins` instead of `dnf-plugins-core` — correct DNF5 package for Fedora 43+ (DNF4 plugins package was unused and potentially absent on Fedora 44)
- Bootstrap step uses `dnf5-plugins` on Fedora 44+ (`python3-dnf5-plugins` was renamed)
- Version detection fallback updated to Fedora 44
- AMD GPU page uses `mesa-va-drivers-freeworld` (RPM Fusion) on Fedora 44+ — `mesa-va-drivers` was replaced in F44

---

## [1.1.1] — 2026-03-22

### Added
- Controller Support section on Gaming page — kernel-modules-extra for controller and input device support

### Fixed
- Flatpak progress output no longer spams individual percentage lines in the log
- Clipboard buttons on Done page now work reliably on Wayland
- Graphical artifact (patience message overlapping log) during Flatpak installs removed
- First launch from Discover post-install — silent retry with 1.5s delay if pkexec not immediately available

---

## [1.1.0] — 2026-03-19

### Security
- **Privilege separation** — GUI runs as normal user. Privileged operations handled by a dedicated helper binary (`lgl-system-loadout-helper`) launched via pkexec. Single password prompt at Install time.
- **Helper allow-list** — every operation validated against a strict allow-list before execution. No arbitrary command execution possible.
- **No shell invocation** — all commands executed via direct `QProcess` with explicit argument lists.
- **Absolute paths** — all program calls use full paths (e.g. `/usr/bin/dnf`). No PATH lookup.
- **Socket IPC** — helper communicates via a randomised Unix socket in `/run/lgl-XXXXXX/`. Path delivered to GUI via stdout.

### Added
- `src/helper/` — privileged helper binary (Unix socket server, allow-list validation, process execution)
- SCX Scheduler Tools page — dedicated page after CachyOS Kernel
- Welcome page preflight check — clear message if helper/policy not found on first launch from Discover

### Changed
- System Update page restored — live log, kernel detection, reboot prompt. Runs via helper on its own session; Install gets a fresh session.
- Heroic Games Launcher — switched from Flatpak to `atim/heroic-games-launcher` COPR
- Google Chrome — switched from dnf repo to Flatpak (`com.google.Chrome`)
- Theming page removed — app now works on any Fedora desktop environment
- Done page reboot — uses `pkexec /usr/bin/systemctl reboot`
- Install worker — single persistent socket connection across all steps
- RPM `%post` — reloads polkit after install so policy is active immediately

### Fixed
- DNF5 `config-manager` syntax updated (`addrepo --from-repofile`)
- `pipx` commands use full path `/usr/bin/pipx`
- `dnf clean all` allow-list match corrected
- Flatpak progress `\r` handling — progress updates in place rather than spamming new lines

---

## [1.0.4] — 2026-03-10
- Metainfo file renamed to reverse-DNS format

## [1.0.3] — 2026-03-10
- Desktop file renamed to reverse-DNS format for AppStream validation

## [1.0.2] — 2026-03-10
- AppStream component ID corrected

## [1.0.1] — 2026-03-10

Special thanks to **Mojibake.d** for being the first to report bugs in 1.0.0 🙌

- Panel Colorizer and KZones download path fixes (spaces in temp path)
- Kernel warning box layout fix
- Reboot confirmation dialog clarified
- Secure Boot warning added to CachyOS page
- Thread lifecycle, atomic cancel flag, QProcess timeout hardening
- Data race fix (`m_cancelled` → `std::atomic<bool>`)
- Target user input validation strengthened
- Test suite added

## [1.0.0] — 2026-03-09
- pkexec / polkit support — single password prompt
- System Update page with live log and kernel detection
- LibreWolf added as Flatpak
- Thunderbird switched to Flatpak
- Wine installs both `wine` and `wine.i686`
- Virtualisation page simplified
- NetworkManager-wait-online and DNF cache cleanup moved to opt-in tweaks

## [0.1.0] — Initial release
- Qt6 wizard covering gaming, multimedia, content creation, development, GPU drivers, virtualisation, browsers, communication, and CachyOS kernel
