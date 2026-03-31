#include "mainwizard.h"
#include "pages/welcomepage.h"
#include "pages/updatepage.h"
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
#include "pages/cachyospage.h"
#include "pages/scxpage.h"
#include "pages/reviewpage.h"
#include "pages/installpage.h"
#include "pages/donepage.h"
#include <QProcess>
#include <QEventLoop>
#include <QTimer>
#include <QRegularExpression>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
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
    setPage(PAGE_UPDATE,      new UpdatePage(this));
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
    setPage(PAGE_CACHYOS,     new CachyOSPage(this));
    setPage(PAGE_SCX,         new ScxPage(this));
    setPage(PAGE_REVIEW,      new ReviewPage(this));
    setPage(PAGE_INSTALL,     new InstallPage(this));
    setPage(PAGE_DONE,        new DonePage(this));

    setStartId(PAGE_WELCOME);
}

// detectSystem() runs synchronously in the constructor, before the window is shown.
// This is an accepted startup probe: it invokes only fast local binaries (rpm, logname)
// with strict timeouts (≤3 s). It must never make network calls or invoke dnf.
// If either binary hangs beyond its timeout it is killed and a safe default is used.
void MainWizard::detectSystem()
{
    {
        QProcess p;
        p.start("rpm", {"-E", "%fedora"});
        if (!p.waitForFinished(3000)) p.kill();
        m_fedoraVersion = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        if (m_fedoraVersion.isEmpty() || m_fedoraVersion.startsWith('%'))
            m_fedoraVersion = "44";
    }
    {
        // targetUser is passed as an argument to usermod and as context for
        // user-scoped tools. Validate immediately — only lowercase POSIX usernames
        // accepted. Anything else results in a "root" fallback with a warning.
        static const QRegularExpression kSafeUsername(
            QStringLiteral("^[a-z_][a-z0-9_\\-]{0,31}$"));

        auto isSafeUsername = [&](const QString &name) -> bool {
            return !name.isEmpty()
                && name != QStringLiteral("root")
                && kSafeUsername.match(name).hasMatch();
        };

        m_targetUser = qEnvironmentVariable("SUDO_USER");
        if (!isSafeUsername(m_targetUser)) {
            QProcess p;
            p.start("logname", {});
            if (!p.waitForFinished(2000)) p.kill();
            m_targetUser = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        }
        if (!isSafeUsername(m_targetUser)) {
            qWarning() << "detectSystem: could not determine a safe non-root target user "
                          "(SUDO_USER unset or invalid, logname returned nothing useful). "
                          "Falling back to 'root'. User-scoped tools (pipx, kpackagetool6) "
                          "will run as root.";
            m_targetUser = "root";
        }
    }
}

void MainWizard::setOpt(const QString &k, const QVariant &v) { m_opts[k] = v; }
QVariant MainWizard::getOpt(const QString &k, const QVariant &def) const { return m_opts.value(k, def); }

// -----------------------------------------------------------------------
// Launch the privileged helper
// -----------------------------------------------------------------------
QString MainWizard::launchHelper()
{
    // Guard: only launch once per session.
    if (m_helperProcess && m_helperProcess->state() == QProcess::Running)
        return m_opts.value("install/socketPath").toString();

    // Attempt to launch the helper, retrying once after a short delay.
    // On first launch from Discover post-install, the D-Bus session may not
    // be ready to handle pkexec authentication. A single silent retry after
    // 1.5 s covers this without the user ever seeing an error.
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (attempt > 0) {
            // Wait 1.5 s before retrying without blocking the UI thread.
            QEventLoop loop;
            QTimer::singleShot(1500, &loop, &QEventLoop::quit);
            loop.exec();
        }

        delete m_helperProcess;
        m_helperProcess = new QProcess(this);
        m_helperProcess->setProcessChannelMode(QProcess::SeparateChannels);
        m_helperProcess->start("pkexec",
            {"/usr/libexec/lgl-system-loadout/lgl-system-loadout-helper"});

        if (!m_helperProcess->waitForStarted(10000)) {
            delete m_helperProcess;
            m_helperProcess = nullptr;
            continue;
        }

        QString socketPath;
        if (m_helperProcess->waitForReadyRead(60000))
            socketPath = QString::fromLocal8Bit(m_helperProcess->readLine()).trimmed();

        if (!socketPath.isEmpty() && socketPath.startsWith("/run/lgl-")) {
            m_opts["install/socketPath"] = socketPath;
            return socketPath;
        }

        qWarning() << "lgl-helper launch attempt" << (attempt + 1) << "failed."
                   << "pkexec stderr:" << m_helperProcess->readAllStandardError()
                   << "exit code:" << m_helperProcess->exitCode();
        m_helperProcess->kill();
        delete m_helperProcess;
        m_helperProcess = nullptr;
    }

    return {};
}

