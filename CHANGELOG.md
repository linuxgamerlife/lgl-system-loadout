# Changelog

---

## [3.0.0] - 2026-10-25

### Changed
- The interface is now built with GTK 4 (gtkmm) and follows the desktop theme, including dark mode, the accent colour, and the desktop monospace font
- The app no longer needs the Qt runtime; the package name and install flow are unchanged
- The window can use either the app title bar or the compositor's title bar, toggled with Ctrl+Shift+M and remembered between runs
- Source archives and the SRPM now build the GTK implementation from the repository root and exclude the archived Qt tree
- Only the Welcome and System Update steps are offered until the update has run or has been explicitly skipped, and skipping now asks for confirmation
- Browsers are grouped by engine: Chromium-based, then Mozilla-based
- Zed has its own section with both the Flatpak and the zed.dev install routes

### Added
- Help window (F1) and About dialog
- A settings file at `~/.config/lgl-system-loadout/settings.ini`
- `LOADOUT_GTK_SNAPSHOT` for rendering windows to PNG in tests and reviews
- Essentials step: one file manager (Dolphin or Nemo) and one terminal (Konsole, Kitty, Foot or Ghostty)
- Moonlight and Sunshine as Flatpaks, and Tailscale from the Fedora repositories, under Remote Access
- git, GitHub Desktop, opencode and pi on Development Tools
- Waterfox, Zen and Floorp on Browsers
- Epic Games Launcher, Audacity 4.0.1 and the ArtCraft launcher as AppImages in `~/Applications`, run as the normal user rather than through the helper
- Every card carries a source pill (Fedora, RPM Fusion, COPR, Flatpak, pipx, AppImage) beside its installed state, and LinuxGamerLife's picks carry an LGL'S CHOICE chip
- Telegram on Communication, the ArtCraft launcher as an AppImage, and pointers to FilmCraft and SoundCraft
- LGL CLIamp on Communication & Productivity
- Ghostty in Essentials, installed from the `scottames/ghostty` COPR because Fedora does not package it
- Refresh, Select All and Select None on Essentials
- Bundled icons for the GUI applications in the catalogue, exposed through an icon theme resource path so cards show real artwork before the application is installed

### Removed
- Qt 6 build and runtime dependencies
- htop, xrdp, Kdenlive, GIMP, Inkscape, Blender, Tenacity, Thunderbird, Vesktop, MangoHud and vkBasalt

### Fixed
- The Installing step is now Install Now, and opens a review gate; nothing starts and no password prompt appears until Start Installation is pressed
- The System Update page locks navigation while it is running, so an install cannot start concurrently
- Ctrl+Page Down and Ctrl+Page Up no longer skip past progressive disclosure to hidden steps
- AppImages download to a `.part` file, verify pinned SHA-256 checksums where the vendor provides one, publish with an atomic rename, use the aarch64 asset on aarch64, and leave the old AppImage intact on failure
- AppImage downloads are cancelled when the window closes instead of blocking the process for up to 15 minutes
- The target account's real home directory is used instead of a hardcoded `/home/<user>`, and `SUDO_USER` is ignored unless the process runs as root
- Helper and command output buffers are capped, `runCommand` cannot hang after a child closes stdout, and no allocation happens between fork and exec
- Copy now discloses the CachyOS SELinux boolean, package replacements, and the separate update and installation password sessions
- The Flatpak follow-up prompt is visible above the log, and a Continue button at the bottom left of the content area appears whenever the current step is ready, including after the system update and after the installation; it is hidden on Welcome and Setup Complete
- The update page explains that the first repository refresh can take several minutes
- The About dialog is a fixed 640x520 popup, and its credits and legal text wrap instead of stretching the dialog to the window width
- The application stylesheet is reloaded and GtkSettings theme changes are watched, so live theme and accent changes update the app's custom colours
- The spec and CI install `appstream` for metainfo validation in minimal build roots

### Security
- The privileged helper now requires a one-time session nonce, and its socket is owned by the pkexec caller with mode 0600
- The helper allow-list is restricted to the COPR projects and download hosts the catalogue uses; option-like package names are rejected, and user-scoped commands must target the authenticated caller
- The helper path environment override was removed from release builds, and the session directory is now removed recursively

### Note
- The Qt implementation that shipped up to 2.1.3 is preserved in `legacy/qt/` as historical reference and is not built or packaged

## [2.1.3] - 2026-10-08

### Added
- LGL Keychron Helper added to LGL Tool Kit

