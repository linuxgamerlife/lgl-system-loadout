#pragma once
#include <QLabel>
#include <QWizardPage>
#include <QCheckBox>
#include <QRadioButton>
#include <QGroupBox>
#include <QStackedWidget>
#include <QMap>

class MainWizard;

class GpuPage : public QWizardPage {
    Q_OBJECT
public:
    explicit GpuPage(MainWizard *wizard);
    void initializePage() override;
    bool validatePage() override;

private slots:
    void onGpuChoice();
    void selectAllAmd();
    void selectNoneAmd();

private:
    MainWizard     *m_wiz         = nullptr;
    QRadioButton   *m_radioAmd    = nullptr;
    QRadioButton   *m_radioNvidia = nullptr;
    QRadioButton   *m_radioSkip   = nullptr;
    QStackedWidget *m_stack       = nullptr;
    QMap<QString, QCheckBox*> m_amdBoxes;
    QLabel *m_checkingLabel = nullptr;
};
