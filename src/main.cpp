#include <QApplication>
#include <QFont>
#include <QFileInfo>
#include <QDialog>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QProcess>
#include <QSettings>
#include <QVBoxLayout>
#include <QTextStream>
#include "mainwizard.h"
#include <unistd.h>
#include <pwd.h>
#include <cstdlib>
#include <cerrno>
#include <climits>

int main(int argc, char *argv[])
{
    // Restore the real user's environment before QApplication is created,
    // so the platform theme (KDE/Breeze) loads correctly.
    // pkexec sets PKEXEC_UID; sudo -E sets SUDO_USER.
    struct passwd *pw = nullptr;

    const char *pkexecUid = std::getenv("PKEXEC_UID");
    const char *sudoUser  = std::getenv("SUDO_USER");

    if (pkexecUid && geteuid() == 0) {
        // Validate PKEXEC_UID before use. atoi() is unsafe — a non-numeric or
        // empty value returns 0, which resolves to root via getpwuid(0).
        // strtoul with explicit error checking prevents that.
        char *endptr = nullptr;
        errno = 0;
        const unsigned long rawUid = std::strtoul(pkexecUid, &endptr, 10);
        const bool validUid = (errno == 0 && endptr != pkexecUid && *endptr == '\0'
                               && rawUid <= static_cast<unsigned long>(UINT_MAX));
        if (validUid) {
            pw = getpwuid(static_cast<uid_t>(rawUid));
        }
    } else if (sudoUser && geteuid() == 0) {
        pw = getpwnam(sudoUser);
    }

    if (pw) {
        uid_t uid = pw->pw_uid;
        qputenv("HOME",                 pw->pw_dir);
        qputenv("XDG_RUNTIME_DIR",      QString("/run/user/%1").arg(uid).toUtf8());
        qputenv("DBUS_SESSION_BUS_ADDRESS",
                 QString("unix:path=/run/user/%1/bus").arg(uid).toUtf8());
        qputenv("XDG_CONFIG_HOME",
                 QString("%1/.config").arg(pw->pw_dir).toUtf8());
        qputenv("XDG_DATA_HOME",
                 QString("%1/.local/share").arg(pw->pw_dir).toUtf8());
    }

    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME"))
        qputenv("QT_QPA_PLATFORMTHEME", "kde");

    QApplication app(argc, argv);
    app.setApplicationName("LGL System Loadout");
    app.setApplicationVersion("1.0.4");
    app.setOrganizationName("LinuxGamerLife");

    // Persist the opt-out choice so the relaunch prompt only appears once for
    // users who explicitly choose not to be asked again.
    QSettings settings;
    const bool skipPrivilegePrompt = settings.value("ui/skipPrivilegePrompt", false).toBool();

    if (geteuid() != 0) {
        if (!skipPrivilegePrompt) {
            QDialog dialog;
            dialog.setWindowTitle("Administrator access required");
            dialog.setMinimumWidth(460);

            auto *layout = new QVBoxLayout(&dialog);
            auto *title = new QLabel("<b>This requires sudo access.</b>");
            auto *body = new QLabel(
                "LGL System Loadout needs administrator privileges to install packages and make system changes.");
            body->setWordWrap(true);
            auto *dontAskAgain = new QCheckBox("Don't ask again");
            auto *buttons = new QDialogButtonBox;
            auto *cancelBtn = buttons->addButton("Cancel", QDialogButtonBox::RejectRole);
            auto *continueBtn = buttons->addButton("Continue", QDialogButtonBox::AcceptRole);

            layout->addWidget(title);
            layout->addWidget(body);
            layout->addWidget(dontAskAgain);
            layout->addWidget(buttons);

            QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
            QObject::connect(continueBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

            if (dialog.exec() != QDialog::Accepted)
                return 0;

            if (dontAskAgain->isChecked()) {
                settings.setValue("ui/skipPrivilegePrompt", true);
                settings.sync();
            }
        }

        const QString program = QFileInfo(QStringLiteral("/proc/self/exe")).symLinkTarget();
        QString resolvedProgram = program;
        if (resolvedProgram.isEmpty())
            resolvedProgram = QFileInfo(QString::fromLocal8Bit(argv[0])).absoluteFilePath();

        QStringList pkexecArgs;
        // pkexec starts with a sanitized environment, so forward the desktop
        // session variables Qt needs to re-open the UI correctly.
        pkexecArgs << "env";

        const auto addEnv = [&pkexecArgs](const char *name) {
            const QByteArray value = qgetenv(name);
            if (!value.isEmpty())
                pkexecArgs << QString("%1=%2").arg(name, QString::fromLocal8Bit(value));
        };

        addEnv("DISPLAY");
        addEnv("WAYLAND_DISPLAY");
        addEnv("XAUTHORITY");
        addEnv("XDG_RUNTIME_DIR");
        addEnv("DBUS_SESSION_BUS_ADDRESS");
        const QByteArray platformTheme = qgetenv("QT_QPA_PLATFORMTHEME");
        if (!platformTheme.isEmpty()) {
            pkexecArgs << QString("QT_QPA_PLATFORMTHEME=%1")
                              .arg(QString::fromLocal8Bit(platformTheme));
        } else {
            pkexecArgs << "QT_QPA_PLATFORMTHEME=kde";
        }

        pkexecArgs << resolvedProgram;
        for (int i = 1; i < argc; ++i)
            pkexecArgs << QString::fromLocal8Bit(argv[i]);

        if (QProcess::startDetached("pkexec", pkexecArgs))
            return 0;

        QTextStream(stderr)
            << "Administrator privileges required.\n"
            << "Please run: pkexec " << resolvedProgram << '\n';
        return 1;
    }

    // Increase base font size by 2pt for readability
    QFont appFont = app.font();
    appFont.setPointSize(appFont.pointSize() + 2);
    app.setFont(appFont);

    MainWizard wizard;
    wizard.show();

    return app.exec();
}
