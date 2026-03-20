#include <QApplication>
#include <QFont>
#include "mainwizard.h"

// main.cpp — LGL System Loadout GUI entry point
//
// The GUI runs as the normal user at all times.
// Privileged operations are performed by lgl-system-loadout-helper,
// which is launched via pkexec when the user clicks Update Now or Install.

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME"))
        qputenv("QT_QPA_PLATFORMTHEME", "kde");

    QApplication app(argc, argv);
    app.setApplicationName("LGL System Loadout");
    app.setApplicationVersion("1.1.0");
    app.setOrganizationName("LinuxGamerLife");

    // Increase base font size by 2pt for readability.
    QFont appFont = app.font();
    appFont.setPointSize(appFont.pointSize() + 2);
    app.setFont(appFont);

    MainWizard wizard;
    wizard.show();

    return app.exec();
}
