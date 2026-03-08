#include "mainwizard.h"
#include "pages/welcomepage.h"
#include "pages/repospage.h"
#include "pages/systemtoolspage.h"
#include "pages/pythonpage.h"
#include "pages/multimediapage.h"
#include "pages/contentpage.h"
#include "pages/gpupage.h"
#include "pages/gamingpage.h"
#include "pages/virtpage.h"
#include "pages/browserspage.h"
#include "pages/commspage.h"
#include "pages/themingpage.h"
#include "pages/cachyospage.h"
#include "pages/reviewpage.h"
#include "pages/installpage.h"
#include "pages/donepage.h"
#include <QProcess>
#include "pagehelpers.h"

MainWizard::MainWizard(QWidget *parent) : QWizard(parent)
{
    detectSystem();

    setWindowTitle("LGL System Loadout");
    setMinimumSize(960, 700);
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnStartPage, true);
    setOption(QWizard::NoBackButtonOnLastPage,  true);
    setOption(QWizard::DisabledBackButtonOnLastPage, true);

    setPage(PAGE_WELCOME,     new WelcomePage(this));
    setPage(PAGE_REPOS,       new ReposPage(this));
    setPage(PAGE_SYSTEMTOOLS, new SystemToolsPage(this));
    setPage(PAGE_PYTHON,      new PythonPage(this));
    setPage(PAGE_MULTIMEDIA,  new MultimediaPage(this));
    setPage(PAGE_CONTENT,     new ContentPage(this));
    setPage(PAGE_GPU,         new GpuPage(this));
    setPage(PAGE_GAMING,      new GamingPage(this));
    setPage(PAGE_VIRT,        new VirtPage(this));
    setPage(PAGE_BROWSERS,    new BrowsersPage(this));
    setPage(PAGE_COMMS,       new CommsPage(this));
    setPage(PAGE_THEMING,     new ThemingPage(this));
    setPage(PAGE_CACHYOS,     new CachyOSPage(this));
    setPage(PAGE_REVIEW,      new ReviewPage(this));
    setPage(PAGE_INSTALL,     new InstallPage(this));
    setPage(PAGE_DONE,        new DonePage(this));

    setStartId(PAGE_WELCOME);
}

void MainWizard::detectSystem()
{
    {
        QProcess p;
        p.start("rpm", {"-E", "%fedora"});
        p.waitForFinished(3000);
        m_fedoraVersion = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        if (m_fedoraVersion.isEmpty() || m_fedoraVersion.startsWith('%'))
            m_fedoraVersion = "41";
    }
    {
        m_targetUser = qEnvironmentVariable("SUDO_USER");
        if (m_targetUser.isEmpty() || m_targetUser == "root") {
            QProcess p;
            p.start("logname", {});
            p.waitForFinished(2000);
            m_targetUser = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        }
        if (m_targetUser.isEmpty()) m_targetUser = "root";
    }
}

void MainWizard::setOpt(const QString &k, const QVariant &v) { m_opts[k] = v; }
QVariant MainWizard::getOpt(const QString &k, const QVariant &def) const { return m_opts.value(k, def); }

