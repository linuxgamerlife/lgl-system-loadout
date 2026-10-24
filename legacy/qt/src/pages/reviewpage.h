#pragma once
#include <QWizardPage>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>

class MainWizard;

class ReviewPage : public QWizardPage {
    Q_OBJECT
public:
    explicit ReviewPage(MainWizard *wizard);
    void initializePage() override;
    bool isComplete() const override;

private slots:
    void onProceedAnyway();

private:
    MainWizard  *m_wiz;
    QTextEdit   *m_textEdit;
    QLabel      *m_diskLabel;
    QPushButton *m_proceedBtn;
    bool         m_diskOk       = true;
    bool         m_proceedForced = false;
};
