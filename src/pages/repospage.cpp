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
    if (layout()) {
        QLayoutItem *item;
        while ((item = layout()->takeAt(0))) { if (item->widget()) item->widget()->deleteLater(); delete item; }
        delete layout();
    }
    m_boxes.clear();

    auto *outerLayout = new QVBoxLayout(this);
    auto *toolbarWidget = new QWidget;
    auto *toolbar = new QHBoxLayout(toolbarWidget);
    toolbar->setContentsMargins(0,0,0,0);
    toolbar->addStretch();
    auto *allBtn = makeToolbarBtn("Select All");
    auto *noneBtn = makeToolbarBtn("Select None");
    connect(allBtn,  &QPushButton::clicked, this, [this]{ for (auto *cb : m_boxes) cb->setChecked(true); });
    connect(noneBtn, &QPushButton::clicked, this, [this]{ for (auto *cb : m_boxes) cb->setChecked(false); });
    // Create checking label first so the refresh lambda can reference it safely
    m_checkingLabel = new QLabel("  Checking...");
    m_checkingLabel->setStyleSheet("color: palette(highlight); font-style: italic;");
    m_checkingLabel->setVisible(true);
    auto *refreshBtn = makeToolbarBtn("Refresh");
    refreshBtn->setToolTip("Re-check installed status of all items");
    connect(refreshBtn, &QPushButton::clicked, this, [this] {
        initializePage();
    });
    toolbar->addSpacing(8);
    toolbar->addWidget(refreshBtn);
    toolbar->addSpacing(4);
    toolbar->addWidget(m_checkingLabel);
    toolbar->addWidget(allBtn); toolbar->addWidget(noneBtn);
    outerLayout->addWidget(toolbarWidget);

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
    // No badge for "upgrade" - it's an action not a package
    auto *cbUpgrade = makeItemRow(inner, layout, "Full system upgrade before installing", false, false);
    layout->addWidget(makeDescLabel(inner, "Runs 'dnf upgrade --refresh' to ensure all existing packages are up to date. Recommended."));
    layout->addSpacing(4);
    m_boxes["upgrade"] = cbUpgrade;

    layout->addStretch();
    outerLayout->addWidget(inner);
    // Run install checks concurrently
    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"rpmfusion_free",    []{ return isDnfInstalled("rpmfusion-free-release"); }});
    _checks.append({"rpmfusion_nonfree", []{ return isDnfInstalled("rpmfusion-nonfree-release"); }});
    runChecksAsync(this, _checks, [this](QMap<QString,bool> results) {
        for (auto it = results.constBegin(); it != results.constEnd(); ++it) {
            if (!m_boxes.contains(it.key())) continue;
            auto *row = m_boxes[it.key()]->parentWidget();
            if (!row) continue;
            auto *lbl = row->findChild<QLabel*>();
            if (!lbl) continue;
            lbl->setText(it.value() ? "[Installed]" : "[Not Installed]");
            lbl->setStyleSheet(it.value()
                ? "color: #3db03d; font-weight: bold; font-size: 8pt;"
                : "color: #cc7700; font-weight: bold; font-size: 8pt;");
        }
        if (m_checkingLabel) m_checkingLabel->setVisible(false);
    });
}

bool ReposPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("repos/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
