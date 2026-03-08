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

private:
    MainWizard     *m_wiz;
    QLabel         *m_summaryLabel;
    QPlainTextEdit *m_errorDetail;
    QPushButton    *m_copyErrorsBtn;
    QPushButton    *m_copyLogBtn;
};
