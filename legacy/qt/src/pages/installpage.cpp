#include <QAbstractButton>
#include <QColor>
#include <QThread>
#include <QLabel>
#include <QProgressBar>
#include <QListWidget>
#include <QPlainTextEdit>
#include "installpage.h"
#include "../mainwizard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFrame>
#include <QFont>
#include <QScrollBar>
#include <QTextCursor>

InstallPage::InstallPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Installing");
    setSubTitle("Installation is in progress. Please do not close this window. Some packages may take a while — please be patient.");

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(6);

    m_statusLabel = new QLabel("Preparing...");
    layout->addWidget(m_statusLabel);

    m_progress = new QProgressBar;
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    layout->addWidget(m_progress);

    QFont mono("Monospace");
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSize(9);

    auto *topSplitter = new QSplitter(Qt::Horizontal);

    m_stepList = new QListWidget;
    m_stepList->setMinimumWidth(220);
    m_stepList->setMaximumWidth(340);
    topSplitter->addWidget(m_stepList);

    m_fullLog = new QPlainTextEdit;
    m_fullLog->setReadOnly(true);
    m_fullLog->setFont(mono);
    m_fullLog->setPlaceholderText("Full install log will appear here...");
    topSplitter->addWidget(m_fullLog);

    topSplitter->setStretchFactor(0, 0);
    topSplitter->setStretchFactor(1, 1);

    m_stepDetailLabel = new QLabel("Click a step on the left to see its output.");
    m_stepDetailLabel->setStyleSheet("font-style: italic;");

    m_stepDetail = new QPlainTextEdit;
    m_stepDetail->setReadOnly(true);
    m_stepDetail->setFont(mono);
    m_stepDetail->setPlaceholderText("Step output will appear here when you click a step.");

    auto *vSplitter = new QSplitter(Qt::Vertical);
    vSplitter->addWidget(topSplitter);

    auto *detailWidget = new QWidget;
    auto *detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(2);
    detailLayout->addWidget(m_stepDetailLabel);
    detailLayout->addWidget(m_stepDetail);
    vSplitter->addWidget(detailWidget);

    vSplitter->setStretchFactor(0, 3);
    vSplitter->setStretchFactor(1, 1);

    layout->addWidget(vSplitter);

    connect(m_stepList, &QListWidget::itemClicked, this, &InstallPage::onStepClicked);
}

InstallPage::~InstallPage() { shutdownWorkerSync(); }

void InstallPage::shutdownWorkerSync()
{
    // This is the destructor-only synchronous shutdown path.
    // We must block until the thread exits so Qt does not attempt to deliver
    // queued signals to a destroyed InstallPage after we return.
    if (m_worker) {
        m_worker->cancel();
        // m_worker is on m_thread; cancel() is thread-safe (atomic flag).
        // Do not touch m_worker further — it will be deleted via the
        // finished -> deleteLater connection once the thread exits.
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(); // Block until thread exits — safe in destructor only.
        // m_thread has no parent and will self-delete via its own
        // finished -> deleteLater connection, which fired before wait() returned.
    }
    // QPointers are now null — no further cleanup required.
}

void InstallPage::initializePage()
{
    wizard()->button(QWizard::BackButton)->setEnabled(false);

    // If a previous run is somehow still alive (e.g. re-entry during testing),
    // cancel it and let the signal-driven cleanup finish asynchronously.
    // The QPointers will be null once the objects are deleted.
    if (m_worker) m_worker->cancel();
    if (m_thread) m_thread->quit();

    m_stepList->clear();
    m_fullLog->clear();
    m_stepDetail->clear();
    m_stepDetailLabel->setText("Click a step on the left to see its output.");
    m_stepItems.clear();
    m_stepLogs.clear();
    m_done = false;
    m_doneSteps = 0;
    m_currentStepId.clear();

    // Launch the privileged helper. This triggers the polkit password prompt.
    // The helper writes its socket path to stdout; we store it for the worker.
    const QString socketPath = m_wiz->launchHelper();
    if (socketPath.isEmpty()) {
        m_statusLabel->setText("Error: failed to launch privileged helper. "
                               "Check that pkexec and the helper binary are installed.");
        m_done = true;
        emit completeChanged();
        return;
    }

    QList<InstallStep> steps = m_wiz->buildSteps();
    m_totalSteps = steps.size();
    m_progress->setRange(0, m_totalSteps);
    m_progress->setValue(0);

    for (const InstallStep &step : steps) {
        auto *item = new QListWidgetItem("[..]  " + step.description);
        item->setData(Qt::UserRole, step.id);
        m_stepList->addItem(item);
        m_stepItems[step.id] = item;
        m_stepLogs[step.id]  = QString();
    }

    // Thread has NO parent — its lifetime is managed entirely by the
    // finished -> deleteLater connection below. Giving it a parent would
    // create a race between Qt parent-child destruction and moveToThread,
    // which TSan correctly flags as a data race.
    auto *thread = new QThread;
    auto *worker = new InstallWorker;
    worker->setSteps(steps);
    worker->setSocketPath(socketPath);
    worker->moveToThread(thread);

    // Worker lifetime: deleted on the thread it lives on, after it finishes.
    connect(thread, &QThread::finished,           worker, &QObject::deleteLater);
    // Thread lifetime: self-deletes after its event loop exits.
    // No parent owns this QThread — deleteLater is the sole cleanup path.
    connect(thread, &QThread::finished,           thread, &QThread::deleteLater);

    connect(thread, &QThread::started,            worker, &InstallWorker::run);
    connect(worker, &InstallWorker::stepStarted,  this,   &InstallPage::onStepStarted);
    connect(worker, &InstallWorker::stepFinished, this,   &InstallPage::onStepFinished);
    connect(worker, &InstallWorker::stepSkipped,  this,   &InstallPage::onStepSkipped);
    connect(worker, &InstallWorker::logLine,      this,   &InstallPage::onLogLine);
    connect(worker, &InstallWorker::allDone,      this,   &InstallPage::onAllDone);
    // Worker signals thread to stop its event loop when done.
    connect(worker, &InstallWorker::allDone,      thread, &QThread::quit);

    // Store as QPointers so any post-deletion access is a safe null check.
    m_thread = thread;
    m_worker = worker;

    thread->start();
}

