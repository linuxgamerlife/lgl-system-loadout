#include "lgltoolkitpage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QPushButton>

LglToolKitPage::LglToolKitPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("LGL Tool Kit");
    setSubTitle("Small utilities from LinuxGamerLife, distributed via COPR.");
}

void LglToolKitPage::initializePage()
{
    if (layout()) {
        QLayoutItem *i; while ((i = layout()->takeAt(0))) { if (i->widget()) i->widget()->deleteLater(); delete i; }
        delete layout();
    }
    m_boxes.clear();

    auto *outer = new QVBoxLayout(this);

    auto *toolbarWidget = new QWidget;
    auto *toolbar = new QHBoxLayout(toolbarWidget);
    toolbar->setContentsMargins(0,0,0,0);
    toolbar->addStretch();
    auto *allBtn = makeToolbarBtn("Select All");
    auto *noneBtn = makeToolbarBtn("Select None");
    connect(allBtn,  &QPushButton::clicked, this, [this]{ for (auto *cb : m_boxes) cb->setChecked(true); });
    connect(noneBtn, &QPushButton::clicked, this, [this]{ for (auto *cb : m_boxes) cb->setChecked(false); });
    m_checkingLabel = new QLabel("  Checking...");
    m_checkingLabel->setStyleSheet("color: palette(highlight); font-style: italic;");
    m_checkingLabel->setVisible(true);
    auto *refreshBtn = makeToolbarBtn("Refresh");
    refreshBtn->setToolTip("Re-check installed status of all items");
    connect(refreshBtn, &QPushButton::clicked, this, [this]{ initializePage(); });
    toolbar->addSpacing(8);
    toolbar->addWidget(refreshBtn);
    toolbar->addSpacing(4);
    toolbar->addWidget(m_checkingLabel);
    toolbar->addWidget(allBtn);
    toolbar->addWidget(noneBtn);
    outer->addWidget(toolbarWidget);

    auto *scroll = new SmoothScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto *inner = new QWidget; auto *layout = new QVBoxLayout(inner); layout->setSpacing(4);

    auto addItem = [&](const QString &key, const QString &label, const QString &desc) {
        auto *cb = makeItemRow(inner, layout, label, false);
        layout->addWidget(makeDescLabel(inner, desc)); layout->addSpacing(2);
        m_boxes[key] = cb;
    };

    addItem("lgl_scxctl_manager", "LGL SCXCTL Manager",
        "Qt6 GUI for managing sched-ext BPF schedulers via scxctl. Start, stop, and switch schedulers, "
        "manage scx_loader.service autostart, and browse per-scheduler flags. "
        "Pulls in scx-tools and scx-scheds automatically if not already installed.");
    addItem("lgl_dnf_helper", "LGL DNF Helper",
        "Inspect installed RPM packages and DNF5 dependency relationships - what a package is, why it's "
        "installed, what depends on it, and which repo it came from. Early read-only prototype.");
    addItem("lgl_emoji_picker", "LGL Emoji Picker",
        "Small Qt6 emoji picker with search, recent history, and Wayland/X11 clipboard support.");
    addItem("lgl_colour_picker", "LGL Colour Picker",
        "Small Qt6 utility for sampling a colour from the screen and copying it in ready-to-paste formats.");
    addItem("lgl_powerprofile_manager", "LGL Power Profile Manager",
        "Simple, desktop-friendly interface for switching between tuned/power-profiles-daemon profiles.");

    layout->addStretch();
    scroll->setWidget(inner);
    outer->addWidget(scroll);

    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"lgl_scxctl_manager",        []{ return isDnfInstalled("lgl-scxctl-manager"); }});
    _checks.append({"lgl_dnf_helper",            []{ return isDnfInstalled("lgl-dnf-helper"); }});
    _checks.append({"lgl_emoji_picker",          []{ return isDnfInstalled("lgl-emoji-picker"); }});
    _checks.append({"lgl_colour_picker",         []{ return isDnfInstalled("lgl-colour-picker"); }});
    _checks.append({"lgl_powerprofile_manager",  []{ return isDnfInstalled("lgl-powerprofile-manager"); }});

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

bool LglToolKitPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("toolkit/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
