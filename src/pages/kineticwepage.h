#pragma once
#include <QLabel>
#include <QWizardPage>
#include <QCheckBox>
#include <QMap>
class MainWizard;
class KineticWEPage : public QWizardPage {
    Q_OBJECT
public:
    explicit KineticWEPage(MainWizard *wizard);
    void initializePage() override;
    bool validatePage() override;
private:
    MainWizard               *m_wiz;
    QMap<QString, QCheckBox*> m_boxes;
    QLabel                   *m_checkingLabel = nullptr;
};
