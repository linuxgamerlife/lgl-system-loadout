#include "welcomepage.h"
#include "../mainwizard.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QApplication>
#include <unistd.h>

WelcomePage::WelcomePage(MainWizard *wizard) : QWizardPage(wizard)
{
    setTitle("Welcome to LGL System Loadout");
    setSubTitle("This wizard will guide you through setting up your Fedora system for gaming, "
                "content creation, development, and more.");

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    // Root warning
    if (geteuid() != 0) {
        auto *warnFrame = new QFrame;
        warnFrame->setFrameShape(QFrame::StyledPanel);
        warnFrame->setStyleSheet("QFrame { background: palette(highlight); border-radius: 6px; padding: 4px; }");
        auto *wl = new QVBoxLayout(warnFrame);
        auto *warnLabel = new QLabel(
            "<b>Warning: This application must be run as root.</b><br>"
            "Please restart with: <tt>pkexec lgl-system-loadout</tt>"
        );
        warnLabel->setWordWrap(true);
        wl->addWidget(warnLabel);
        layout->addWidget(warnFrame);
    }

    // System info
    auto *infoFrame = new QFrame;
    infoFrame->setFrameShape(QFrame::StyledPanel);
    auto *fl = new QVBoxLayout(infoFrame);

    auto *title = new QLabel("<h2>System Information</h2>");
    fl->addWidget(title);

    auto *fedLabel = new QLabel(
        QString("<b>Fedora Version:</b> %1").arg(wizard->fedoraVersion())
    );
    fl->addWidget(fedLabel);

    auto *userLabel = new QLabel(
        QString("<b>Target User:</b> %1  "
                "<span style='color: palette(mid);'>(user-level tools will be installed for this account)</span>")
        .arg(wizard->targetUser())
    );
    userLabel->setWordWrap(true);
    fl->addWidget(userLabel);

    layout->addWidget(infoFrame);

    // Description
    auto *desc = new QLabel(
        "<p>This wizard will let you choose exactly what to install on your system. "
        "Nothing is selected by default - every choice is yours.</p>"
        "<p>You can go back and change selections at any time before the install begins. "
        "Once installation starts it will run to completion.</p>"
        "<p><b>A working internet connection is required.</b></p>"
    );
    desc->setWordWrap(true);
    layout->addWidget(desc);

    // Checkbox explanation
    auto *checkFrame = new QFrame;
    checkFrame->setFrameShape(QFrame::StyledPanel);
    auto *cl = new QVBoxLayout(checkFrame);
    auto *checkLabel = new QLabel(
        "<b>How the checkboxes work:</b><br>"
        "Each item shows its current installed state with a coloured badge.<br>"
        "<span style='color:#3db03d;'><b>[Installed]</b></span> - "
        "this item is already on your system. Ticking it will install it again / ensure it is up to date.<br>"
        "<span style='color:#cc7700;'><b>[Not Installed]</b></span> - "
        "ticking this item will install it.<br>"
        "Items left unticked are not changed."
    );
    checkLabel->setWordWrap(true);
    cl->addWidget(checkLabel);
    layout->addWidget(checkFrame);

    layout->addStretch();
}

bool WelcomePage::isComplete() const
{
    return geteuid() == 0;
}
