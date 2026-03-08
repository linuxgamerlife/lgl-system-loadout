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

    const QList<std::tuple<QString,QString,QString,QString>> items = {
        {"virtmanager",  "virt-manager",                  "Graphical interface for managing KVM virtual machines.",                "virt-manager"},
        {"libvirt",      "libvirt",                       "Virtualisation API and daemon.",                                        "libvirt"},
        {"libvirt_net",  "libvirt-daemon-config-network", "Default NAT-based virtual network configuration.",                     "libvirt-daemon-config-network"},
        {"libvirt_kvm",  "libvirt-daemon-kvm",            "KVM-specific libvirt daemon for hardware-accelerated virtualisation.",  "libvirt-daemon-kvm"},
        {"qemu_kvm",     "qemu-kvm",                      "QEMU with KVM support - the hypervisor that runs virtual machines.",    "qemu-kvm"},
        {"virt_install", "virt-install",                  "Command-line tool for creating new virtual machines.",                  "virt-install"},
        {"virt_viewer",  "virt-viewer",                   "Lightweight viewer for VM consoles via SPICE or VNC.",                  "virt-viewer"},
        {"ovmf",         "edk2-ovmf",                     "UEFI firmware for VMs. Required for Windows 11 and Secure Boot.",      "edk2-ovmf"},
        {"swtpm",        "swtpm",                         "Software TPM emulator - required for Windows 11 VMs.",                  "swtpm"},
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
        "A reboot (or re-login) is required for group membership to take effect."
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
    _checks.append({"libvirt_net", []{ return isDnfInstalled("libvirt-daemon-config-network"); }});
    _checks.append({"libvirt_kvm", []{ return isDnfInstalled("libvirt-daemon-kvm"); }});
    _checks.append({"qemu_kvm", []{ return isDnfInstalled("qemu-kvm"); }});
    _checks.append({"virt_install", []{ return isDnfInstalled("virt-install"); }});
    _checks.append({"virt_viewer", []{ return isDnfInstalled("virt-viewer"); }});
    _checks.append({"ovmf", []{ return isDnfInstalled("edk2-ovmf"); }});
    _checks.append({"swtpm", []{ return isDnfInstalled("swtpm"); }});
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

bool VirtPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("virt/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
