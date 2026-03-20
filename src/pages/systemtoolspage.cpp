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
    // Rebuild the page each time so Refresh can repopulate the installed-state
    // badges from a clean layout.
    clearWidgetLayout(this);
    m_boxes.clear();

    auto *outer = new QVBoxLayout(this);

    auto toolbarUi = makeSelectionToolbar(this, this,
        [this] { initializePage(); },
        [this] { for (auto *cb : m_boxes) cb->setChecked(true); },
        [this] { for (auto *cb : m_boxes) cb->setChecked(false); });
    m_checkingLabel = toolbarUi.checkingLabel;
    outer->addWidget(toolbarUi.widget);

    auto *scroll = new SmoothScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto *inner = new QWidget; auto *layout = new QVBoxLayout(inner); layout->setSpacing(4);

    const QList<std::tuple<QString,QString,QString>> items = {
        {"fastfetch",     "fastfetch",     "Fast system information tool (neofetch alternative)."},
        {"btop",          "btop",          "Resource monitor with a rich terminal UI."},
        {"htop",          "htop",          "Interactive process viewer."},
        {"distrobox",     "distrobox",     "Run other Linux distros in containers, integrated with your desktop."},
        {"xrdp",          "xrdp",          "Remote desktop protocol server."},
        {"timeshift",     "timeshift",     "System restore and snapshot utility."},
    };

    for (const auto &[key, label, desc] : items) {
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
    // Run install checks concurrently - only the package rows need lookups.
    // Only the package-backed rows need rpm lookups; the tweak rows are
    // actions rather than installable packages.
    const QList<QPair<QString, QString>> packageItems = {
        {"fastfetch", "fastfetch"},
        {"btop", "btop"},
        {"htop", "htop"},
        {"distrobox", "distrobox"},
        {"xrdp", "xrdp"},
        {"timeshift", "timeshift"},
    };
    QList<QPair<QString, std::function<bool()>>> _checks;
    for (const auto &[key, pkg] : packageItems)
        _checks.append({key, [pkg]{ return isDnfInstalled(pkg); }});
    runChecksAsync(this, _checks, [this](QMap<QString,bool> results) {
        applySelectionCheckResults(m_boxes, results, m_checkingLabel);
    });
}

bool SystemToolsPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("systools/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
