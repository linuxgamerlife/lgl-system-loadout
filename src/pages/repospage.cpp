#include <QCheckBox>
#include <QPalette>
#include "repospage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QApplication>

ReposPage::ReposPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Repositories");
    setSubTitle("Select which repositories to enable. RPM Fusion is required for most multimedia, gaming, and codec packages.");
}

void ReposPage::initializePage()
{
    // Rebuild the page each time so Refresh can repopulate the installed-state
    // badges from a clean layout.
    clearWidgetLayout(this);
    m_boxes.clear();

    auto *outerLayout = new QVBoxLayout(this);
    auto toolbarUi = makeSelectionToolbar(this, this,
        [this] { initializePage(); },
        [this] { for (auto *cb : m_boxes) cb->setChecked(true); },
        [this] { for (auto *cb : m_boxes) cb->setChecked(false); });
    m_checkingLabel = toolbarUi.checkingLabel;
    outerLayout->addWidget(toolbarUi.widget);

    auto *inner = new QWidget; auto *layout = new QVBoxLayout(inner);
    layout->setSpacing(8);

    auto addItem = [&](const QString &key, const QString &label, const QString &desc, bool installed) {
        auto *cb = makeItemRow(inner, layout, label, installed);
        layout->addWidget(makeDescLabel(inner, desc)); layout->addSpacing(4);
        m_boxes[key] = cb;
    };

    addItem("rpmfusion_free",    "RPM Fusion Free",
            "Provides free/open-source software not in the official Fedora repos. Required for ffmpeg, VLC, codecs, and more.",
            false);
    addItem("rpmfusion_nonfree", "RPM Fusion NonFree",
            "Provides proprietary/non-free software including Steam, NVIDIA drivers, and certain codecs.",
            false);
    layout->addStretch();
    outerLayout->addWidget(inner);
    // Run install checks concurrently
    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"rpmfusion_free",    []{ return isDnfInstalled("rpmfusion-free-release"); }});
    _checks.append({"rpmfusion_nonfree", []{ return isDnfInstalled("rpmfusion-nonfree-release"); }});
    runChecksAsync(this, _checks, [this](QMap<QString,bool> results) {
        applySelectionCheckResults(m_boxes, results, m_checkingLabel);
    });
}

bool ReposPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("repos/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
