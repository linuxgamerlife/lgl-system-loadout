#include "updatepage.h"
#include "../mainwizard.h"
#include "../installworker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QFrame>
#include <QScrollBar>
#include <QTextCursor>
#include <QThread>
#include <QProcess>
#include <QMessageBox>

UpdatePage::UpdatePage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("System Update");
    setSubTitle("It is strongly recommended to fully update your system before installing new software.");
}

void UpdatePage::initializePage()
{
    m_complete  = false;
    m_updating  = false;

    if (layout()) {
        QLayoutItem *i;
        while ((i = layout()->takeAt(0))) { if (i->widget()) i->widget()->deleteLater(); delete i; }
        delete layout();
    }

    auto *outer = new QVBoxLayout(this);
    outer->setSpacing(10);

    // Info box
    auto *infoBox    = new QFrame;
    infoBox->setFrameShape(QFrame::StyledPanel);
    auto *infoLayout = new QVBoxLayout(infoBox);
    auto *infoLabel  = new QLabel(
        "<b>Why update first?</b><br>"
        "Installing software on top of an outdated system can cause package conflicts, "
        "dependency errors, and instability. Running a full update ensures your system "
        "is in a clean, consistent state before we add anything new."
    );
    infoLabel->setWordWrap(true);
    infoLayout->addWidget(infoLabel);
    outer->addWidget(infoBox);

    // Buttons
    auto *btnWidget = new QWidget;
    auto *btnLayout = new QHBoxLayout(btnWidget);
    btnLayout->setContentsMargins(0, 0, 0, 0);

    m_updateBtn = new QPushButton("Update Now");
    m_updateBtn->setFixedHeight(34);
    m_skipBtn   = new QPushButton("Skip (not recommended)");
    m_skipBtn->setFixedHeight(34);

    connect(m_updateBtn, &QPushButton::clicked, this, &UpdatePage::startUpdate);
    connect(m_skipBtn,   &QPushButton::clicked, this, [this] {
        m_wiz->setOpt("update/include", false);
        m_complete = true;
        m_updateBtn->setEnabled(false);
        m_skipBtn->setEnabled(false);
        m_statusLabel->setText(
            "<span style='color:#cc7700;'>⚠ System update skipped. Proceed at your own risk.</span>"
        );
        emit completeChanged();
    });

    btnLayout->addWidget(m_updateBtn);
    btnLayout->addWidget(m_skipBtn);
    btnLayout->addStretch();
    outer->addWidget(btnWidget);

    m_statusLabel = new QLabel;
    m_statusLabel->setWordWrap(true);
    outer->addWidget(m_statusLabel);

    // Progress bar (hidden until update starts)
    m_progress = new QProgressBar;
    m_progress->setRange(0, 0);  // indeterminate
    m_progress->setVisible(false);
    outer->addWidget(m_progress);

    // Log output (hidden until update starts)
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setVisible(false);
    m_log->setMaximumBlockCount(5000);
    m_log->setFont(QFont("monospace"));
    outer->addWidget(m_log);

    // Flatpak update prompt (hidden until system update completes)
    m_flatpakLabel = new QLabel("Do you want to update Flatpaks?");
    m_flatpakLabel->setVisible(false);
    outer->addWidget(m_flatpakLabel);

    auto *flatpakBtnWidget = new QWidget;
    auto *flatpakBtnLayout = new QHBoxLayout(flatpakBtnWidget);
    flatpakBtnLayout->setContentsMargins(0, 0, 0, 0);

    m_flatpakYesBtn = new QPushButton("Yes");
    m_flatpakYesBtn->setVisible(false);
    m_flatpakNoBtn  = new QPushButton("No");
    m_flatpakNoBtn->setVisible(false);

    connect(m_flatpakYesBtn, &QPushButton::clicked, this, &UpdatePage::startFlatpakUpdate);
    connect(m_flatpakNoBtn,  &QPushButton::clicked, this, [this] {
        m_flatpakLabel->setVisible(false);
        m_flatpakYesBtn->setVisible(false);
        m_flatpakNoBtn->setVisible(false);
        finishUpdate();
    });

    flatpakBtnLayout->addWidget(m_flatpakYesBtn);
    flatpakBtnLayout->addWidget(m_flatpakNoBtn);
    flatpakBtnLayout->addStretch();
    outer->addWidget(flatpakBtnWidget);

    // Kernel detection / reboot area (hidden until update completes)
    m_kernelLabel = new QLabel;
    m_kernelLabel->setWordWrap(true);
    m_kernelLabel->setVisible(false);
    outer->addWidget(m_kernelLabel);

    auto *postBtnWidget = new QWidget;
    auto *postBtnLayout = new QHBoxLayout(postBtnWidget);
    postBtnLayout->setContentsMargins(0, 0, 0, 0);

    m_rebootBtn   = new QPushButton("Reboot Now");
    m_rebootBtn->setVisible(false);
    m_continueBtn = new QPushButton("Continue Anyway");
    m_continueBtn->setVisible(false);

    connect(m_rebootBtn, &QPushButton::clicked, this, [this] {
        auto reply = QMessageBox::question(this, "Reboot",
            "Please save any open files before rebooting.\n\n"
            "After rebooting, launch LGL System Loadout again to continue with your selections.\n\n"
            "Reboot now?",
            QMessageBox::Yes | QMessageBox::Cancel);
        if (reply == QMessageBox::Yes)
            QProcess::startDetached("pkexec", {"/usr/bin/systemctl", "reboot"});
    });

    connect(m_continueBtn, &QPushButton::clicked, this, [this] {
        m_complete = true;
        m_continueBtn->setEnabled(false);
        m_rebootBtn->setEnabled(false);
        emit completeChanged();
    });

    postBtnLayout->addWidget(m_rebootBtn);
    postBtnLayout->addWidget(m_continueBtn);
    postBtnLayout->addStretch();
    outer->addWidget(postBtnWidget);

    outer->addStretch();
}

