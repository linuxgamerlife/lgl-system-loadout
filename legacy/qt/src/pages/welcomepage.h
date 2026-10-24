#pragma once
#include <QWizardPage>

class MainWizard;

class WelcomePage : public QWizardPage {
    Q_OBJECT
public:
    explicit WelcomePage(MainWizard *wizard);
    bool isComplete() const override;
};
