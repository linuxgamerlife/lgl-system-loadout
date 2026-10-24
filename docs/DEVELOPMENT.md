# Development layout

The repository root builds the maintained GTK 4 application. Application and
shared C++ code live in `src/`, GResource inputs and CSS live in `data/`, and
tests live in `tests/`.

```text
src/core/     no widget headers; unit tested; linked by the app and the tests
src/ui/       windows, pages, the card grid, and the theme watcher
src/helper/   the privileged helper, launched only through pkexec
data/         style.css, HELP.md, the GResource manifest, and bundled icons
tests/        one executable per core module, registered with CTest
legacy/qt/    the archived Qt implementation (read-only)
```

Configure, build, and test a fresh tree from the repository root:

```sh
sudo dnf install cmake gcc-c++ ninja-build pkgconf-pkg-config \
    gtkmm4.0-devel libadwaita-devel glibmm2.68-devel json-glib-devel glib2-devel \
    desktop-file-utils appstream
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

`appstream` and `desktop-file-utils` are needed by the RPM `%check` stage; they
are not required to compile the app. Build and test inside a Fedora container
that matches the target release. Fedora 43, 44 and 45 are supported, and CI
builds against the current Fedora release.

`cmake --build build` produces:

- `build/lgl-system-loadout` - the GUI, runs as the normal user
- `build/lgl-system-loadout-helper` - the privileged helper, installed to
  `%{_libexecdir}/lgl-system-loadout/` and started with `pkexec`

The test suite has seven binaries: `catalog_test`, `plan_test`, `policy_test`,
`protocol_test`, `selection_test`, `settings_test`, and `theme_test`. `%check`
runs all of them, validates the desktop file, and validates the metainfo with
`appstreamcli`.

## Snapshot smoke test

Set `LOADOUT_GTK_SNAPSHOT` to a folder to render every visible window to
`<folder>/<window title>.png` after three seconds and quit:

```sh
LOADOUT_GTK_SNAPSHOT=/tmp/loadout-snap ./build/lgl-system-loadout
```

The run is `NON_UNIQUE`, so it does not hand off to a running instance. Use a
temporary `XDG_CONFIG_HOME` (with a copy of `gtk-4.0/`, following symlinks) to
check settings-dependent states such as the system title bar mode.

## Source archives

`make source` writes `lgl-system-loadout-<version>.tar.gz` from the checked-out
tree. `make srpm` builds the SRPM into `result/`; `.copr/Makefile` includes the
root Makefile so COPR uses the same rules. `legacy/qt/` is excluded from both.
Install `appstream` and `desktop-file-utils` before rebuilding the SRPM locally,
because `%check` validates both files.

## Versioning

The version is defined in two places and must stay in sync:

- `CMakeLists.txt`: `project(lgl-system-loadout VERSION 3.0.0 ...)`
- `packaging/lgl-system-loadout.spec`: `Version: 3.0.0`, `Release: 1%{?dist}`

`LOADOUT_VERSION` is compiled from the CMake project version. The same version
appears in the metainfo release entry and in `CHANGELOG.md`. The current package
is **3.0.0-1**; release numbers are not separate application versions.

## Reference audit

Active build files, packaging, and CI must not refer to Qt or to a `gtk/` source
directory. Documentation may mention `legacy/qt/` as historical reference, but
nothing outside that directory may be built or installed.

## Deviations from the LGL GTK standard

Recorded here, as the standard's compliance clause requires, with the reason for
each. Everything not listed follows the standard.

- **Navigation is the step sidebar.** There are no Back and Next buttons in the
  header bar; every step is directly clickable, and Left/Right move focus between
  the sidebar and the current page. A Continue button at the bottom left of the
  content area advances a ready step, including after the update and after the
  installation, and is hidden on Welcome and Setup Complete. Requested by the
  project owner.
- **The wizard is a stack, not an assistant.** GTK 4 has no assistant widget, so
  steps are a sidebar plus `Gtk::Stack`, and the header bar carries the actions.
- **The privileged helper is a separate binary.** `lgl-system-loadout-helper`
  links JSON-GLib and the C library only, never GTK, and is started through
  `pkexec`. The GUI always runs as the normal user. The helper socket is owned by
  the pkexec caller with mode 0600, the GUI must send a random per-session nonce
  before the helper accepts commands, and the allow-list is limited to the COPR
  projects, download hosts, packages, pipx tools and users the catalogue uses.
  Helper and command output buffers are capped, and the helper session directory
  is removed recursively.
- **Selections are session-only.** `settings.ini` stores only
  `window/systemTitleBar`; a new run starts with nothing selected.
- **Install progress uses one combined log**, and failed steps are collected on
  the final step rather than in a per-step pane.
- **Stop is offered during install**, behind a confirmation, because a package
  manager with no way out is worse than a half-configured package.
- **Status chips use pastel tints.** Banners and notices still use the fixed
  status colours; the small chips use softened versions so a wall of cards stays
  readable.
- **GUI application icons are bundled** in `data/icons/` and exposed through an
  icon theme resource path, so cards look right before an app is installed.
  Attribution and licences are in `data/icons/ATTRIBUTION.md`.
- **The application stylesheet is reloaded on theme changes.** GTK loads the
  user's `gtk-4.0/gtk.css` only once, so `ThemeWatcher` re-parses the app
  stylesheet and watches both AdwStyleManager and GtkSettings. This keeps the
  custom `@lo_*` colours in step with the desktop accent and theme.
- **The main scroll areas hide their scrollbars** (wheel and keyboard still
  scroll), at the project owner's request, so nothing overlaps the cards.
