#include <QPalette>
#include "virtpage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QPushButton>
#include <QApplication>

VirtPage::VirtPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Virtualisation");
    setSubTitle("KVM/QEMU virtualisation stack and management tools.");
}

void VirtPage::initializePage()
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

    auto *biosBox = new QFrame; biosBox->setFrameShape(QFrame::StyledPanel);
    biosBox->setStyleSheet("QFrame { border: 2px solid #cc7700; border-radius: 4px; }");
    auto *biosLayout = new QVBoxLayout(biosBox);
    auto *biosLabel = new QLabel(
        "<b>⚠ Enable virtualisation in your BIOS/UEFI</b><br><br>"
        "For the best performance when running VMs, ensure hardware virtualisation (Intel VT-x/VT-d "
        "or AMD-V/AMD-Vi) is enabled in your BIOS/UEFI firmware (usually by pressing F2, F10, F12, or "
        "Del at startup). Without it, virtual machines will run significantly slower or may fail to start."
    );
    biosLabel->setWordWrap(true);
    biosLayout->addWidget(biosLabel);
    layout->addWidget(biosBox);
    layout->addSpacing(8);

    const QList<std::tuple<QString,QString,QString,QString>> items = {
        {"virtmanager",  "virt-manager",  "Graphical interface for managing KVM virtual machines.",        "virt-manager"},
        {"libvirt",      "libvirt",       "Virtualisation API and daemon.",                                "libvirt"},
        {"virt_install", "virt-install",  "Command-line tool for creating new virtual machines.",          "virt-install"},
        {"virt_viewer",  "virt-viewer",   "Lightweight viewer for VM consoles via SPICE or VNC.",          "virt-viewer"},
    };

    for (const auto &[key, label, desc, pkg] : items) {
        auto *cb = makeItemRow(inner, layout, label, false);
        layout->addWidget(makeDescLabel(inner, desc)); layout->addSpacing(2);
        m_boxes[key] = cb;
    }

    auto *noteBox = new QFrame; noteBox->setFrameShape(QFrame::StyledPanel);
    auto *noteLayout = new QVBoxLayout(noteBox);
    auto *noteLabel = new QLabel(QString(
        "<b>Note:</b> If any virtualisation package is selected, the following will happen automatically:<br>"
        "&bull; <tt>libvirtd</tt> service will be enabled and started<br>"
        "&bull; User <tt>%1</tt> will be added to the <tt>libvirt</tt> group<br>"
        "A reboot (or re-login) is required for group membership to take effect.<br><br>"
        "<i>qemu-kvm, edk2-ovmf, swtpm, and libvirt daemon components are pulled in automatically as dependencies of virt-manager.</i>"
    ).arg(m_wiz->targetUser()));
    noteLabel->setWordWrap(true);
    noteLayout->addWidget(noteLabel);
    layout->addSpacing(8); layout->addWidget(noteBox);
    layout->addStretch();
    scroll->setWidget(inner);
    outer->addWidget(scroll);
    // Run install checks concurrently
    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"virtmanager", []{ return isDnfInstalled("virt-manager"); }});
    _checks.append({"libvirt", []{ return isDnfInstalled("libvirt"); }});
    _checks.append({"virt_install", []{ return isDnfInstalled("virt-install"); }});
    _checks.append({"virt_viewer", []{ return isDnfInstalled("virt-viewer"); }});
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

bool VirtPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("virt/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
