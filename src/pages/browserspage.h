#pragma once
#include <QLabel>
#include <QWizardPage>
#include <QCheckBox>
#include <QMap>
class MainWizard;
class BrowsersPage : public QWizardPage {
    Q_OBJECT
public:
    explicit BrowsersPage(MainWizard *wizard);
    void initializePage() override;
    bool validatePage() override;
private:
    MainWizard               *m_wiz;
    QMap<QString, QCheckBox*> m_boxes;
    QLabel *m_checkingLabel = nullptr;
};