// -----------------------------------------------------------------------
// Build ordered install steps from all user selections
// -----------------------------------------------------------------------
QList<InstallStep> MainWizard::buildSteps() const
{
    QList<InstallStep> S;
    const QString fv = m_fedoraVersion;
    const QString tu = m_targetUser;

    // Derive the helper's temp directory from the socket path.
    // socketPath = /run/lgl-XXXXXX/lgl-helper.sock
    // tmpDir     = /run/lgl-XXXXXX
    // This directory is owned by root (mode 0700) and managed by the helper.
    const QString socketPath = m_opts.value("install/socketPath").toString();
    const QString tmpDir = socketPath.isEmpty()
        ? QString()
        : socketPath.section('/', 0, -2);  // strip filename component

    // Helper: produce a temp path under the helper's session directory.
    // Falls back to a predictable path if socketPath isn't known yet
    // (shouldn't happen in practice — buildSteps() is called after launchHelper()).
    auto tmpPath = [&](const QString &filename) -> QString {
        return tmpDir.isEmpty()
            ? QString("/run/lgl-tmp/%1").arg(filename)
            : QString("%1/%2").arg(tmpDir, filename);
    };

    auto get = [&](const QString &k) { return m_opts.value(k, false).toBool(); };

    // Helper: plain dnf install step (most common case).
    auto dnfStep = [&](const QString &id, const QString &pkg) -> InstallStep {
        return InstallStep{id, QString("Install %1").arg(pkg),
                           {"/usr/bin/dnf", "-y", "install", pkg}};
    };

    // Helper: flatpak install from Flathub.
    auto flatpakStep = [&](const QString &id, const QString &appId,
                           const QString &label) -> InstallStep {
        return InstallStep{id, QString("Install %1 (Flatpak)").arg(label),
            {"/usr/bin/flatpak", "install", "-y", "--system", "flathub", appId}};
    };

    // ---- System update (optional, user chose on update page) ----
    // ---- Always: bootstrap dnf tools ----
    S << InstallStep{"bootstrap", "Ensure core system tools are present",
        {"/usr/bin/dnf", "-y", "install", "curl", "wget2-wget", "git", "python3-dnf5-plugins"},
        /*optional=*/false,
        /*alreadyInstalledCheck=*/{"/usr/bin/rpm", "-q", "--quiet",
            "curl", "wget2-wget", "git", "python3-dnf5-plugins"}};

    // ---- Repos ----
    if (get("repos/rpmfusion_free")) {
        S << InstallStep{"rpmfusion_free", "Enable RPM Fusion Free",
            {"/usr/bin/dnf", "-y", "install",
             QString("https://download1.rpmfusion.org/free/fedora/"
                     "rpmfusion-free-release-%1.noarch.rpm").arg(fv)}};
    }
    if (get("repos/rpmfusion_nonfree")) {
        S << InstallStep{"rpmfusion_nonfree", "Enable RPM Fusion NonFree",
            {"/usr/bin/dnf", "-y", "install",
             QString("https://download1.rpmfusion.org/nonfree/fedora/"
                     "rpmfusion-nonfree-release-%1.noarch.rpm").arg(fv)}};
    }

    // ---- System Tools ----
    for (const auto &pkg : QStringList{
             "fastfetch", "btop", "htop", "distrobox", "xrdp", "timeshift"}) {
        if (get(QString("systools/%1").arg(pkg)))
            S << dnfStep(QString("systool_%1").arg(pkg), pkg);
    }

    // ---- Python & CLI dev tools ----
    if (get("python/pip"))
        S << dnfStep("pip", "python3-pip");

    if (get("python/pipx")) {
        S << dnfStep("pipx_pkg", "pipx");
    }
    if (get("python/tldr"))
        S << InstallStep{"pipx_tldr", "Install tldr via pipx",
            {"/usr/bin/sudo", "-u", tu, "/usr/bin/pipx", "install", "--include-deps", "tldr"}};

    if (get("python/ytdlp"))
        S << InstallStep{"pipx_ytdlp", "Install yt-dlp via pipx",
            {"/usr/bin/sudo", "-u", tu, "/usr/bin/pipx", "install", "--include-deps", "yt-dlp"}};

    // ---- Multimedia & Codecs ----
    if (get("media/ffmpeg"))
        S << InstallStep{"ffmpeg_swap", "Swap ffmpeg-free for full ffmpeg",
            {"/usr/bin/dnf", "swap", "-y", "ffmpeg-free", "ffmpeg", "--allowerasing"}};

    if (get("media/gst_bad_nonfree"))
        S << InstallStep{"gst_bad_nonfree", "Install GStreamer ugly plugins",
            {"/usr/bin/dnf", "-y", "install", "--skip-unavailable", "--allowerasing",
             "gstreamer1-plugins-ugly"}};

    if (get("media/gst_bad_free_extras"))
        S << InstallStep{"gst_bad_free_extras", "Install GStreamer bad-free-extras",
            {"/usr/bin/dnf", "-y", "install", "--skip-unavailable",
             "gstreamer1-plugins-bad-free-extras"}};

    if (get("media/vlc"))
        S << dnfStep("vlc", "vlc");

    // ---- Content Creation ----
    for (const auto &[key, pkg] : QList<QPair<QString,QString>>{
            {"obs",       "obs-studio"},
            {"kdenlive",  "kdenlive"},
            {"gimp",      "gimp"},
            {"inkscape",  "inkscape"},
            {"audacity",  "audacity"},
        }) {
        if (get(QString("content/%1").arg(key)))
            S << dnfStep(key, pkg);
    }

    // ---- GPU Drivers (AMD) ----
    const QString gpuChoice = m_opts.value("gpu/choice", "none").toString();
    if (gpuChoice == "amd") {
        for (const auto &[key, pkg] : QList<QPair<QString,QString>>{
                {"mesa_dri",      "mesa-dri-drivers"},
                {"mesa_vulkan",   "mesa-vulkan-drivers"},
                {"vulkan_loader", "vulkan-loader"},
                {"mesa_va",       "mesa-va-drivers"},
                {"linux_fw",      "linux-firmware"},
            }) {
            if (get(QString("gpu/amd/%1").arg(key)))
                S << InstallStep{QString("amd_%1").arg(key),
                    QString("Install %1").arg(pkg),
                    {"/usr/bin/dnf", "-y", "install", pkg}};
        }
    }

    // ---- Flatpak infrastructure (injected once if any Flatpak item selected) ----
    const bool needFlatpak =
        get("gaming/heroic")    || get("gaming/protonup")   ||
        get("gaming/protonplus")|| get("gaming/flatseal")   ||
        get("content/blender")  ||
        get("comms/discord")    || get("comms/vesktop")     ||
        get("comms/spotify")    || get("comms/thunderbird") ||
        get("browsers/librewolf");

    if (needFlatpak) {
        S << dnfStep("flatpak_pkg", "flatpak");
        S << InstallStep{"flathub_remote", "Add Flathub remote",
            {"/usr/bin/flatpak", "remote-add", "--if-not-exists", "--system",
             "flathub", "https://flathub.org/repo/flathub.flatpakrepo"}};
    }

    // ---- Gaming — RPM ----
    if (get("gaming/kernel_modules_extra"))
        S << dnfStep("gaming_kernel_modules_extra", "kernel-modules-extra");
    if (get("gaming/steam"))
        S << dnfStep("gaming_steam", "steam");
    if (get("gaming/lutris"))
        S << dnfStep("gaming_lutris", "lutris");
    if (get("gaming/mangohud"))
        S << dnfStep("gaming_mangohud", "mangohud");
    if (get("gaming/vkbasalt"))
        S << dnfStep("gaming_vkbasalt", "vkBasalt");
    if (get("gaming/goverlay"))
        S << dnfStep("gaming_goverlay", "goverlay");
    if (get("gaming/wine"))
        // Both 64-bit and 32-bit wine required for full multilib support.
        S << InstallStep{"gaming_wine", "Install wine + wine.i686 (32-bit support)",
            {"/usr/bin/dnf", "-y", "install", "wine", "wine.i686"}};
    if (get("gaming/protontricks"))
        S << dnfStep("gaming_protontricks", "protontricks");

    // ---- Gaming — Flatpak ----
    // Heroic: COPR preferred over Flatpak for native integration.
    if (get("gaming/heroic")) {
        S << InstallStep{"heroic_copr", "Enable Heroic Games Launcher COPR",
            {"/usr/bin/dnf", "copr", "enable", "-y", "atim/heroic-games-launcher"}};
        S << dnfStep("heroic_install", "heroic-games-launcher-bin");
    }
    if (get("gaming/protonup"))
        S << flatpakStep("flatpak_protonup", "net.davidotek.pupgui2", "ProtonUp-Qt");
    if (get("gaming/protonplus"))
        S << flatpakStep("flatpak_protonplus", "com.vysp3r.ProtonPlus", "ProtonPlus");
    if (get("gaming/flatseal"))
        S << flatpakStep("flatpak_flatseal", "com.github.tchx84.Flatseal", "Flatseal");

    // ---- Virtualisation ----
    const bool anyVirt = get("virt/virtmanager") || get("virt/libvirt") ||
                         get("virt/virt_install") || get("virt/virt_viewer");
    for (const auto &[key, pkg] : QList<QPair<QString,QString>>{
            {"virtmanager",  "virt-manager"},
            {"libvirt",      "libvirt"},
            {"virt_install", "virt-install"},
            {"virt_viewer",  "virt-viewer"},
        }) {
        if (get(QString("virt/%1").arg(key)))
            S << dnfStep(QString("virt_%1").arg(key), pkg);
    }
    if (anyVirt) {
        S << InstallStep{"libvirtd_enable", "Enable libvirtd service",
            {"/usr/bin/systemctl", "enable", "--now", "libvirtd"}};
        S << InstallStep{"libvirt_group",
            QString("Add %1 to libvirt group").arg(tu),
            {"/usr/sbin/usermod", "-aG", "libvirt", tu}};
    }

    // ---- Browsers ----
    if (get("browsers/firefox"))
        S << dnfStep("firefox", "firefox");

    if (get("browsers/chromium"))
        S << dnfStep("chromium", "chromium");

    if (get("browsers/chrome"))
        S << flatpakStep("chrome", "com.google.Chrome", "Google Chrome");

    if (get("browsers/brave")) {
        S << InstallStep{"brave_repo_dl", "Download Brave repo file",
            {"/usr/bin/curl", "-fsSL", "-o",
             tmpPath("brave-browser.repo"),
             "https://brave-browser-rpm-release.s3.brave.com/brave-browser.repo"}};
        S << InstallStep{"brave_repo_add", "Add Brave repo",
            {"/usr/bin/dnf", "config-manager", "addrepo", "--from-repofile", tmpPath("brave-browser.repo")}};
        S << dnfStep("brave", "brave-browser");
    }

    if (get("browsers/vivaldi")) {
        S << InstallStep{"vivaldi_repo_dl", "Download Vivaldi repo file",
            {"/usr/bin/curl", "-fsSL", "-o",
             tmpPath("vivaldi.repo"),
             "https://repo.vivaldi.com/archive/vivaldi-fedora.repo"}};
        S << InstallStep{"vivaldi_repo_add", "Add Vivaldi repo",
            {"/usr/bin/dnf", "config-manager", "addrepo", "--from-repofile", tmpPath("vivaldi.repo")}};
        S << dnfStep("vivaldi", "vivaldi-stable");
    }

    if (get("browsers/librewolf"))
        S << flatpakStep("librewolf", "io.gitlab.librewolf-community", "LibreWolf");

    // ---- Communication & Productivity ----
    if (get("comms/thunderbird"))
        S << flatpakStep("flatpak_thunderbird", "org.mozilla.Thunderbird", "Thunderbird");

    for (const auto &[key, appid] : QList<QPair<QString,QString>>{
            {"discord", "com.discordapp.Discord"},
            {"vesktop", "dev.vencord.Vesktop"},
            {"spotify", "com.spotify.Client"},
        }) {
        if (get(QString("comms/%1").arg(key)))
            S << flatpakStep(QString("flatpak_%1").arg(key), appid, key);
    }

    // Blender — Flatpak, listed under content
    if (get("content/blender"))
        S << flatpakStep("flatpak_blender", "org.blender.Blender", "Blender");

        // ---- CachyOS Kernel ----
    // kernel-cachyos COPR is only needed for the kernel itself.
    // kernel-cachyos-addons COPR is needed for both the kernel and all scx packages.
    const bool anyScx = get("cachyos/scx_scheds") || get("cachyos/scx_manager") || get("cachyos/scx_tools");
    if (get("cachyos/kernel") || anyScx) {
        S << InstallStep{"cachyos_copr2", "Enable kernel-cachyos-addons COPR",
            {"/usr/bin/dnf", "copr", "enable", "-y", "bieszczaders/kernel-cachyos-addons"}};
    }
    if (get("cachyos/kernel")) {
        S << InstallStep{"cachyos_copr1", "Enable kernel-cachyos COPR",
            {"/usr/bin/dnf", "copr", "enable", "-y", "bieszczaders/kernel-cachyos"}};
        S << InstallStep{"cachyos_kernel", "Install kernel-cachyos",
            {"/usr/bin/dnf", "-y", "install", "kernel-cachyos"}};
    }
    if (get("cachyos/kernel_devel"))
        S << InstallStep{"cachyos_devel", "Install kernel-cachyos-devel-matched",
            {"/usr/bin/dnf", "-y", "install", "kernel-cachyos-devel-matched"}};

    if (get("cachyos/kernel") || get("cachyos/kernel_devel"))
        S << InstallStep{"selinux_bool", "Set SELinux domain_kernel_load_modules",
            {"/usr/sbin/setsebool", "-P", "domain_kernel_load_modules", "on"}};

    // scx packages: exit 1 is a known benign exit on some kernel configs.
    if (get("cachyos/scx_scheds"))
        // Skip if either the stable or git variant is already present —
        // both provide the same scheduler functionality.
        S << InstallStep{"scx_scheds", "Install scx-scheds",
            {"/usr/bin/dnf", "-y", "install", "--allowerasing", "scx-scheds"},
            /*optional=*/true,
            /*alreadyInstalledCheck=*/{"/usr/bin/rpm", "-q", "--quiet", "scx-scheds-git"},
            {1}};
    if (get("cachyos/scx_manager"))
        S << InstallStep{"scx_manager", "Install scx-manager",
            {"/usr/bin/dnf", "-y", "install", "--allowerasing", "scx-manager"},
            /*optional=*/false, {}, {1}};
    if (get("cachyos/scx_tools"))
        S << InstallStep{"scx_tools", "Install scx-tools",
            {"/usr/bin/dnf", "-y", "install", "--allowerasing", "scx-tools"},
            /*optional=*/false, {}, {1}};

    // ---- Final cleanup / tweaks ----
    if (get("systools/nm_wait_online"))
        S << InstallStep{"nm_wait_online",
            "Disable NetworkManager-wait-online (faster boot)",
            {"/usr/bin/systemctl", "disable", "--now", "NetworkManager-wait-online"},
            /*optional=*/true};

    if (get("systools/clean_cache"))
        S << InstallStep{"clean_all", "Clean DNF cache",
            {"/usr/bin/dnf", "clean", "all"}};

    return S;
}