void UpdatePage::startUpdate()
{
    if (m_updating) return;
    m_updating = true;

    m_updateBtn->setEnabled(false);
    m_skipBtn->setEnabled(false);
    m_statusLabel->setText("<span style='color:#888;'>Launching privileged helper…</span>");
    m_progress->setVisible(true);
    m_log->setVisible(true);

    // Snapshot kernel list before update
    QProcess snap;
    snap.start("/usr/bin/rpm", {"-q", "kernel", "kernel-cachyos", "--queryformat", "%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n"});
    snap.waitForFinished(5000);
    m_kernelsBefore = QString::fromLocal8Bit(snap.readAllStandardOutput()).trimmed();

    // Launch helper for the update session
    m_socketPath = m_wiz->launchHelper();
    if (m_socketPath.isEmpty()) {
        m_statusLabel->setText(
            "<span style='color:#cc0000;'>Error: failed to launch privileged helper. "
            "Check that pkexec and the helper binary are installed.</span>"
        );
        m_progress->setVisible(false);
        m_updateBtn->setEnabled(true);
        m_skipBtn->setEnabled(true);
        m_updating = false;
        return;
    }

    m_statusLabel->setText("<span style='color:#888;'>Updating…</span>");

    const QList<InstallStep> steps = {
        InstallStep{"system_update", "System update — dnf upgrade --refresh",
            {"/usr/bin/dnf", "-y", "upgrade", "--refresh"}}
    };
    runSteps(steps, [this](int errors) {
        onUpdateFinished(errors == 0);
        promptFlatpakUpdate();
    });
}

