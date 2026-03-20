#pragma once
#include <QLabel>
#include <QWizardPage>
#include <QCheckBox>
#include <QMap>
#include "../mainwizard.h"
class MainWizard;
class CachyOSPage : public QWizardPage {
    Q_OBJECT
public:
    explicit CachyOSPage(MainWizard *wizard);
    void initializePage() override;
    bool validatePage() override;
    int  nextId() const override { return PAGE_SCX; }
private:
    MainWizard               *m_wiz;
    QMap<QString, QCheckBox*> m_boxes;
    QLabel                   *m_checkingLabel = nullptr;
};
