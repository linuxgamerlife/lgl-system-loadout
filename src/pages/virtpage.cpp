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
        applySelectionCheckResults(m_boxes, results, m_checkingLabel);
    });
}

bool VirtPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("virt/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
