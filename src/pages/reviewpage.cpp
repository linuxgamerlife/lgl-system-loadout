#include "reviewpage.h"
#include "../mainwizard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QAbstractButton>
#include <QTextEdit>

ReviewPage::ReviewPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Review Your Selections");
    setSubTitle("Please review everything below before installation begins. "
                "You can go back to any page to make changes.");

    auto *layout = new QVBoxLayout(this);

    // Disk space banner
    m_diskLabel = new QLabel;
    m_diskLabel->setWordWrap(true);
    m_diskLabel->setStyleSheet("padding: 8px; border-radius: 4px;");
    layout->addWidget(m_diskLabel);

    // Proceed anyway button (only visible when disk is low)
    m_proceedBtn = new QPushButton("Proceed Anyway (Not Recommended)");
    m_proceedBtn->setVisible(false);
    connect(m_proceedBtn, &QPushButton::clicked, this, &ReviewPage::onProceedAnyway);
    layout->addWidget(m_proceedBtn);

    m_textEdit = new QTextEdit;
    m_textEdit->setReadOnly(true);
    layout->addWidget(m_textEdit);
}

void ReviewPage::initializePage()
{
    m_proceedForced = false;

    const QList<InstallStep> steps = m_wiz->buildSteps();
    const int neededMB   = m_wiz->estimateDiskMB();
    const int availMB    = MainWizard::availableDiskMB();

    // --- Disk space check ---
    // Add 15% buffer to estimate for package manager overhead
    const int neededWithBuffer = neededMB + (neededMB * 15 / 100) + 500; // +500 for dnf cache

    if (availMB < 0) {
        // Could not determine
        m_diskLabel->setText(
            QString("<b>Estimated disk space required: ~%1 GB</b> &nbsp;|&nbsp; "
                    "Available space could not be determined. Ensure you have enough free space before continuing.")
            .arg(QString::number(neededMB / 1024.0, 'f', 1)));
        m_diskLabel->setStyleSheet("padding: 8px; background: #7a6000; color: #ffe; border-radius: 4px;");
        m_diskOk = true; // don't block if we can't check
        m_proceedBtn->setVisible(false);
    } else if (availMB < neededWithBuffer) {
        const int shortMB = neededWithBuffer - availMB;
        m_diskLabel->setText(
            QString("<b>WARNING: Insufficient disk space!</b><br>"
                    "Estimated required: <b>~%1 GB</b> (including overhead) &nbsp;|&nbsp; "
                    "Available: <b>%2 GB</b><br>"
                    "You are approximately <b>%3 MB</b> short. "
                    "Free up space before continuing, or click 'Proceed Anyway' at your own risk.")
            .arg(QString::number(neededWithBuffer / 1024.0, 'f', 1))
            .arg(QString::number(availMB / 1024.0, 'f', 1))
            .arg(shortMB));
        m_diskLabel->setStyleSheet("padding: 8px; background: #7a0000; color: #fee; border-radius: 4px; font-weight: bold;");
        m_diskOk = false;
        m_proceedBtn->setVisible(true);
    } else {
        m_diskLabel->setText(
            QString("<b>Disk space:</b> Estimated ~%1 GB required &nbsp;|&nbsp; "
                    "%2 GB available &nbsp;|&nbsp; <span style='color:#3db03d;'>OK</span>")
            .arg(QString::number(neededMB / 1024.0, 'f', 1))
            .arg(QString::number(availMB / 1024.0, 'f', 1)));
        m_diskLabel->setStyleSheet("padding: 8px; border: 1px solid palette(mid); border-radius: 4px;");
        m_diskOk = true;
        m_proceedBtn->setVisible(false);
    }

    emit completeChanged();

    // --- Step list ---
    QString html;
    html += "<html><body style='font-family: monospace;'>";
    html += QString("<p><b>Fedora %1</b> &nbsp;|&nbsp; Target user: <b>%2</b></p>")
                .arg(m_wiz->fedoraVersion())
                .arg(m_wiz->targetUser());
    html += QString("<p>%1 step(s) will be run:</p>").arg(steps.size());
    html += "<ol>";
    for (const InstallStep &step : steps)
        html += QString("<li>%1</li>").arg(step.description.toHtmlEscaped());
    html += "</ol>";

    if (steps.size() <= 1)
        html += "<p><i>Nothing selected beyond the bootstrap tools. "
                "Go back and select items to install.</i></p>";

    html += "</body></html>";
    m_textEdit->setHtml(html);
}

void ReviewPage::onProceedAnyway()
{
    m_proceedForced = true;
    m_proceedBtn->setVisible(false);
    m_diskLabel->setText(m_diskLabel->text() +
        "<br><b>Proceeding anyway as requested. Installation may fail partway through.</b>");
    m_diskLabel->setStyleSheet("padding: 8px; background: #5a4000; color: #ffe; border-radius: 4px;");
    emit completeChanged();
    // Enable the Next button
    wizard()->button(QWizard::NextButton)->setEnabled(true);
}

bool ReviewPage::isComplete() const
{
    return m_diskOk || m_proceedForced;
}
