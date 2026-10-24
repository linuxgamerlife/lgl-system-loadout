#pragma once
#include <QLabel>
#include <QWizardPage>
#include <QCheckBox>
#include <QMap>
class MainWizard;
class MultimediaPage : public QWizardPage {
    Q_OBJECT
public:
    explicit MultimediaPage(MainWizard *wizard);
    void initializePage() override;
    bool validatePage() override;
private:
    MainWizard               *m_wiz;
    QMap<QString, QCheckBox*> m_boxes;
    QLabel *m_checkingLabel = nullptr;
};
