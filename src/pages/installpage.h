#pragma once
#include <QWizardPage>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QThread>
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
    void stopWorker();

    MainWizard     *m_wiz;
    QListWidget    *m_stepList        = nullptr;
    QPlainTextEdit *m_fullLog         = nullptr;
    QPlainTextEdit *m_stepDetail      = nullptr;
    QLabel         *m_stepDetailLabel = nullptr;
    QProgressBar   *m_progress        = nullptr;
    QLabel         *m_statusLabel     = nullptr;

    QThread        *m_thread          = nullptr;
    InstallWorker  *m_worker          = nullptr;
    bool            m_done            = false;
    int             m_totalSteps      = 0;
    int             m_doneSteps       = 0;

    QString         m_currentStepId;
    QMap<QString, QListWidgetItem*> m_stepItems;
    QMap<QString, QString>          m_stepLogs;
};