// -----------------------------------------------------------------------
// Build ordered install steps from all user selections
// -----------------------------------------------------------------------
QList<InstallStep> MainWizard::buildSteps() const
{
    QList<InstallStep> S;
    const QString fv = m_fedoraVersion;
    const QString tu = m_targetUser;

    auto get = [&](const QString &k) { return m_opts.value(k, false).toBool(); };

    // Helper: install a package (install-only - removal not supported)
    auto dnfStep = [&](const QString &id, const QString &desc, const QString &pkg) -> InstallStep {
        return InstallStep{id, QString("Install %1").arg(pkg), {"dnf", "-y", "install", pkg}};
    };

    auto flatpakStep = [&](const QString &id, const QString &desc, const QString &appId, const QString &label) -> InstallStep {
        return InstallStep{id, QString("Install %1 (Flatpak)").arg(label),
            {"flatpak", "install", "-y", "--noninteractive", "flathub", appId}};
    };

    // ---- Always: bootstrap dnf tools ----
    S << InstallStep{"bootstrap", "Ensure core system tools are present",
        {"dnf", "-y", "install", "curl", "wget", "git", "dnf-plugins-core"}};

    // ---- Repos ----
    if (get("repos/rpmfusion_free")) {
        S << InstallStep{"rpmfusion_free", "Enable RPM Fusion Free",
            {"dnf", "-y", "install",
             QString("https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-%1.noarch.rpm").arg(fv)}};
    }
    if (get("repos/rpmfusion_nonfree")) {
        S << InstallStep{"rpmfusion_nonfree", "Enable RPM Fusion NonFree",
            {"dnf", "-y", "install",
             QString("https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-%1.noarch.rpm").arg(fv)}};
    }
    if (get("repos/upgrade")) {
        S << InstallStep{"sysupgrade", "Full system upgrade",
            {"dnf", "-y", "upgrade", "--refresh"}};
    }

    // ---- System Tools ----
    for (const auto &pkg : QStringList{
             "curl","wget","git","fastfetch","btop","htop",
             "distrobox","xrdp",
             "firewall-config","timeshift"}) {
        if (get(QString("systools/%1").arg(pkg))) {
            S << dnfStep(QString("systool_%1").arg(pkg), pkg, pkg);
        }
    }

    // ---- Python & CLI dev tools ----
    if (get("python/python3")) {
        S << dnfStep("python3", "python3", "python3");
    }
    if (get("python/pip")) {
        S << dnfStep("pip", "python3-pip", "python3-pip");
    }
    if (get("python/pipx")) {
        S << dnfStep("pipx_pkg", "pipx", "pipx");
        S << InstallStep{"pipx_ensurepath", QString("pipx ensurepath for %1").arg(tu),
            {"sudo", "-u", tu, "pipx", "ensurepath"}};
    }
    if (get("python/tldr")) {
        S << InstallStep{"pipx_tldr", "Install tldr via pipx",
            {"sudo", "-u", tu, "pipx", "install", "--include-deps", "tldr"}};
    }
    if (get("python/ytdlp")) {
        S << InstallStep{"pipx_ytdlp", "Install yt-dlp via pipx",
            {"sudo", "-u", tu, "pipx", "install", "--include-deps", "yt-dlp"}};
    }

    // ---- Multimedia & Codecs ----
    if (get("media/ffmpeg")) {
        S << InstallStep{"ffmpeg_swap", "Swap ffmpeg-free for full ffmpeg",
            {"dnf", "swap", "-y", "ffmpeg-free", "ffmpeg", "--allowerasing"}};
    }
    // GStreamer plugins - each individual
    const QList<QPair<QString,QString>> gst_plugins = {
        {"gst_bad_free",        "gstreamer1-plugins-bad-free"},
        {"gst_bad_free_extras", "gstreamer1-plugins-bad-free-extras"},
        {"gst_bad_nonfree",     "gstreamer1-plugins-ugly"},
        {"gst_good",            "gstreamer1-plugins-good"},
        {"gst_good_extras",     "gstreamer1-plugins-good-extras"},
        {"gst_base",            "gstreamer1-plugins-base"},
        {"gst_libav",           "gstreamer1-libav"},
    };
    for (const auto &[key, pkg] : gst_plugins) {
        if (get(QString("media/%1").arg(key))) {
            bool nonfree = (key == "gst_bad_nonfree" || key == "gst_libav");
            QStringList cmd = {"dnf", "-y", "install", "--skip-unavailable"};
            if (nonfree) cmd << "--allowerasing";
            cmd << pkg;
            S << InstallStep{key, QString("Install %1").arg(pkg), cmd};
        }
    }
    if (get("media/lame")) {
        S << InstallStep{"lame", "Install lame (MP3 encoding)", {"dnf", "-y", "install", "lame", "lame-libs"}};
    }
    if (get("media/vaapi")) {
        S << InstallStep{"vaapi", "Install VA-API libraries", {"dnf", "-y", "install", "ffmpeg-libs", "libva", "libva-utils"}};
    }
    if (get("media/vlc")) {
        S << dnfStep("vlc", "VLC media player", "vlc");
    }

    // ---- Content Creation ----
    if (get("content/obs")) {
        S << dnfStep("obs", "OBS Studio", "obs-studio");
    }
    if (get("content/kdenlive")) {
        S << dnfStep("kdenlive", "Kdenlive", "kdenlive");
    }
    if (get("content/gimp")) {
        S << dnfStep("gimp", "Gimp", "gimp");
    }
    if (get("content/inkscape")) {
        S << dnfStep("inkscape", "Inkscape", "inkscape");
    }
    if (get("content/audacity")) {
        S << dnfStep("audacity", "Audacity", "audacity");
    }

    // ---- GPU Drivers (AMD) ----
    const QString gpuChoice = m_opts.value("gpu/choice", "none").toString();
    if (gpuChoice == "amd") {
        const QList<QPair<QString,QString>> amdPkgs = {
            {"mesa_dri",    "mesa-dri-drivers"},
            {"mesa_vulkan", "mesa-vulkan-drivers"},
            {"vulkan_loader","vulkan-loader"},
            {"mesa_va",     "mesa-va-drivers"},
            {"mesa_vdpau",  "mesa-vdpau-drivers"},
            {"linux_fw",    "linux-firmware"},
        };
        for (const auto &[key, pkg] : amdPkgs) {
            if (get(QString("gpu/amd/%1").arg(key))) {
                if (key == "mesa_vdpau") {
                    // Install both standard and freeworld VDPAU drivers
                    S << InstallStep{QString("amd_%1").arg(key),
                        "Install mesa-vdpau-drivers + freeworld",
                        {"dnf", "-y", "install", "--skip-unavailable",
                         "mesa-vdpau-drivers", "mesa-vdpau-drivers-freeworld"}};
                } else {
                    S << InstallStep{
                        QString("amd_%1").arg(key),
                        QString("Install %1").arg(pkg),
                        {"dnf", "-y", "install", pkg}
                    };
                }
            }
        }
    }

    // ---- Gaming ----
    // Flatpak setup (injected once if any flatpak gaming app selected)
    bool needFlatpak = get("gaming/heroic") || get("gaming/protonup") ||
                       get("gaming/protonplus") || get("gaming/flatseal") ||
                       get("content/blender") ||
                       get("comms/discord") || get("comms/vesktop") ||
                       get("comms/spotify");
    if (needFlatpak) {
        S << InstallStep{"flatpak_pkg", "Install Flatpak",
            {"dnf", "-y", "install", "flatpak"}};
        S << InstallStep{"flathub_remote", "Add Flathub remote",
            {"flatpak", "remote-add", "--if-not-exists", "flathub",
             "https://flathub.org/repo/flathub.flatpakrepo"}};
    }

    const QList<QPair<QString,QString>> gamingRpm = {
        {"steam",     "steam"},
        {"lutris",    "lutris"},
        {"mangohud",  "mangohud"},
        {"vkbasalt",  "vkBasalt"},
        {"goverlay",  "goverlay"},
        {"gamemode",  "gamemode"},
        {"gamescope", "gamescope"},
        {"wine",      "wine"},
        {"protontricks","protontricks"},
        {"winetricks","winetricks"},
    };
    for (const auto &[key, pkg] : gamingRpm) {
        if (get(QString("gaming/%1").arg(key))) {
            S << dnfStep(QString("gaming_%1").arg(key), pkg, pkg);
        }
    }

    const QList<QPair<QString,QString>> gamingFlatpak = {
        {"heroic",     "com.heroicgameslauncher.hgl"},
        {"protonup",   "net.davidotek.pupgui2"},
        {"protonplus", "com.vysp3r.ProtonPlus"},
        {"flatseal",   "com.github.tchx84.Flatseal"},
    };
    for (const auto &[key, appid] : gamingFlatpak) {
        if (get(QString("gaming/%1").arg(key))) {
            S << flatpakStep(QString("flatpak_%1").arg(key), key, appid, key);
        }
    }

    // ---- Virtualisation ----
    bool anyVirt = get("virt/virtmanager") || get("virt/libvirt") ||
                   get("virt/libvirt_net") || get("virt/libvirt_kvm") ||
                   get("virt/qemu_kvm") || get("virt/virt_install") ||
                   get("virt/virt_viewer") || get("virt/ovmf") || get("virt/swtpm");
    const QList<QPair<QString,QString>> virtPkgs = {
        {"virtmanager",  "virt-manager"},
        {"libvirt",      "libvirt"},
        {"libvirt_net",  "libvirt-daemon-config-network"},
        {"libvirt_kvm",  "libvirt-daemon-kvm"},
        {"qemu_kvm",     "qemu-kvm"},
        {"virt_install", "virt-install"},
        {"virt_viewer",  "virt-viewer"},
        {"ovmf",         "edk2-ovmf"},
        {"swtpm",        "swtpm"},
    };
    for (const auto &[key, pkg] : virtPkgs) {
        if (get(QString("virt/%1").arg(key))) {
            S << dnfStep(QString("virt_%1").arg(key), pkg, pkg);
        }
    }
    if (anyVirt) {
        S << InstallStep{"libvirtd_enable", "Enable libvirtd service",
            {"systemctl", "enable", "--now", "libvirtd"}};
        S << InstallStep{"libvirt_group", QString("Add %1 to libvirt group").arg(tu),
            {"usermod", "-aG", "libvirt", tu}};
    }

    // ---- Browsers ----
    if (get("browsers/firefox")) {
        S << dnfStep("firefox", "Firefox", "firefox");
    }
    if (get("browsers/chromium")) {
        S << dnfStep("chromium", "Chromium", "chromium");
    }
    if (get("browsers/chrome")) {
        S << InstallStep{"chrome_repo", "Add Google Chrome repo",
            {"bash", "-c",
             "cat > /etc/yum.repos.d/google-chrome.repo << 'REPO'\n"
             "[google-chrome]\n"
             "name=google-chrome\n"
             "baseurl=https://dl.google.com/linux/chrome/rpm/stable/x86_64\n"
             "enabled=1\n"
             "gpgcheck=1\n"
             "gpgkey=https://dl.google.com/linux/linux_signing_key.pub\n"
             "REPO"}};
        S << dnfStep("chrome", "Google Chrome", "google-chrome-stable");
    }
    if (get("browsers/brave")) {
        S << InstallStep{"brave_repo", "Add Brave repo",
            {"bash", "-c",
             "curl -fsSLo /etc/yum.repos.d/brave-browser.repo "
             "https://brave-browser-rpm-release.s3.brave.com/brave-browser.repo && "
             "rpm --import https://brave-browser-rpm-release.s3.brave.com/brave-core.asc"}};
        S << InstallStep{"brave", "Install Brave Browser",
            {"dnf", "-y", "install", "brave-browser"}};
    }
    if (get("browsers/vivaldi")) {
        S << InstallStep{"vivaldi_repo", "Add Vivaldi repo",
            {"bash", "-c",
             "curl -fsSLo /etc/yum.repos.d/vivaldi.repo "
             "https://repo.vivaldi.com/archive/vivaldi-fedora.repo && "
             "rpm --import https://repo.vivaldi.com/archive/linux_signing_key.pub"}};
        S << InstallStep{"vivaldi", "Install Vivaldi",
            {"dnf", "-y", "install", "--setopt=keepcache=0", "vivaldi-stable"}};
    }

    // ---- Communication & Productivity ----
    if (get("comms/thunderbird")) {
        S << dnfStep("thunderbird", "Thunderbird", "thunderbird");
    }
    const QList<QPair<QString,QString>> commsFlatpak = {
        {"discord",    "com.discordapp.Discord"},
        {"vesktop",    "dev.vencord.Vesktop"},
        {"spotify",    "com.spotify.Client"},
    };
    for (const auto &[key, appid] : commsFlatpak) {
        if (get(QString("comms/%1").arg(key))) {
            S << flatpakStep(QString("flatpak_%1").arg(key), key, appid, key);
        }
    }
    // Blender lives in content but is Flatpak
    if (get("content/blender")) {
        S << flatpakStep("flatpak_blender", "Blender", "org.blender.Blender", "Blender");
    }

    // ---- Customisation & Theming ----
    if (get("theming/kvantum")) {
        S << dnfStep("kvantum", "kvantum", "kvantum");
    }
    if (get("theming/kvantum_manager")) {
        // kvantummanager binary ships in the 'kvantum' package on Fedora
        S << dnfStep("kvantum_manager", "kvantummanager", "kvantum");
    }
    if (get("theming/kzones")) {
        S << InstallStep{"kzones_dl", "Download KZones KWin script",
            {"bash", "-c",
             "curl -L -o /tmp/kzones.kwinscript "
             "https://github.com/gerritdevriese/kzones/releases/latest/download/kzones.kwinscript"}};
        S << InstallStep{"kzones_install", "Install KZones KWin script",
            {"sudo", "-u", tu, "kpackagetool6", "--type", "KWin/Script",
             "--install", "/tmp/kzones.kwinscript"}};
    }
    if (get("theming/panel_colorizer")) {
        // Download via GitHub API to get the actual release asset URL, then install.
        // The asset is a zip containing the plasmoid - we extract it first.
        S << InstallStep{"panel_col_dl", "Download Panel Colorizer plasmoid",
            {"bash", "-c",
             "ASSET=$(curl -sL https://api.github.com/repos/luisbocanegra/plasma-panel-colorizer/releases/latest"
             " | grep browser_download_url | grep '\\.plasmoid' | head -1 | cut -d'\"' -f4) && "
             "echo \"Downloading: $ASSET\" && "
             "curl -L -o /tmp/panel-colorizer.plasmoid \"$ASSET\" && "
             "ls -lh /tmp/panel-colorizer.plasmoid && "
             "file /tmp/panel-colorizer.plasmoid"}};
        S << InstallStep{"panel_col_install", "Install Panel Colorizer plasmoid",
            {"sudo", "-u", tu, "kpackagetool6", "--type", "Plasma/Applet",
             "--install", "/tmp/panel-colorizer.plasmoid"}};
    }

    // ---- CachyOS Kernel ----
    if (get("cachyos/kernel")) {
        S << InstallStep{"cachyos_copr1", "Enable kernel-cachyos COPR",
            {"dnf", "copr", "enable", "-y", "bieszczaders/kernel-cachyos"}};
        S << InstallStep{"cachyos_copr2", "Enable kernel-cachyos-addons COPR",
            {"dnf", "copr", "enable", "-y", "bieszczaders/kernel-cachyos-addons"}};
        S << InstallStep{"cachyos_kernel", "Install kernel-cachyos",
            {"dnf", "install", "-y", "kernel-cachyos"}};
    }
    if (get("cachyos/kernel_devel")) {
        S << InstallStep{"cachyos_devel", "Install kernel-cachyos-devel-matched",
            {"dnf", "install", "-y", "kernel-cachyos-devel-matched"}};
    }
    if (get("cachyos/kernel") || get("cachyos/kernel_devel")) {
        S << InstallStep{"selinux_bool", "Set SELinux domain_kernel_load_modules",
            {"setsebool", "-P", "domain_kernel_load_modules", "on"}};
    }
    if (get("cachyos/scx_scheds")) {
        S << InstallStep{"scx_scheds", "Install scx-scheds",
            {"dnf", "install", "-y", "scx-scheds"}};
    }
    if (get("cachyos/scx_manager")) {
        S << InstallStep{"scx_manager", "Install scx-manager",
            {"dnf", "install", "-y", "scx-manager"}};
    }
    if (get("cachyos/scx_tools")) {
        S << InstallStep{"scx_tools", "Install scx-tools",
            {"dnf", "install", "-y", "--allowerasing", "scx-tools"}};
    }
    if (get("cachyos/scxctl")) {
        // scxctl may be provided by scx-scheds package; --skip-unavailable handles if separate pkg missing
        S << InstallStep{"scxctl", "Install scxctl",
            {"dnf", "install", "-y", "--skip-unavailable", "scxctl"},
            /*optional=*/true};
    }

    // ---- Final cleanup ----
    S << InstallStep{"nm_wait_online", "Disable NetworkManager-wait-online (faster boot)",
        {"systemctl", "disable", "NetworkManager-wait-online.service"}, /*optional=*/true};
    S << InstallStep{"clean_all", "Clean DNF cache",
        {"dnf", "-y", "clean", "all"}};

    return S;
}

