#include "donepage.h"
#include "../mainwizard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QClipboard>
#include <QMimeData>
#include <QApplication>
#include <QFrame>
#include <QFont>
#include <QPushButton>

DonePage::DonePage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Setup Complete");

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    m_summaryLabel = new QLabel;
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    // Error detail area (hidden when no errors)
    auto *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    layout->addWidget(sep);

    auto *detailHeader = new QLabel("<b>Failed Steps</b>");
    layout->addWidget(detailHeader);

    m_errorDetail = new QPlainTextEdit;
    m_errorDetail->setReadOnly(true);
    QFont mono("Monospace");
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSize(9);
    m_errorDetail->setFont(mono);
    layout->addWidget(m_errorDetail);

    // Buttons
    auto *btnLayout = new QHBoxLayout;

    m_copyErrorsBtn = new QPushButton("Copy Failed Steps to Clipboard");
    connect(m_copyErrorsBtn, &QPushButton::clicked, this, &DonePage::copyErrorsToClipboard);
    btnLayout->addWidget(m_copyErrorsBtn);

    m_copyLogBtn = new QPushButton("Copy Full Log to Clipboard");
    connect(m_copyLogBtn, &QPushButton::clicked, this, &DonePage::copyFullLogToClipboard);
    btnLayout->addWidget(m_copyLogBtn);

    auto *rebootBtn = new QPushButton("Reboot Now");
    connect(rebootBtn, &QPushButton::clicked, []() {
        QProcess::startDetached("pkexec", {"/usr/bin/systemctl", "reboot"});
    });
    btnLayout->addStretch();
    btnLayout->addWidget(rebootBtn);

    layout->addLayout(btnLayout);
}

void DonePage::initializePage()
{
    const int     errors      = m_wiz->getOpt("install/errorCount",  0).toInt();
    const QString failedSteps = m_wiz->getOpt("install/failedSteps", QString()).toString();

    if (errors == 0) {
        setSubTitle("Everything installed successfully.");
        m_summaryLabel->setText(
            "<p><b>All selected packages and tools have been installed.</b></p>"
            "<p>A reboot is recommended to ensure all changes take effect, "
            "especially if the CachyOS kernel, GPU drivers, or virtualisation packages were installed.</p>"
        );
        m_errorDetail->hide();
        m_copyErrorsBtn->hide();
    } else {
        setSubTitle(QString("%1 step(s) encountered errors.").arg(errors));
        m_summaryLabel->setText(
            QString("<p><b>%1 step(s) did not complete successfully.</b></p>"
                    "<p>The failed steps and their output are shown below. "
                    "You can copy them to the clipboard to investigate or report the issue. "
                    "A reboot is still recommended for any packages that installed successfully.</p>")
            .arg(errors)
        );
        m_errorDetail->setPlainText(failedSteps.isEmpty()
            ? "(No detail available)" : failedSteps);
        m_errorDetail->show();
        m_copyErrorsBtn->show();
    }
}

static void copyToClipboard(const QString &text)
{
    // Use QMimeData for reliable Wayland clipboard support.
    auto *md = new QMimeData;
    md->setText(text);
    QApplication::clipboard()->setMimeData(md, QClipboard::Clipboard);
    // Also set Selection mode for middle-click paste on X11.
    if (QApplication::clipboard()->supportsSelection()) {
        auto *mdSel = new QMimeData;
        mdSel->setText(text);
        QApplication::clipboard()->setMimeData(mdSel, QClipboard::Selection);
    }
}

void DonePage::copyErrorsToClipboard()
{
    copyToClipboard(m_wiz->getOpt("install/failedSteps", QString()).toString());
}

void DonePage::copyFullLogToClipboard()
{
    copyToClipboard(m_wiz->getOpt("install/fullLog", QString()).toString());
}