void UpdatePage::runSteps(const QList<InstallStep> &steps, std::function<void(int)> onDone)
{
    auto *thread = new QThread(this);
    auto *worker = new InstallWorker;
    worker->moveToThread(thread);
    worker->setSteps(steps);
    worker->setSocketPath(m_socketPath);

    connect(thread, &QThread::started,       worker, &InstallWorker::run);
    connect(worker, &InstallWorker::logLine,  this,   [this](const QString &line) {
        if (line.contains('\r')) {
            const QString last = line.section('\r', -1);
            QTextCursor c = m_log->textCursor();
            c.movePosition(QTextCursor::End);
            c.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
            c.insertText(last);
        } else {
            m_log->appendPlainText(line);
        }
        m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
    });
    connect(worker, &InstallWorker::allDone, this, [this, thread, worker, onDone](int errors) {
        thread->quit();
        thread->wait();
        worker->deleteLater();
        thread->deleteLater();
        onDone(errors);
    });

    thread->start();
}

void UpdatePage::onUpdateFinished(bool success)
{
    m_progress->setVisible(false);
    m_wiz->setOpt("update/include", false);  // already ran, don't repeat in buildSteps

    if (success) {
        m_statusLabel->setText(
            "<span style='color:#3db03d;'>✓ System update complete.</span>"
        );
    } else {
        m_statusLabel->setText(
            "<span style='color:#cc7700;'>⚠ Update completed with errors. Check the log above.</span>"
        );
    }
}

void UpdatePage::promptFlatpakUpdate()
{
    m_flatpakLabel->setVisible(true);
    m_flatpakYesBtn->setVisible(true);
    m_flatpakNoBtn->setVisible(true);
}

void UpdatePage::startFlatpakUpdate()
{
    m_flatpakLabel->setVisible(false);
    m_flatpakYesBtn->setVisible(false);
    m_flatpakNoBtn->setVisible(false);

    m_statusLabel->setText("<span style='color:#888;'>Launching privileged helper…</span>");
    m_progress->setVisible(true);

    // The helper process from the system update phase has already shut down
    // (it only ever accepts one client connection, for its whole lifetime) —
    // launch a fresh session for this phase, same as Update/Install already do.
    m_socketPath = m_wiz->launchHelper();
    if (m_socketPath.isEmpty()) {
        m_statusLabel->setText(
            "<span style='color:#cc0000;'>Error: failed to launch privileged helper. "
            "Check that pkexec and the helper binary are installed.</span>"
        );
        m_progress->setVisible(false);
        finishUpdate();
        return;
    }

    m_statusLabel->setText("<span style='color:#888;'>Updating Flatpaks…</span>");

    const QList<InstallStep> steps = {
        InstallStep{"flatpak_update", "Flatpak update",
            {"/usr/bin/flatpak", "update", "-y", "--system"},
            /*optional=*/true}
    };
    runSteps(steps, [this](int /*errors*/) {
        m_progress->setVisible(false);
        finishUpdate();
    });
}

void UpdatePage::finishUpdate()
{
    // Clear socket path so Install page gets a fresh helper session
    m_wiz->setOpt("install/socketPath", QString());

    // Kernel detection
    QProcess snap2;
    snap2.start("/usr/bin/rpm", {"-q", "kernel", "kernel-cachyos", "--queryformat", "%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n"});
    snap2.waitForFinished(5000);
    const QString kernelsAfter = QString::fromLocal8Bit(snap2.readAllStandardOutput()).trimmed();

    const bool newKernel = (kernelsAfter != m_kernelsBefore);
    if (newKernel) {
        m_kernelLabel->setText(
            "<b>⚠ A new kernel was installed.</b><br>"
            "A reboot is required before the new kernel takes effect. "
            "It is strongly recommended to reboot now rather than continuing."
        );
        m_kernelLabel->setVisible(true);
        m_rebootBtn->setVisible(true);
        m_continueBtn->setVisible(true);
    } else {
        m_complete = true;
        emit completeChanged();
    }
}

bool UpdatePage::isComplete() const
{
    return m_complete;
}
