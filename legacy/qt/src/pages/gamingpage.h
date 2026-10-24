#pragma once
#include <QLabel>
#include <QWizardPage>
#include <QCheckBox>
#include <QMap>

class MainWizard;

class GamingPage : public QWizardPage {
    Q_OBJECT
public:
    explicit GamingPage(MainWizard *wizard);
    void initializePage() override;
    bool validatePage() override;

private slots:
    void selectAll();
    void selectNone();

private:
    MainWizard               *m_wiz;
    QMap<QString, QCheckBox*> m_boxes;
    QLabel *m_checkingLabel = nullptr;
};
