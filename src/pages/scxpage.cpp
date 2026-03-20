#include "scxpage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QPushButton>

ScxPage::ScxPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("SCX Scheduler Tools");
    setSubTitle("sched-ext (SCX) allows loading user-space CPU schedulers as BPF programs.");
}

void ScxPage::initializePage()
{
    if (layout()) {
        QLayoutItem *i; while ((i = layout()->takeAt(0))) { if (i->widget()) i->widget()->deleteLater(); delete i; }
        delete layout();
    }
    m_boxes.clear();

    auto *outer = new QVBoxLayout(this);

    auto *toolbarWidget = new QWidget;
    auto *toolbar       = new QHBoxLayout(toolbarWidget);
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->addStretch();
    auto *allBtn  = makeToolbarBtn("Select All");
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

    auto *scroll = new SmoothScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *inner  = new QWidget;
    auto *layout = new QVBoxLayout(inner);
    layout->setSpacing(8);

    auto addItem = [&](const QString &key, const QString &label,
                       const QString &desc, bool installed) {
        auto *cb = makeItemRow(inner, layout, label, installed);
        layout->addWidget(makeDescLabel(inner, desc));
        layout->addSpacing(4);
        m_boxes[key] = cb;
    };

    addItem("scx_scheds",  "scx-scheds",  "Collection of SCX schedulers (scx_lavd, scx_bpfland, scx_rusty, etc).", false);
    addItem("scx_manager", "scx-manager", "Systemd service for managing the active SCX scheduler.",                  false);
    addItem("scx_tools",   "scx-tools",   "Userspace tools for interacting with SCX schedulers.",                    false);

    layout->addStretch();
    scroll->setWidget(inner);
    outer->addWidget(scroll);

    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"scx_scheds",  []{ return isDnfInstalledAny({"scx-scheds", "scx-scheds-git"}); }});
    _checks.append({"scx_manager", []{ return isDnfInstalled("scx-manager"); }});
    _checks.append({"scx_tools",   []{ return isDnfInstalledAny({"scx-tools", "scx-tools-git"}); }});

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

bool ScxPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("cachyos/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
