Name:           lgl-system-loadout
Version:        3.0.0
Release:        1%{?dist}
Summary:        Guided setup wizard for Fedora: gaming, content creation, and development

License:        MIT
URL:            https://github.com/linuxgamerlife/lgl-system-loadout
# Generated from the checked-out commit by `make source` or `make srpm`.
# `legacy/qt/` is excluded from the archive; COPR SCM builds need no Git tag.
Source0:        %{name}-%{version}.tar.gz

# The root CMakeLists.txt builds the maintained GTK 4 application and its
# privileged helper from src/.

BuildRequires:  cmake >= 3.20
BuildRequires:  cmake-rpm-macros
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  pkgconfig(gtkmm-4.0) >= 4.20
BuildRequires:  pkgconfig(libadwaita-1) >= 1.6
BuildRequires:  pkgconfig(glibmm-2.68)
BuildRequires:  pkgconfig(json-glib-1.0)
BuildRequires:  glib2-devel
BuildRequires:  desktop-file-utils
BuildRequires:  appstream

# Runtime dependencies
# The GTK and GLib libraries are resolved from the binary's sonames. All the
# tools this wizard runs (dnf, flatpak, curl) are invoked through the
# privileged helper and are already present on any Fedora 43+ system.
Requires:       polkit

%description
LGL System Loadout gets a fresh Fedora install ready for gaming, content
creation, and development - without the terminal. Pick exactly what you want
from a curated list, click Install, and you are done.

Nothing is selected by default. Every item shows whether it is already
installed before you commit. One password prompt covers the package
installation; the system update and Flatpak update are separate sessions.

%prep
%autosetup

%build
%cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
%cmake_build

%install
%cmake_install

%check
%ctest
desktop-file-validate %{buildroot}%{_datadir}/applications/com.linuxgamerlife.%{name}.desktop
appstreamcli validate --no-net %{buildroot}%{_datadir}/metainfo/com.linuxgamerlife.%{name}.metainfo.xml

%post
if [ -x /usr/bin/gtk-update-icon-cache ]; then
    gtk-update-icon-cache -f -t %{_datadir}/icons/hicolor &>/dev/null || :
fi
if [ -x /usr/bin/update-desktop-database ]; then
    update-desktop-database -q %{_datadir}/applications &>/dev/null || :
fi
if [ -x /usr/bin/appstreamcli ]; then
    appstreamcli refresh --force &>/dev/null || :
fi
# Reload polkit so the new policy is picked up immediately without requiring
# a reboot or manual polkit restart.
if [ -x /usr/bin/systemctl ]; then
    systemctl reload polkit &>/dev/null || systemctl restart polkit &>/dev/null || :
fi

%postun
if [ -x /usr/bin/gtk-update-icon-cache ]; then
    gtk-update-icon-cache -f -t %{_datadir}/icons/hicolor &>/dev/null || :
fi
if [ -x /usr/bin/update-desktop-database ]; then
    update-desktop-database -q %{_datadir}/applications &>/dev/null || :
fi
if [ -x /usr/bin/appstreamcli ]; then
    appstreamcli refresh --force &>/dev/null || :
fi

%files
%license LICENSE
%doc README.md CHANGELOG.md HELP.md data/icons/ATTRIBUTION.md
%{_bindir}/%{name}
%{_libexecdir}/%{name}/%{name}-helper
%{_datadir}/applications/com.linuxgamerlife.%{name}.desktop
%{_datadir}/metainfo/com.linuxgamerlife.%{name}.metainfo.xml
%{_datadir}/polkit-1/actions/com.linuxgamerlife.%{name}.run-helper.policy
%{_datadir}/icons/hicolor/48x48/apps/%{name}.png
%{_datadir}/icons/hicolor/64x64/apps/%{name}.png
%{_datadir}/icons/hicolor/128x128/apps/%{name}.png
%{_datadir}/icons/hicolor/256x256/apps/%{name}.png

%changelog
* Sun Oct 25 2026 LinuxGamerLife - 3.0.0-1
- Rewrite the interface in GTK 4 (gtkmm) with libadwaita theming
- Follow the desktop theme live, including the monospace font and dark mode
- Add both title bar modes, Help, About, and a settings file
- Move the Qt implementation to legacy/qt as historical reference
- Drop the Qt runtime dependency; dnf, flatpak and curl still run through the
  privileged helper after a single pkexec prompt
- Add the v3 content set: Essentials, Remote Access, Git, Zed, ArtCraft, LGL
  CLIamp, browser grouping, Telegram, opencode and pi
- Add Refresh, Select All and Select None to Essentials
- Add an Install Now gate, a Continue footer, and lock the update page while
  it is running
- Harden the privileged helper with a private session socket, a per-session
  nonce, a restricted allow-list, and bounded buffers
- Install Ghostty from the scottames/ghostty COPR
- Make AppImage downloads atomic, checksum-verified where available, and
  architecture-aware
- Fix the About dialog to a compact fixed size and wrap its credits
- Bundle the GUI application icons and expose them through an icon theme
  resource path, so cards show real artwork on a fresh system
- Reload the application stylesheet and watch GtkSettings so live theme and
  accent changes update the app's custom colours
- Build-require appstream so metainfo validation works in minimal build roots

* Thu Oct 08 2026 LinuxGamerLife - 2.1.3-1
- Install LGL Tool Kit apps from the shared linuxgamerlife/lgl-toolkit COPR
- Rename LGL SCXCTL Manager to LGL Scheduler Manager (lgl-scheduler-manager)
- Add LGL Keychron Helper to LGL Tool Kit
- Report the correct application version

* Wed Oct 07 2026 LinuxGamerLife - 2.1.2-1
- Ensure libvirt client selections install the local QEMU daemon runtime before
  enabling libvirtd or adding the target user to the libvirt group