// -----------------------------------------------------------------------
// Run a single command via the helper, streaming output to a callback.
// Launches the helper if not already running.
// -----------------------------------------------------------------------
int MainWizard::runHelperCommand(const QStringList &command,
                                 std::function<void(const QString &)> outputLine)
{
    // Helper must already be launched via launchHelper() on the main thread
    // before calling this function. launchHelper() must never be called from
    // a worker thread as it creates a QProcess parented to MainWizard.
    const QString socketPath = m_opts.value("install/socketPath").toString();
    if (socketPath.isEmpty()) {
        outputLine("ERROR: helper not launched — call launchHelper() first");
        return -1;
    }

    QLocalSocket sock;
    sock.connectToServer(socketPath);
    if (!sock.waitForConnected(5000)) {
        outputLine(QString("ERROR: could not connect to helper: %1")
                   .arg(sock.errorString()));
        return -1;
    }

    static int s_counter = 0;
    const QString requestId = QString("main-%1").arg(++s_counter);

    QJsonObject msg;
    msg["type"]      = "execute";
    msg["requestId"] = requestId;
    msg["program"]   = command.first();
    QJsonArray args;
    for (int i = 1; i < command.size(); ++i) args.append(command[i]);
    msg["args"] = args;

    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    data.append('\n');
    sock.write(data);
    sock.flush();

    QByteArray readBuf;
    int result = -1;

    while (true) {
        if (!sock.waitForReadyRead(500)) {
            if (sock.state() != QLocalSocket::ConnectedState) {
                outputLine("ERROR: helper disconnected unexpectedly");
                break;
            }
            continue;
        }

        readBuf.append(sock.readAll());

        int newline;
        while ((newline = readBuf.indexOf('\n')) != -1) {
            const QByteArray raw = readBuf.left(newline).trimmed();
            readBuf.remove(0, newline + 1);
            if (raw.isEmpty()) continue;

            QJsonParseError parseErr;
            const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseErr);
            if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) continue;

            const QJsonObject resp = doc.object();
            const QString     type = resp["type"].toString();

            if (type == "output") {
                outputLine(resp["line"].toString());
            } else if (type == "finished") {
                result = resp["exitCode"].toInt(-1);
                goto done; // NOLINT
            } else if (type == "rejected") {
                outputLine(QString("ERROR: helper rejected: %1").arg(resp["error"].toString()));
                goto done; // NOLINT
            } else if (type == "error") {
                outputLine(QString("ERROR: %1").arg(resp["error"].toString()));
                goto done; // NOLINT
            }
        }
    }