// -----------------------------------------------------------------------
// Disk space estimation
// -----------------------------------------------------------------------
int MainWizard::estimateDiskMB() const
{
    auto get = [&](const QString &k) { return m_opts.value(k, false).toBool(); };
    int mb = 0;

    // Repos / upgrade
    if (get("repos/rpmfusion_free"))    mb += 1;
    if (get("repos/rpmfusion_nonfree")) mb += 1;
    if (get("repos/upgrade"))           mb += 500; // unpredictable, assume moderate

    // System tools (~10 MB each)
    for (const auto &pkg : QStringList{
             "curl","wget","git","fastfetch","btop","htop",
             "distrobox","xrdp",
             "firewall-config","timeshift"})
        if (get(QString("systools/%1").arg(pkg))) mb += 10;

    // Python
    if (get("python/python3")) mb += 30;
    if (get("python/pip"))     mb += 10;
    if (get("python/pipx"))    mb += 5;
    if (get("python/tldr"))    mb += 5;
    if (get("python/ytdlp"))   mb += 20;

    // Multimedia
    if (get("media/ffmpeg"))            mb += 50;
    if (get("media/gst_bad_free"))      mb += 15;
    if (get("media/gst_bad_free_extras"))mb += 10;
    if (get("media/gst_bad_nonfree"))   mb += 10;
    if (get("media/gst_good"))          mb += 15;
    if (get("media/gst_good_extras"))   mb += 10;
    if (get("media/gst_base"))          mb += 15;
    if (get("media/gst_libav"))         mb += 10;
    if (get("media/lame"))              mb += 5;
    if (get("media/vaapi"))             mb += 20;
    if (get("media/vlc"))               mb += 80;

    // Content creation
    if (get("content/obs"))      mb += 120;
    if (get("content/kdenlive")) mb += 150;
    if (get("content/gimp"))     mb += 200;
    if (get("content/inkscape")) mb += 100;
    if (get("content/audacity")) mb += 30;
    if (get("content/blender"))  mb += 600;

    // GPU
    const QString gpuChoice = m_opts.value("gpu/choice", "none").toString();
    if (gpuChoice == "amd") {
        if (get("gpu/amd/mesa_dri"))      mb += 80;
        if (get("gpu/amd/mesa_vulkan"))   mb += 50;
        if (get("gpu/amd/vulkan_loader")) mb += 5;
        if (get("gpu/amd/mesa_va"))       mb += 20;
        if (get("gpu/amd/mesa_vdpau"))    mb += 20;
        if (get("gpu/amd/linux_fw"))      mb += 500;
    }

    // Gaming
    if (get("gaming/steam"))      mb += 400;
    if (get("gaming/lutris"))     mb += 50;
    if (get("gaming/heroic"))     mb += 200;
    if (get("gaming/wine"))       mb += 300;
    if (get("gaming/winetricks")) mb += 5;
    if (get("gaming/protonup"))   mb += 100;
    if (get("gaming/protonplus")) mb += 100;
    if (get("gaming/mangohud"))   mb += 10;
    if (get("gaming/goverlay"))   mb += 20;
    if (get("gaming/vkbasalt"))   mb += 5;
    if (get("gaming/gamemode"))   mb += 5;
    if (get("gaming/gamescope"))  mb += 20;
    if (get("gaming/flatseal"))   mb += 10;

    // Virtualisation
    if (get("virt/virtmanager"))  mb += 30;
    if (get("virt/libvirt"))      mb += 50;
    if (get("virt/qemu_kvm"))     mb += 200;
    if (get("virt/ovmf"))         mb += 5;
    if (get("virt/swtpm"))        mb += 5;

    // Browsers
    if (get("browsers/firefox"))  mb += 250;
    if (get("browsers/chromium")) mb += 300;
    if (get("browsers/chrome"))   mb += 415;
    if (get("browsers/brave"))    mb += 415;
    if (get("browsers/vivaldi"))  mb += 423;

    // Comms
    if (get("comms/thunderbird"))  mb += 100;
    if (get("comms/discord"))      mb += 200;
    if (get("comms/vesktop"))      mb += 200;
    if (get("comms/spotify"))      mb += 200;

    // Theming
    if (get("theming/kvantum"))         mb += 5;
    if (get("theming/kvantum_manager")) mb += 5;
    if (get("theming/kzones"))          mb += 2;
    if (get("theming/panel_colorizer")) mb += 2;

    // CachyOS kernel
    if (get("cachyos/kernel"))      mb += 120;
    if (get("cachyos/kernel_devel"))mb += 80;
    if (get("cachyos/scx_scheds"))  mb += 10;
    if (get("cachyos/scx_manager")) mb += 5;
    if (get("cachyos/scx_tools"))   mb += 5;
    if (get("cachyos/scxctl"))      mb += 2;

    // Flatpak infrastructure
    bool anyFlatpak = get("gaming/heroic")    || get("gaming/protonup")  ||
                      get("gaming/protonplus") || get("gaming/flatseal")  ||
                      get("comms/discord")     || get("comms/vesktop")    ||
                      get("comms/spotify")    ||
                      get("content/blender");
    if (anyFlatpak) mb += 200; // Flatpak runtime overhead

    return mb;
}

int MainWizard::availableDiskMB()
{
    QProcess p;
    p.start("df", {"--output=avail", "-m", "/"});
    p.waitForFinished(3000);
    const QStringList lines = QString::fromUtf8(p.readAllStandardOutput())
                                .trimmed().split('\n');
    if (lines.size() >= 2) {
        bool ok;
        int avail = lines.last().trimmed().toInt(&ok);
        if (ok) return avail;
    }
    return -1;
}
