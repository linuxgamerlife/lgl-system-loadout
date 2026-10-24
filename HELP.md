# LGL System Loadout Help

LGL System Loadout prepares a Fedora install for gaming, content creation and
development. Pick what you want on each step, review the list, and install.

Nothing is selected by default. Every item shows whether it is already on your
system before you commit, and every step is listed before you start.

## Installs only

This app installs. It never removes itself, and nothing it adds is uninstalled
for you. The only removals happen when a selected step has to replace a
conflicting package, such as the ffmpeg swap, and that step is listed before you
start. Removing anything afterwards is up to you, with dnf, Flatpak, or by
deleting the AppImage from ~/Applications.

## NetworkManager-wait-online

The System Tools step can disable NetworkManager-wait-online, which otherwise
adds several seconds to every boot while the network comes up.

Disabling it is safe on:

- a normal gaming desktop
- plain Ethernet or Wi-Fi
- Docker or Podman containers

Keep it enabled if you:

- mount NAS shares (NFS or CIFS) at boot
- use remote home directories
- log in with network-based authentication
- run servers with network-dependent startup services
- connect a VPN at boot

## Theme

The app follows the desktop light and dark mode, accent colour, and monospace
font. It also reloads the user's `~/.config/gtk-4.0/gtk.css` when that file
changes, so theme generators such as Noctalia apply without restarting the app.

## Cards and badges

Each card carries a small pill naming where the package comes from - FEDORA, RPM
FUSION, COPR, FLATPAK, PIPX or APPIMAGE - next to INSTALLED or NOT INSTALLED,
which show the current state on your system. LGL'S CHOICE marks the options
LinuxGamerLife recommends when you have no strong preference of your own.

{{pills}}

Click a card to select it, and click it again to leave it out.

## Steps

Only Welcome and System Update are offered until the update has run or you have
confirmed skipping it, because the rest of the wizard depends on an up-to-date
system. After that you can move between steps freely, and your choices are kept.

- **Welcome** - your Fedora version and the account user-level tools install for.
- **System Update** - a full update, recommended before anything else.
- **Repositories** - RPM Fusion Free and NonFree.
- **Essentials** - one file manager and one terminal, or neither.
- **System Tools** - command-line and desktop utilities, plus two system tweaks.
- **Remote Access** - Moonlight, Sunshine, Parsec and Tailscale.
- **Development Tools** - Python tooling, git, Zed, GitHub Desktop, opencode, pi.
- **Multimedia & Codecs** - ffmpeg, GStreamer plugins and VLC.
- **Content Creation** - OBS Studio, the Audacity 4.0.1 AppImage, yt-dlp, and
  pointers to FilmCraft and SoundCraft through the ArtCraft launcher.
- **GPU Drivers** - AMD driver packages. NVIDIA needs manual steps, which the
  step explains.
- **Gaming** - launchers, compatibility tools, overlays and controller support.
- **Virtualisation** - the KVM/QEMU stack, viewers and LGL VM Curator.
- **Browsers** - Chromium-based and Mozilla-based browsers, grouped by engine.
- **Communication** - the ArtCraft launcher, office, chat and music applications.
- **CachyOS Kernel** - an optional performance kernel. Secure Boot must be off.
  Selecting the kernel or its matching headers also enables the SELinux
  `domain_kernel_load_modules` boolean permanently.
- **LGL Tool Kit** - small utilities from LinuxGamerLife.
- **Review Selections** - the full list of steps that will run, and a disk check.
- **Install Now** - review the plan, then choose Start Installation. Progress and output appear here.
- **Setup Complete** - a summary, any failed steps, and the option to reboot.

A **Continue** button sits at the bottom left of the content area on steps that
have a next step. It appears when the step is ready and is hidden on Welcome and
Setup Complete. It moves you to the next step, including after the system update
and after the installation.

## The password prompt

The app itself always runs as your normal user. When you start an installation it
asks for your password once and starts a small privileged helper. That helper
accepts only the operations this app performs, runs them without a shell, and
closes when the session ends. The helper socket is private to your account and
the app has to present a one-time session code before the helper runs anything,
so another local program cannot take over the session.

The system update and the package installation are separate sessions, so each
asks once. Accepting the Flatpak update prompt is another session, and so are
the Reboot buttons.

AppImages are the exception: they are downloaded into your own `~/Applications`
folder as you, with no password.

## Shortcuts

- **Ctrl+Q** - quit
- **Ctrl+Page Down** - next step; on Review Selections it opens Install Now,
  where Start Installation begins the installation
- **Ctrl+Page Up** - previous step
- **Left** - move focus from the cards to the step list
- **Right** - move focus from the step list into the current page
- **Ctrl+Shift+M** - switch between the app title bar and the system title bar
- **Alt** (tap) - open or close the main menu
- **F10** - open the main menu
- **F1** - open this help
- **Escape** - close the main menu or a dialog

## If a step fails

The Install Now step marks each step as it runs. Failed steps turn red and their
output appears in the log. The Setup Complete step collects every failed step so
you can copy it and report the problem.

A failed step does not stop the remaining steps. Packages that installed
successfully stay installed.

## If installation will not start

The helper is launched through polkit. If the Welcome step warns that polkit does
not know the app's action, close the app and open it again from your application
menu, then try once more. Launching straight from the software centre just after
installation can happen before polkit has loaded the new policy.

## Disk space

The review step estimates the space needed, adds a 15% margin plus 500 MB for the
package cache, and compares it with free space on `/`. You can continue past a
warning, but packages may fail to unpack.

## Uninstalling

Removing the app does not remove anything it installed. Use dnf or the software
centre for those packages. AppImages stay in `~/Applications` until you delete
them.
