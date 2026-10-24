#pragma once
#include <QWizardPage>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QThread>
#include <QPointer>
#include <QMap>
#include <QSplitter>
#include "../installworker.h"

class MainWizard;

class InstallPage : public QWizardPage {
    Q_OBJECT
public:
    explicit InstallPage(MainWizard *wizard);
    ~InstallPage() override;

    void initializePage() override;
    bool isComplete()     const override;

private slots:
    void onStepStarted(const QString &id, const QString &description);
    void onStepFinished(const QString &id, bool success, int exitCode);
    void onStepSkipped(const QString &id, const QString &description);
    void onLogLine(const QString &line);
    void onAllDone(int errorCount);
    void onStepClicked(QListWidgetItem *item);

private:
    // Signals the worker to cancel and blocks until the thread exits.
    // Only called from the destructor — do not call from the main thread
    // while the event loop is running, as it will block the UI.
    void shutdownWorkerSync();

    MainWizard     *m_wiz;
    QListWidget    *m_stepList        = nullptr;
    QPlainTextEdit *m_fullLog         = nullptr;
    QPlainTextEdit *m_stepDetail      = nullptr;
    QLabel         *m_stepDetailLabel = nullptr;
    QProgressBar   *m_progress        = nullptr;
    QLabel         *m_statusLabel     = nullptr;

    // Raw pointers are intentionally not parented to this page.
    // Thread and worker lifetimes are managed explicitly:
    //   - worker: QThread::finished -> QObject::deleteLater
    //   - thread: QThread::finished -> QThread::deleteLater (self-cleanup)
    // QPointer<> is used so that stale pointer checks are safe after async deletion.
    QPointer<QThread>        m_thread;
    QPointer<InstallWorker>  m_worker;

    bool            m_done            = false;
    int             m_totalSteps      = 0;
    int             m_doneSteps       = 0;

    QString         m_currentStepId;
    QMap<QString, QListWidgetItem*> m_stepItems;
    QMap<QString, QString>          m_stepLogs;
};
