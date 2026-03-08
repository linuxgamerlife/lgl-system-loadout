#include <QApplication>
#include <QFont>
#include <QMessageBox>
#include "mainwizard.h"
#include <unistd.h>
#include <pwd.h>
#include <cstdlib>

int main(int argc, char *argv[])
{
    // Restore the real user's environment before QApplication is created,
    // so the platform theme (KDE/Breeze) loads correctly.
    // pkexec sets PKEXEC_UID; sudo -E sets SUDO_USER.
    struct passwd *pw = nullptr;

    const char *pkexecUid = std::getenv("PKEXEC_UID");
    const char *sudoUser  = std::getenv("SUDO_USER");

    if (pkexecUid && geteuid() == 0) {
        pw = getpwuid(static_cast<uid_t>(std::atoi(pkexecUid)));
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
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("LinuxGamerLife");

    // Increase base font size by 2pt for readability
    QFont appFont = app.font();
    appFont.setPointSize(appFont.pointSize() + 2);
    app.setFont(appFont);

    if (geteuid() != 0) {
        QMessageBox::warning(
            nullptr,
            "Administrator privileges required",
            "LGL System Loadout requires administrator privileges to install packages.\n\n"
            "Please launch it from your application menu, or run:\n"
            "  pkexec lgl-system-loadout"
        );
    }

    MainWizard wizard;
    wizard.show();

    return app.exec();
}
