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

InstallPage::InstallPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Installing");
    setSubTitle("Installation is in progress. Please do not close this window.");

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(6);

    m_statusLabel = new QLabel("Preparing...");
    layout->addWidget(m_statusLabel);

    // Patience message
    auto *patienceFrame = new QFrame;
    patienceFrame->setFrameShape(QFrame::StyledPanel);
    patienceFrame->setStyleSheet("QFrame { background: palette(midlight); border-radius: 4px; padding: 2px; }");
    auto *patienceLayout = new QHBoxLayout(patienceFrame);
    patienceLayout->setContentsMargins(8, 4, 8, 4);
    auto *patienceLabel = new QLabel(
        "Some packages may take a while to download and install. "
        "If it appears to have frozen, it probably has not. "
        "Please be patient -- or go make a cup of tea!"
    );
    patienceLabel->setWordWrap(true);
    patienceLayout->addWidget(patienceLabel);
    layout->addWidget(patienceFrame);

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

InstallPage::~InstallPage() { stopWorker(); }

void InstallPage::stopWorker()
{
    if (m_worker) { m_worker->cancel(); m_worker = nullptr; }
    if (m_thread) { m_thread->quit(); m_thread->wait(5000); m_thread = nullptr; }
}

void InstallPage::initializePage()
{
    wizard()->button(QWizard::BackButton)->setEnabled(false);
    stopWorker();

    m_stepList->clear();
    m_fullLog->clear();
    m_stepDetail->clear();
    m_stepDetailLabel->setText("Click a step on the left to see its output.");
    m_stepItems.clear();
    m_stepLogs.clear();
    m_done = false;
    m_doneSteps = 0;
    m_currentStepId.clear();

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

    m_thread = new QThread(this);
    m_worker = new InstallWorker;
    m_worker->setSteps(steps);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started,            m_worker, &InstallWorker::run);
    connect(m_worker, &InstallWorker::stepStarted,  this,     &InstallPage::onStepStarted);
    connect(m_worker, &InstallWorker::stepFinished, this,     &InstallPage::onStepFinished);
    connect(m_worker, &InstallWorker::stepSkipped,  this,     &InstallPage::onStepSkipped);
    connect(m_worker, &InstallWorker::logLine,      this,     &InstallPage::onLogLine);
    connect(m_worker, &InstallWorker::allDone,      this,     &InstallPage::onAllDone);
    connect(m_worker, &InstallWorker::allDone,      m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished,           m_worker, &QObject::deleteLater);

    m_thread->start();
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
    m_fullLog->appendPlainText(line);
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
    m_done   = true;
    m_worker = nullptr;

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
