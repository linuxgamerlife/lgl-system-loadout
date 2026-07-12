#include <QPalette>
#include "systemtoolspage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QPushButton>
#include <QApplication>

SystemToolsPage::SystemToolsPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("System Tools & Utilities");
    setSubTitle("Essential command-line and system utilities.");
}

void SystemToolsPage::initializePage()
{
    if (layout()) {
        QLayoutItem *i; while ((i = layout()->takeAt(0))) { if (i->widget()) i->widget()->deleteLater(); delete i; }
        delete layout();
    }
    m_boxes.clear();

    const QString tu = m_wiz->targetUser();
    auto *outer = new QVBoxLayout(this);

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
    outer->addWidget(toolbarWidget);

    auto *scroll = new SmoothScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto *inner = new QWidget; auto *layout = new QVBoxLayout(inner); layout->setSpacing(4);

    auto addSection = [&](const QString &title) {
        if (layout->count() > 0) layout->addSpacing(6);
        auto *lbl = new QLabel(QString("<b>%1</b>").arg(title));
        layout->addWidget(lbl);
        auto *sep = new QFrame; sep->setFrameShape(QFrame::HLine);
        layout->addWidget(sep);
    };

    const QList<std::tuple<QString,QString,QString>> cliItems = {
        {"fastfetch",     "fastfetch",     "Fast system information tool (neofetch alternative)."},
        {"btop",          "btop",          "Resource monitor with a rich terminal UI."},
        {"htop",          "htop",          "Interactive process viewer."},
        {"xrdp",          "xrdp",          "Remote desktop protocol server."},
        {"cmatrix",       "cmatrix",       "Terminal screensaver in the style of \"The Matrix\"."},
        {"tldr",          "tldr  (via pipx)", "Simplified man pages with quick examples. Installs pipx first if needed."},
        {"distrobox",     "distrobox",     "Run other Linux distros in containers, integrated with your desktop."},
    };
    const QList<std::tuple<QString,QString,QString>> guiItems = {
        {"timeshift",     "timeshift",           "System restore and snapshot utility."},
        {"flatseal",      "Flatseal  (Flatpak)", "Graphical tool for managing Flatpak application permissions."},
    };

    addSection("CLI");
    for (const auto &[key, label, desc] : cliItems) {
        auto *cb = makeItemRow(inner, layout, label, false);
        layout->addWidget(makeDescLabel(inner, desc)); layout->addSpacing(2);
        m_boxes[key] = cb;
    }

    addSection("GUI");
    for (const auto &[key, label, desc] : guiItems) {
        auto *cb = makeItemRow(inner, layout, label, false);
        layout->addWidget(makeDescLabel(inner, desc)); layout->addSpacing(2);
        m_boxes[key] = cb;
    }

    // System Tweaks section
    layout->addSpacing(8);
    auto *tweakLabel = new QLabel("<b>System Tweaks</b>");
    layout->addWidget(tweakLabel);
    auto *tweakSep = new QFrame; tweakSep->setFrameShape(QFrame::HLine);
    layout->addWidget(tweakSep);

    auto *nmCb = makeItemRow(inner, layout, "Disable NetworkManager-wait-online", false, false);
    layout->addWidget(makeDescLabel(inner, "Disables the NetworkManager-wait-online.service unit, which can add several seconds to boot time on systems that don't need it."));
    layout->addSpacing(2);
    m_boxes["nm_wait_online"] = nmCb;

    auto *cacheCb = makeItemRow(inner, layout, "Clean DNF cache after install", false, false);
    layout->addWidget(makeDescLabel(inner, "Runs 'dnf clean all' at the end of the installation to free up cached package data."));
    layout->addSpacing(2);
    m_boxes["clean_cache"] = cacheCb;

    layout->addStretch();
    scroll->setWidget(inner);
    outer->addWidget(scroll);
    // Run install checks concurrently - one check per item in m_boxes
    QList<QPair<QString, std::function<bool()>>> _checks;
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it) {
        QString key = it.key();
        if (key == "flatseal" || key == "tldr") continue;  // checked separately below
        _checks.append({key, [key]{ return isDnfInstalled(key); }});
    }
    _checks.append({"flatseal", []{ return isFlatpakInstalled("com.github.tchx84.Flatseal"); }});
    _checks.append({"tldr", [tu]{ return isPipxToolInstalled(tu, "tldr"); }});
    runChecksAsync(this, _checks, [this](QMap<QString,bool> results) {
        for (auto it = results.constBegin(); it != results.constEnd(); ++it) {
            if (!m_boxes.contains(it.key())) continue;
            auto *row = m_boxes[it.key()]->parentWidget();
            if (!row) continue;
            auto *lbl = row->findChild<QLabel*>("badge");
            if (!lbl) continue;
            lbl->setText(it.value() ? "[Installed]" : "[Not Installed]");
            lbl->setStyleSheet(it.value()
                ? "color: #3db03d; font-weight: bold; font-size: 8pt;"
                : "color: #cc7700; font-weight: bold; font-size: 8pt;");
        }
        if (m_checkingLabel) m_checkingLabel->setVisible(false);
    });
}

bool SystemToolsPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("systools/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