- Install QEMU's SDL display module with VM Curator

* Tue Oct 06 2026 LinuxGamerLife - 2.1.1-1
- Fixed standalone VM Curator installation by adding its common Fedora runtime
  support and the target user to the kvm group
- Limited libvirt service and group setup to libvirt-based tools
- Stopped virt-viewer-only installations from enabling the local libvirt daemon

* Sun Sep 13 2026 LinuxGamerLife - 2.1.0-1
- Added Microsoft Edge and Helium to Browsers
- Added cbonsai and podman to System Tools
- Added LGL Papercutter to LGL Tool Kit
- Added VM Curator to Virtualisation
- Removed KineticWE page (no longer supported)

* Sun Jul 12 2026 LinuxGamerLife - 2.0.3-1
- Fixed Flatpak installs failing for Flatseal, Zed, and GitHub Desktop: their
  install steps ran before the Flathub remote was added
- Fixed Tenacity never actually installing despite being selectable
- Fixed reboot buttons on the Update and Done pages not triggering polkit -
  they now go through the app's own privileged helper instead of a raw,
  unmonitored pkexec call
- Fixed Flatpak install-detection relying on PATH resolution instead of an
  absolute path, and not recognizing Fedora's ESR-branded Thunderbird build
  as already installed

* Sun Jul 12 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 2.0.2-1
- Fixed "could not connect to helper socket" error when accepting the Flatpak
  update prompt on the System Update page: the privileged helper only ever
  accepts one client connection for its whole process lifetime and had
  already shut down after the system update step; the Flatpak update step
  now launches its own fresh helper session, same as other phases already do

* Sun Jul 12 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 2.0.1-1
- Fixed app icon losing transparency (background baked in as opaque white
  instead of transparent when regenerating the 48/64/128/256 sizes)

* Sun Jul 12 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 2.0.0-1
- New "LGL Tool Kit" page (LGL SCXCTL Manager, LGL DNF Helper, LGL Emoji Picker,
  LGL Colour Picker, LGL Power Profile Manager), replacing the standalone SCX
  Scheduler Tools page: scx-tools/scx-scheds now only install as a
  prerequisite of LGL SCXCTL Manager
- New "KineticWE" page (COPR), with an IMPORTANT notice that it obsoletes
  stock kwin/kwin-common/kwin-libs and installs alongside the current DE
- "Python & CLI Dev Tools" page renamed to "Development Tools"; added Zed and
  GitHub Desktop (Flatpak)
- tldr moved to System Tools, yt-dlp moved to Content Creation; pipx now
  installs automatically if either is selected
- System Tools split into CLI/GUI sections, added cmatrix, Flatseal moved in
  from Gaming page
- Faugus Game Launcher added to Gaming (COPR); ProtonPlus is now the primary
  Proton tool with ProtonUp-Qt as the alternative
- Tenacity added to Content Creation; LibreOffice Calc/Writer added to
  Communication & Productivity; Chromium moved above Firefox on Browsers page
- Flatpak update step added to System Update page with a Yes/No prompt
- BIOS/UEFI virtualisation warning added to Virtualisation page
- Fixed: Reboot Now button on Done page did nothing (silent pkexec failure)
- Fixed: mesa-vulkan-drivers now uses mesa-vulkan-drivers-freeworld (RPM
  Fusion) on Fedora 44+, matching mesa-va-drivers
- New app icon; desktop file and spec icon references cleaned up

* Mon Mar 30 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 1.1.2-1
- Fedora 44 compatibility
- Bootstrap step now installs python3-dnf5-plugins instead of dnf-plugins-core (DNF5/F43+ correction)

* Sun Mar 22 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 1.1.1-1
- Added kernel-modules-extra for controller support on Gaming page
- Fixed Flatpak progress output in log
- Fixed clipboard buttons on Done page (Wayland)
- Fixed graphical artifact during Flatpak installs
- Improved first launch reliability after install from Discover

* Tue Mar 17 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 1.1.0-1
- Privilege separation: GUI now runs as normal user at all times
- New privileged helper binary (lgl-system-loadout-helper) launched via pkexec
- Helper uses Unix socket IPC with strict operation allow-list
- No shell invocation: all commands executed via direct execvp-style QProcess
- polkit prompt deferred until Install is clicked (or Update Now on update page)
- SCX Scheduler Tools split into dedicated page after CachyOS Kernel page
- Heroic Games Launcher switched from Flatpak to atim/heroic-games-launcher COPR
- kernel-cachyos-addons COPR now enabled when any SCX package is selected
- Browser repo setup (Chrome, Brave, Vivaldi) no longer uses bash heredocs
- allowedExitCodes formalised per-step (dnf exit 7, scx exit 1)
- Target user validation strengthened
- Welcome page no longer requires root to proceed

* Tue Mar 10 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 1.0.4-1
- Metainfo file renamed to reverse-DNS format to match component ID and desktop file

* Tue Mar 10 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 1.0.3-1
- Desktop file renamed to reverse-DNS format (com.linuxgamerlife.lgl-system-loadout.desktop)
- AppStream metadata now passes validation; app appears correctly in Discover

* Tue Mar 10 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 1.0.2-1
- Fixed AppStream metadata: component ID corrected to match desktop file name
- Discover/GNOME Software can now find and display the application

* Tue Mar 10 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 1.0.1-1
- Panel Colorizer and KZones download path fixes (removed in 1.1.0)
- Kernel warning box layout fix
- Reboot confirmation dialog clarified
- Secure Boot warning added to CachyOS Kernel page
- Thread lifecycle and atomic cancel flag hardening
- PKEXEC_UID and target user input validation

* Mon Mar 09 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 1.0.0-1
- Initial release
