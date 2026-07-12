#pragma once
#include <QWizardPage>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

class MainWizard;

class DonePage : public QWizardPage {
    Q_OBJECT
public:
    explicit DonePage(MainWizard *wizard);
    void initializePage() override;

private slots:
    void copyErrorsToClipboard();
    void copyFullLogToClipboard();
    void rebootNow();

private:
    MainWizard     *m_wiz;
    QLabel         *m_summaryLabel  = nullptr;
    QPlainTextEdit *m_errorDetail   = nullptr;
    QPushButton    *m_copyErrorsBtn = nullptr;
    QPushButton    *m_copyLogBtn    = nullptr;
    QPushButton    *m_rebootBtn     = nullptr;
};
