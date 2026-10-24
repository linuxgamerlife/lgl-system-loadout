#include <QPalette>
#include "cachyospage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QPushButton>
#include <QApplication>

CachyOSPage::CachyOSPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("CachyOS Kernel");
    setSubTitle("A performance-optimised kernel with improved scheduling, lower latency, and better gaming support.");
}

void CachyOSPage::initializePage()
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

    auto *infoBox    = new QFrame;
    infoBox->setFrameShape(QFrame::StyledPanel);
    auto *infoLayout = new QVBoxLayout(infoBox);
    auto *infoLabel  = new QLabel(
        "<b>About the CachyOS Kernel</b><br><br>"
        "Performance-optimised kernel including BORE scheduler, LATENCY-NICE patches, BBRv3 TCP, "
        "FUTEX2/winesync, and upstream perf patches. The standard Fedora kernel stays installed; "
        "CachyOS appears as an additional GRUB option."
    );
    infoLabel->setWordWrap(true);
    infoLayout->addWidget(infoLabel);
    layout->addWidget(infoBox);
    layout->addSpacing(4);

    auto *sbBox    = new QFrame;
    sbBox->setFrameShape(QFrame::StyledPanel);
    sbBox->setStyleSheet("QFrame { border: 2px solid #cc3300; border-radius: 4px; }");
    auto *sbLayout = new QVBoxLayout(sbBox);
    auto *sbLabel  = new QLabel(
        "<b>⚠ Secure Boot must be disabled</b><br><br>"
        "The CachyOS kernel is not signed with a Microsoft-trusted key. If Secure Boot is enabled "
        "in your BIOS/UEFI firmware, the system will refuse to boot the new kernel.<br><br>"
        "<b>After installing and rebooting:</b> enter your BIOS/UEFI settings (usually by pressing "
        "F2, F10, F12, or Del at startup) and disable Secure Boot. Save and exit, then select the "
        "CachyOS kernel from the GRUB boot menu."
    );
    sbLabel->setWordWrap(true);
    sbLayout->addWidget(sbLabel);
    layout->addWidget(sbBox);
    layout->addSpacing(8);

    auto addSection = [&](const QString &t) {
        auto *l = new QLabel(QString("<b>%1</b>").arg(t));
        layout->addWidget(l);
        auto *s = new QFrame;
        s->setFrameShape(QFrame::HLine);
        layout->addWidget(s);
    };
    auto addItem = [&](const QString &key, const QString &label,
                       const QString &desc, bool installed) {
        auto *cb = makeItemRow(inner, layout, label, installed);
        layout->addWidget(makeDescLabel(inner, desc));
        layout->addSpacing(4);
        m_boxes[key] = cb;
    };

    addSection("Kernel");
    addItem("kernel",       "kernel-cachyos",               "The CachyOS performance-optimised kernel.",              false);
    addItem("kernel_devel", "kernel-cachyos-devel-matched",  "Matched headers - required for DKMS/external modules.", false);

    layout->addStretch();
    scroll->setWidget(inner);
    outer->addWidget(scroll);

    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"kernel",       []{ return isDnfInstalled("kernel-cachyos"); }});
    _checks.append({"kernel_devel", []{ return isDnfInstalled("kernel-cachyos-devel-matched"); }});

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

bool CachyOSPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("cachyos/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
