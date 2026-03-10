Name:           lgl-system-loadout
Version:        1.0.1
Release:        1%{?dist}
Summary:        Guided setup wizard for Fedora — gaming, content creation, and development

License:        MIT
URL:            https://github.com/linuxgamerlife/fedora-gui-installer
Source0:        https://github.com/linuxgamerlife/fedora-gui-installer/releases/download/v%{version}/lgl-system-loadout-%{version}.zip

BuildRequires:  cmake >= 3.16
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel
BuildRequires:  unzip

# Runtime dependencies
# qt6-qtbase is the only true library dependency — all other tools (dnf, curl,
# flatpak) are invoked as subprocesses and are already present on any Fedora 43
# system. Declaring them here causes unnecessary resolver conflicts (e.g. dnf vs dnf5).
Requires:       qt6-qtbase
Requires:       polkit

%description
LGL System Loadout is a graphical wizard that gets a fresh Fedora install
ready in minutes. Choose exactly what to install from a curated list spanning
gaming, multimedia, content creation, development tools, browsers,
communication apps, GPU drivers, virtualisation, KDE theming, and the
CachyOS kernel.

Features:
  - No defaults — nothing is pre-selected
  - Every item shows its current installed state before you commit
  - All checks run concurrently so pages load instantly
  - Installs only — nothing is removed without your knowledge
  - Single polkit password prompt via pkexec

%prep
%setup -q -n lgl-system-loadout-1.0.1

%build
mkdir -p build
cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=%{_prefix}
%make_build

%install
# Binary
install -Dm755 build/lgl-system-loadout \
    %{buildroot}%{_bindir}/lgl-system-loadout

# Desktop entry
install -Dm644 packaging/lgl-system-loadout.desktop \
    %{buildroot}%{_datadir}/applications/lgl-system-loadout.desktop

# Polkit policy
install -Dm644 packaging/lgl-system-loadout.policy \
    %{buildroot}%{_datadir}/polkit-1/actions/com.linuxgamerlife.lgl-system-loadout.policy

# Icons — hicolor theme sizes
install -Dm644 packaging/lgl-system-loadout-256.png \
    %{buildroot}%{_datadir}/icons/hicolor/256x256/apps/lgl-system-loadout.png
install -Dm644 packaging/lgl-system-loadout-128.png \
    %{buildroot}%{_datadir}/icons/hicolor/128x128/apps/lgl-system-loadout.png
install -Dm644 packaging/lgl-system-loadout-64.png \
    %{buildroot}%{_datadir}/icons/hicolor/64x64/apps/lgl-system-loadout.png
install -Dm644 packaging/lgl-system-loadout-48.png \
    %{buildroot}%{_datadir}/icons/hicolor/48x48/apps/lgl-system-loadout.png

# Pixmaps fallback (some older launchers use this)
install -Dm644 packaging/lgl-system-loadout-256.png \
    %{buildroot}%{_datadir}/pixmaps/lgl-system-loadout.png

# AppStream metainfo (Discover uses this for version display and app info)
install -Dm644 packaging/lgl-system-loadout.metainfo.xml \
    %{buildroot}%{_datadir}/metainfo/lgl-system-loadout.metainfo.xml


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
%{_datadir}/applications/lgl-system-loadout.desktop
%{_datadir}/metainfo/lgl-system-loadout.metainfo.xml
%{_datadir}/polkit-1/actions/com.linuxgamerlife.lgl-system-loadout.policy
%{_datadir}/icons/hicolor/256x256/apps/lgl-system-loadout.png
%{_datadir}/icons/hicolor/128x128/apps/lgl-system-loadout.png
%{_datadir}/icons/hicolor/64x64/apps/lgl-system-loadout.png
%{_datadir}/icons/hicolor/48x48/apps/lgl-system-loadout.png
%{_datadir}/pixmaps/lgl-system-loadout.png

%changelog
* Tue Mar 10 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 1.0.1-1
- Panel Colorizer download fixed: QTemporaryDir path contained spaces breaking bash variable expansion
- Panel Colorizer log output no longer shows garbled binary text from file(1) command
- KZones download path fixed: same space-in-path issue as Panel Colorizer
- Kernel warning box no longer overlaps log output on System Update page
- Reboot confirmation dialog clarified: explains unsaved work means other open applications
- Secure Boot warning added to CachyOS Kernel page
- Qt version compatibility note added to README
- Thread lifecycle, atomic cancel flag, QProcess timeout hardening
- PKEXEC_UID and target user input validation

* Mon Mar 09 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 1.0.0-1
- Initial release
