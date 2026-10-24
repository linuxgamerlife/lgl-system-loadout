#pragma once
#include <QWizardPage>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <functional>
#include "../installworker.h"

class MainWizard;

class UpdatePage : public QWizardPage
{
    Q_OBJECT
public:
    explicit UpdatePage(MainWizard *wizard);
    void initializePage() override;
    bool isComplete() const override;

private:
    void startUpdate();
    void onUpdateFinished(bool success);
    void runSteps(const QList<InstallStep> &steps, std::function<void(int)> onDone);
    void promptFlatpakUpdate();
    void startFlatpakUpdate();
    void finishUpdate();

    MainWizard     *m_wiz;
    QLabel         *m_statusLabel   = nullptr;
    QPushButton    *m_updateBtn     = nullptr;
    QPushButton    *m_skipBtn       = nullptr;
    QPlainTextEdit *m_log           = nullptr;
    QProgressBar   *m_progress      = nullptr;
    QLabel         *m_flatpakLabel  = nullptr;
    QPushButton    *m_flatpakYesBtn = nullptr;
    QPushButton    *m_flatpakNoBtn  = nullptr;
    QLabel         *m_kernelLabel   = nullptr;
    QPushButton    *m_rebootBtn     = nullptr;
    QPushButton    *m_continueBtn   = nullptr;
    bool            m_complete      = false;
    bool            m_updating      = false;
    QString         m_socketPath;
    QString         m_kernelsBefore;
};