### Changed
- LGL Tool Kit apps now install from the shared `linuxgamerlife/lgl-toolkit` COPR instead of one COPR per app (LGL Power Profile Manager stays on its own COPR for now)
- LGL SCXCTL Manager renamed to LGL Scheduler Manager (`lgl-scheduler-manager`), following the upstream rename

### Fixed
- The app now reports its real version (it was hardcoded to 1.1.2)

## [2.1.2] - 2026-10-07

### Fixed
- Selecting virt-manager or virt-install now installs Fedora's local libvirt QEMU runtime before enabling libvirtd or adding the target user to the libvirt group
- VM Curator now installs QEMU's SDL display module so SDL-configured VMs can launch

## [2.1.1] - 2026-10-06

### Fixed
- VM Curator can now be installed on its own: its common Fedora runtime support is installed explicitly, the target user is added to the `kvm` group, and unrelated libvirt setup is no longer run
- Selecting virt-viewer alone no longer enables the local libvirt daemon or changes libvirt group membership

## [2.1.0] - 2026-09-13

### Added
- Microsoft Edge and Helium added to Browsers (Edge via Microsoft's official repo, Helium via COPR `imput/helium`)
- cbonsai and podman added to System Tools
- LGL Papercutter (wallpaper editor) added to LGL Tool Kit, installed via COPR
- VM Curator (LinuxGamerLife's fork) added to Virtualisation - TUI for building/managing QEMU/KVM VMs with 3D acceleration, installed via COPR

### Removed
- KineticWE page - KWE is no longer supported

---

## [2.0.3] - 2026-07-12

### Fixed
- Flatpak installs failing for Flatseal, Zed, and GitHub Desktop - their install steps ran before the Flathub remote was added to the install queue
- Tenacity never actually installing despite being selectable on the Content Creation page
- Reboot buttons on the Update and Done pages not triggering polkit - they now go through the app's own privileged helper instead of a raw, unmonitored `pkexec` call with no error handling
- Flatpak install-detection relying on `PATH` resolution instead of an absolute path, and not recognizing Fedora's ESR-branded Thunderbird build (`org.mozilla.thunderbird_esr`) as already installed

---

## [2.0.2] - 2026-07-12

### Fixed
- "could not connect to helper socket" error when accepting the Flatpak update prompt on the System Update page - the privileged helper only ever accepts one client connection for its whole process lifetime and had already shut down after the system update step finished. The Flatpak update step now launches its own fresh helper session, same as other phases (Update, Install) already do - this means a second password prompt when accepting the Flatpak update, which is expected given the app's "no credential caching" policy

---

## [2.0.1] - 2026-07-12

### Fixed
- App icon lost transparency when regenerated at 48/64/128/256 - the resize/crop step baked in an opaque white background instead of preserving transparency
- Spec `Source0` now points at GitHub's tag archive (`archive/refs/tags/v%{version}.tar.gz`) instead of a manually-built release zip, simplifying `%prep` to `%autosetup`

---

## [2.0.0] - 2026-07-12

### Added
- **LGL Tool Kit page** - LGL SCXCTL Manager, LGL DNF Helper, LGL Emoji Picker, LGL Colour Picker, LGL Power Profile Manager, all installed via their own COPR repos
- **KineticWE page** - tiling KWin Wayland compositor via COPR, with an IMPORTANT notice that it obsoletes stock `kwin`/`kwin-common`/`kwin-libs` and installs alongside your current desktop environment
- BIOS/UEFI virtualisation warning on the Virtualisation page
- Zed and GitHub Desktop added to Development Tools (Flatpak) - Zed includes a copy-pasteable official install command and detects a dev-site (`~/.local/bin/zed`) install separately from the Flatpak
- Faugus Game Launcher added to Gaming (COPR)
- Tenacity added to Content Creation (Flatpak, Audacity fork)
- LibreOffice Calc and Writer added to Communication & Productivity
- cmatrix added to System Tools
- Flatpak update step added to the System Update page, with a Yes/No prompt before running it

### Changed
- "Python & CLI Dev Tools" page renamed to "Development Tools"
- tldr moved to System Tools (CLI section); yt-dlp moved to Content Creation - pipx now installs automatically if either is selected, even without its own checkbox
- System Tools split into CLI and GUI sections; Flatseal moved here from the Gaming page
- ProtonPlus is now the primary Proton management tool, with ProtonUp-Qt as the alternative
- Chromium moved above Firefox on the Browsers page
- Removed the standalone "SCX Scheduler Tools" page - `scx-tools`/`scx-scheds` are now only installed as a prerequisite of LGL SCXCTL Manager
- Default window size increased to 1100x1000
- App icon regenerated at all sizes; desktop file and spec icon references cleaned up

### Fixed
- Reboot Now button on the Done page did nothing due to a silent `pkexec` failure
- `mesa-vulkan-drivers` now correctly uses the RPM Fusion `mesa-vulkan-drivers-freeworld` variant on Fedora 44+, matching `mesa-va-drivers`
- Newly added Flatpak items were not wired into the Flatpak-infrastructure/disk-estimate checks
- Removed a dead CMake test target referencing a deleted test file

---

## [1.1.2] - 2026-03-30

### Changed
- Bootstrap step now installs `python3-dnf5-plugins` instead of `dnf-plugins-core` - correct DNF5 package for Fedora 43+ (DNF4 plugins package was unused and potentially absent on Fedora 44)
- Bootstrap step uses `dnf5-plugins` on Fedora 44+ (`python3-dnf5-plugins` was renamed)
- Version detection fallback updated to Fedora 44
- AMD GPU page uses `mesa-va-drivers-freeworld` (RPM Fusion) on Fedora 44+ - `mesa-va-drivers` was replaced in F44

---

## [1.1.1] - 2026-03-22

### Added
- Controller Support section on Gaming page - kernel-modules-extra for controller and input device support

### Fixed
- Flatpak progress output no longer spams individual percentage lines in the log
- Clipboard buttons on Done page now work reliably on Wayland
- Graphical artifact (patience message overlapping log) during Flatpak installs removed
- First launch from Discover post-install - silent retry with 1.5s delay if pkexec not immediately available

---

## [1.1.0] - 2026-03-19

### Security
- **Privilege separation** - GUI runs as normal user. Privileged operations handled by a dedicated helper binary (`lgl-system-loadout-helper`) launched via pkexec. Single password prompt at Install time.
- **Helper allow-list** - every operation validated against a strict allow-list before execution. No arbitrary command execution possible.
- **No shell invocation** - all commands executed via direct `QProcess` with explicit argument lists.
- **Absolute paths** - all program calls use full paths (e.g. `/usr/bin/dnf`). No PATH lookup.
- **Socket IPC** - helper communicates via a randomised Unix socket in `/run/lgl-XXXXXX/`. Path delivered to GUI via stdout.

### Added
- `src/helper/` - privileged helper binary (Unix socket server, allow-list validation, process execution)
- SCX Scheduler Tools page - dedicated page after CachyOS Kernel
- Welcome page preflight check - clear message if helper/policy not found on first launch from Discover

### Changed
- System Update page restored - live log, kernel detection, reboot prompt. Runs via helper on its own session; Install gets a fresh session.
- Heroic Games Launcher - switched from Flatpak to `atim/heroic-games-launcher` COPR
- Google Chrome - switched from dnf repo to Flatpak (`com.google.Chrome`)
- Theming page removed - app now works on any Fedora desktop environment
- Done page reboot - uses `pkexec /usr/bin/systemctl reboot`
- Install worker - single persistent socket connection across all steps
- RPM `%post` - reloads polkit after install so policy is active immediately

### Fixed
- DNF5 `config-manager` syntax updated (`addrepo --from-repofile`)
- `pipx` commands use full path `/usr/bin/pipx`
- `dnf clean all` allow-list match corrected
- Flatpak progress `\r` handling - progress updates in place rather than spamming new lines

---

## [1.0.4] - 2026-03-10
- Metainfo file renamed to reverse-DNS format

## [1.0.3] - 2026-03-10
- Desktop file renamed to reverse-DNS format for AppStream validation

## [1.0.2] - 2026-03-10
- AppStream component ID corrected

## [1.0.1] - 2026-03-10

Special thanks to **Mojibake.d** for being the first to report bugs in 1.0.0 🙌

- Panel Colorizer and KZones download path fixes (spaces in temp path)
- Kernel warning box layout fix
- Reboot confirmation dialog clarified
- Secure Boot warning added to CachyOS page
- Thread lifecycle, atomic cancel flag, QProcess timeout hardening
- Data race fix (`m_cancelled` → `std::atomic<bool>`)
- Target user input validation strengthened
- Test suite added

## [1.0.0] - 2026-03-09
- pkexec / polkit support - single password prompt
- System Update page with live log and kernel detection
- LibreWolf added as Flatpak
- Thunderbird switched to Flatpak
- Wine installs both `wine` and `wine.i686`
- Virtualisation page simplified
- NetworkManager-wait-online and DNF cache cleanup moved to opt-in tweaks

## [0.1.0] - Initial release
- Qt6 wizard covering gaming, multimedia, content creation, development, GPU drivers, virtualisation, browsers, communication, and CachyOS kernel
