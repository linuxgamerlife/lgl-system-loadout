Name:           lgl-system-loadout
Version:        1.1.1
Release:        1%{?dist}
Summary:        Guided setup wizard for Fedora — gaming, content creation, and development

License:        MIT
URL:            https://github.com/linuxgamerlife/lgl-system-loadout
Source0:        %{name}-%{version}.zip
BuildRequires:  cmake >= 3.16
BuildRequires:  unzip
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel
# Runtime dependencies
# qt6-qtbase is the only true library dependency — all other tools (dnf, curl,
# flatpak) are invoked via the privileged helper and are already present on any
# Fedora 43 system. Declaring them here causes unnecessary resolver conflicts.
Requires:       qt6-qtbase
Requires:       polkit

%description
LGL System Loadout gets a fresh Fedora install ready for gaming, content
creation, and development - without the terminal. Pick exactly what you want
from a curated list, click Install, and you are done.

Nothing is selected by default. Every item shows whether it is already
installed before you commit. One password prompt covers the entire
installation.

%prep
cd %{_builddir}
unzip -q %{SOURCE0}
%setup -q -n lgl-system-loadout -D -T

%build
mkdir -p build
cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=%{_prefix}
%make_build

%install
# Main GUI binary — runs as normal user
install -Dm755 build/lgl-system-loadout \
    %{buildroot}%{_bindir}/lgl-system-loadout

# Privileged helper binary — launched via pkexec, never run directly
install -Dm755 build/lgl-system-loadout-helper \
    %{buildroot}%{_libexecdir}/lgl-system-loadout/lgl-system-loadout-helper

# Desktop entry
install -Dm644 packaging/com.linuxgamerlife.lgl-system-loadout.desktop \
    %{buildroot}%{_datadir}/applications/com.linuxgamerlife.lgl-system-loadout.desktop

# Polkit policy — references the helper binary path
install -Dm644 packaging/lgl-system-loadout.policy \
    %{buildroot}%{_datadir}/polkit-1/actions/com.linuxgamerlife.lgl-system-loadout.run-helper.policy

# Icons — hicolor theme sizes
install -Dm644 packaging/lgl-system-loadout-256.png \
    %{buildroot}%{_datadir}/icons/hicolor/256x256/apps/lgl-system-loadout.png
install -Dm644 packaging/lgl-system-loadout-128.png \
    %{buildroot}%{_datadir}/icons/hicolor/128x128/apps/lgl-system-loadout.png
install -Dm644 packaging/lgl-system-loadout-64.png \
    %{buildroot}%{_datadir}/icons/hicolor/64x64/apps/lgl-system-loadout.png
install -Dm644 packaging/lgl-system-loadout-64.png \
    %{buildroot}%{_datadir}/icons/hicolor/64x64/apps/lgl-system-loadout-64.png
install -Dm644 packaging/lgl-system-loadout-48.png \
    %{buildroot}%{_datadir}/icons/hicolor/48x48/apps/lgl-system-loadout.png

# Pixmaps fallback
install -Dm644 packaging/lgl-system-loadout-256.png \
    %{buildroot}%{_datadir}/pixmaps/lgl-system-loadout.png

# AppStream metainfo
install -Dm644 packaging/com.linuxgamerlife.lgl-system-loadout.metainfo.xml \
    %{buildroot}%{_datadir}/metainfo/com.linuxgamerlife.lgl-system-loadout.metainfo.xml

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
if [ -x /usr/bin/kbuildsycoca6 ]; then
    kbuildsycoca6 &>/dev/null || :
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
%{_bindir}/lgl-system-loadout
%{_libexecdir}/lgl-system-loadout/lgl-system-loadout-helper
%{_datadir}/applications/com.linuxgamerlife.lgl-system-loadout.desktop
%{_datadir}/metainfo/com.linuxgamerlife.lgl-system-loadout.metainfo.xml
%{_datadir}/polkit-1/actions/com.linuxgamerlife.lgl-system-loadout.run-helper.policy
%{_datadir}/icons/hicolor/256x256/apps/lgl-system-loadout.png
%{_datadir}/icons/hicolor/128x128/apps/lgl-system-loadout.png
%{_datadir}/icons/hicolor/64x64/apps/lgl-system-loadout.png
%{_datadir}/icons/hicolor/64x64/apps/lgl-system-loadout-64.png
%{_datadir}/icons/hicolor/48x48/apps/lgl-system-loadout.png
%{_datadir}/pixmaps/lgl-system-loadout.png

%changelog
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