done:
    sock.disconnectFromServer();
    return result;
}

// -----------------------------------------------------------------------
// Disk space estimation
// -----------------------------------------------------------------------
int MainWizard::estimateDiskMB() const
{
    auto get = [&](const QString &k) { return m_opts.value(k, false).toBool(); };
    int mb = 0;

    if (get("repos/rpmfusion_free"))    mb += 1;
    if (get("repos/rpmfusion_nonfree")) mb += 1;

    for (const auto &pkg : QStringList{
             "fastfetch", "btop", "htop", "distrobox", "xrdp", "timeshift"})
        if (get(QString("systools/%1").arg(pkg))) mb += 10;

    if (get("python/pip"))   mb += 10;
    if (get("python/pipx"))  mb += 5;
    if (get("python/tldr"))  mb += 5;
    if (get("python/ytdlp")) mb += 20;

    if (get("media/ffmpeg"))             mb += 50;
    if (get("media/gst_bad_free_extras"))mb += 10;
    if (get("media/gst_bad_nonfree"))    mb += 10;
    if (get("media/vlc"))                mb += 80;

    if (get("content/obs"))      mb += 120;
    if (get("content/kdenlive")) mb += 150;
    if (get("content/gimp"))     mb += 200;
    if (get("content/inkscape")) mb += 100;
    if (get("content/audacity")) mb += 30;
    if (get("content/blender"))  mb += 600;

    const QString gpuChoice = m_opts.value("gpu/choice", "none").toString();
    if (gpuChoice == "amd") {
        if (get("gpu/amd/mesa_dri"))      mb += 80;
        if (get("gpu/amd/mesa_vulkan"))   mb += 50;
        if (get("gpu/amd/vulkan_loader")) mb += 5;
        if (get("gpu/amd/mesa_va"))       mb += 20;
        if (get("gpu/amd/linux_fw"))      mb += 500;
    }

    if (get("gaming/kernel_modules_extra")) mb += 50;
    if (get("gaming/steam"))       mb += 400;
    if (get("gaming/lutris"))      mb += 50;
    if (get("gaming/heroic"))      mb += 200;
    if (get("gaming/wine"))        mb += 300;
    if (get("gaming/protonup"))    mb += 100;
    if (get("gaming/protonplus"))  mb += 100;
    if (get("gaming/mangohud"))    mb += 10;
    if (get("gaming/goverlay"))    mb += 20;
    if (get("gaming/vkbasalt"))    mb += 5;
    if (get("gaming/flatseal"))    mb += 10;

    if (get("virt/virtmanager"))  mb += 30;
    if (get("virt/libvirt"))      mb += 50;

    if (get("browsers/firefox"))   mb += 250;
    if (get("browsers/chromium"))  mb += 300;
    if (get("browsers/chrome"))    mb += 200;
    if (get("browsers/brave"))     mb += 415;
    if (get("browsers/vivaldi"))   mb += 423;
    if (get("browsers/librewolf")) mb += 300;

    if (get("comms/thunderbird")) mb += 200;
    if (get("comms/discord"))     mb += 200;
    if (get("comms/vesktop"))     mb += 200;
    if (get("comms/spotify"))     mb += 200;


    if (get("cachyos/kernel"))       mb += 120;
    if (get("cachyos/kernel_devel")) mb += 80;
    if (get("cachyos/scx_scheds"))   mb += 10;
    if (get("cachyos/scx_manager"))  mb += 5;
    if (get("cachyos/scx_tools"))    mb += 5;

    const bool anyFlatpak =
        get("gaming/heroic")    || get("gaming/protonup")   ||
        get("gaming/protonplus")|| get("gaming/flatseal")   ||
        get("comms/discord")    || get("comms/vesktop")     ||
        get("comms/spotify")    || get("comms/thunderbird") ||
        get("browsers/librewolf") || get("content/blender");
    if (anyFlatpak) mb += 200;

    return mb;
}

int MainWizard::availableDiskMB()
{
    QProcess p;
    p.start("df", {"--output=avail", "-m", "/"});
    if (!p.waitForFinished(3000)) { p.kill(); return -1; }
    const QStringList lines = QString::fromUtf8(p.readAllStandardOutput())
                                .trimmed().split('\n');
    if (lines.size() >= 2) {
        bool ok;
        int avail = lines.last().trimmed().toInt(&ok);
        if (ok) return avail;
    }
    return -1;
}