void InstallPage::onStepStarted(const QString &id, const QString &description)
{
    m_currentStepId = id;
    m_statusLabel->setText(QString("Running: %1").arg(description));
    if (m_stepItems.contains(id)) {
        m_stepItems[id]->setText(" >>   " + description);
        m_stepList->scrollToItem(m_stepItems[id]);
    }
}

void InstallPage::onStepFinished(const QString &id, bool success, int exitCode)
{
    m_doneSteps++;
    m_progress->setValue(m_doneSteps);

    if (m_stepItems.contains(id)) {
        const QString prefix = success ? "[OK]  " : "[!!]  ";
        m_stepItems[id]->setText(prefix + m_stepItems[id]->text().mid(6));
        m_stepItems[id]->setForeground(success ? QColor(60, 180, 60) : QColor(200, 60, 60));
    }

    if (m_stepLogs.contains(id))
        m_stepLogs[id] += success
            ? QString("\n[Exit 0 - OK]")
            : QString("\n[Exit %1 - FAILED]").arg(exitCode);

    QListWidgetItem *sel = m_stepList->currentItem();
    if (sel && sel->data(Qt::UserRole).toString() == id)
        onStepClicked(sel);
}

void InstallPage::onStepSkipped(const QString &id, const QString &description)
{
    m_doneSteps++;
    m_progress->setValue(m_doneSteps);

    if (m_stepItems.contains(id)) {
        m_stepItems[id]->setText("[--]  " + description);
        m_stepItems[id]->setForeground(QColor(120, 120, 180));
    }
    if (m_stepLogs.contains(id))
        m_stepLogs[id] += "[Already installed - skipped]\n";
}

void InstallPage::onLogLine(const QString &line)
{
    // Flatpak and some other tools use \r to update progress in place.
    // In QPlainTextEdit we replicate this by replacing the last block.
    if (line.contains('\r')) {
        const QString last = line.section('\r', -1);  // take text after final \r
        QTextCursor c = m_fullLog->textCursor();
        c.movePosition(QTextCursor::End);
        c.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
        c.insertText(last);
    } else {
        m_fullLog->appendPlainText(line);
    }
    m_fullLog->verticalScrollBar()->setValue(m_fullLog->verticalScrollBar()->maximum());

    if (!m_currentStepId.isEmpty() && m_stepLogs.contains(m_currentStepId))
        m_stepLogs[m_currentStepId] += line + "\n";

    QListWidgetItem *sel = m_stepList->currentItem();
    if (sel && sel->data(Qt::UserRole).toString() == m_currentStepId) {
        m_stepDetail->setPlainText(m_stepLogs[m_currentStepId]);
        m_stepDetail->verticalScrollBar()->setValue(m_stepDetail->verticalScrollBar()->maximum());
    }
}

void InstallPage::onStepClicked(QListWidgetItem *item)
{
    if (!item) return;
    const QString id       = item->data(Qt::UserRole).toString();
    const QString stepText = item->text().mid(6);

    m_stepDetailLabel->setText(QString("Output for: %1").arg(stepText));

    if (m_stepLogs.contains(id) && !m_stepLogs[id].isEmpty()) {
        m_stepDetail->setPlainText(m_stepLogs[id]);
        m_stepDetail->verticalScrollBar()->setValue(m_stepDetail->verticalScrollBar()->maximum());
    } else {
        m_stepDetail->setPlainText("(No output yet for this step)");
    }
}

void InstallPage::onAllDone(int errorCount)
{
    m_done = true;
    // Do not null m_worker here. The worker will be deleted asynchronously via
    // the QThread::finished -> QObject::deleteLater connection. The QPointer
    // will automatically become null when that deletion occurs.

    QStringList failedSteps;
    for (auto it = m_stepItems.constBegin(); it != m_stepItems.constEnd(); ++it) {
        if (it.value()->text().startsWith("[!!]")) {
            const QString id   = it.key();
            const QString desc = it.value()->text().mid(6);
            failedSteps << QString("Step: %1\n%2").arg(desc).arg(m_stepLogs.value(id).trimmed());
        }
    }
    m_wiz->setOpt("install/errorCount",  errorCount);
    m_wiz->setOpt("install/failedSteps", failedSteps.join("\n\n---\n\n"));
    m_wiz->setOpt("install/fullLog",     m_fullLog->toPlainText());

    if (errorCount == 0)
        m_statusLabel->setText("Installation complete - no errors.");
    else
        m_statusLabel->setText(
            QString("Installation complete with %1 error(s). See the log for details.").arg(errorCount));

    wizard()->button(QWizard::NextButton)->setEnabled(true);
    emit completeChanged();
}

bool InstallPage::isComplete() const { return m_done; }
