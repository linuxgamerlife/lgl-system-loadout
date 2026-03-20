#include <QGroupBox>
#include <QCheckBox>
#include <QPalette>
#include <QPushButton>
#include <QApplication>
#include <QRadioButton>
#include <QStackedWidget>
#include "gpupage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QScrollArea>

GpuPage::GpuPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("GPU Drivers");
    setSubTitle("Select your GPU manufacturer to see the appropriate driver options.");
}

void GpuPage::initializePage()
{
    // Rebuild the page each time so Refresh can repopulate the installed-state
    // badges from a clean layout.
    clearWidgetLayout(this);
    m_amdBoxes.clear();

    auto *outer = new QVBoxLayout(this);
    outer->setSpacing(12);

    // GPU choice
    auto *choiceBox = new QGroupBox("GPU Manufacturer");
    auto *choiceLayout = new QHBoxLayout(choiceBox);
    m_radioAmd    = new QRadioButton("AMD");
    m_radioNvidia = new QRadioButton("NVIDIA");
    m_radioSkip   = new QRadioButton("Skip / Other");
    choiceLayout->addWidget(m_radioAmd);
    choiceLayout->addWidget(m_radioNvidia);
    choiceLayout->addWidget(m_radioSkip);
    choiceLayout->addStretch();
    outer->addWidget(choiceBox);

    m_stack = new QStackedWidget;

    // Page 0: blank
    m_stack->addWidget(new QWidget);

    // Page 1: AMD
    {
        auto *amdWidget = new QWidget;
        auto *amdOuter  = new QVBoxLayout(amdWidget);

        // Select All / None for AMD
        auto toolbarUi = makeSelectionToolbar(this, this,
            [this] {
                if (m_checkingLabel) m_checkingLabel->setVisible(true);
                QApplication::processEvents();
                initializePage();
            },
            [this] { selectAllAmd(); },
            [this] { selectNoneAmd(); });
        m_checkingLabel = toolbarUi.checkingLabel;
        amdOuter->addWidget(toolbarUi.widget);

        auto *scroll = new SmoothScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
        auto *inner = new QWidget; auto *amdLayout = new QVBoxLayout(inner); amdLayout->setSpacing(6);

        auto *recBox = new QFrame; recBox->setFrameShape(QFrame::StyledPanel);
        auto *recLayout = new QVBoxLayout(recBox);
        auto *recLabel = new QLabel(
            "<b>Recommendation: install all of these for the best AMD experience.</b><br>"
            "The full Mesa and Vulkan stack is required for hardware-accelerated rendering, "
            "Vulkan/DXVK/VKD3D gaming, and GPU-accelerated video decode."
        );
        recLabel->setWordWrap(true);
        recLayout->addWidget(recLabel);
        amdLayout->addWidget(recBox);

        const QList<std::tuple<QString,QString,QString>> amdItems = {
            {"mesa_dri",      "mesa-dri-drivers",
             "Core Mesa DRI drivers - provides OpenGL support for AMD GPUs. Essential."},
            {"mesa_vulkan",   "mesa-vulkan-drivers",
             "Mesa Vulkan drivers (RADV) - AMD open-source Vulkan. Required for DXVK and VKD3D-Proton."},
            {"vulkan_loader", "vulkan-loader",
             "Vulkan ICD loader - connects applications to the Vulkan driver."},
            {"mesa_va",       "mesa-va-drivers",
             "VA-API driver for AMD - GPU-accelerated video decode in VLC, Firefox, mpv, OBS."},
            {"linux_fw",      "linux-firmware",
             "Firmware blobs for AMD GPUs. Without this, GPU may run at reduced clocks."},
        };

        for (const auto &[key, pkg, desc] : amdItems) {
            bool installed = false;
            auto *cb = makeItemRow(inner, amdLayout, pkg, installed);
            amdLayout->addWidget(makeDescLabel(inner, desc));
            amdLayout->addSpacing(2);
            m_amdBoxes[key] = cb;
        }

        amdLayout->addStretch();
        scroll->setWidget(inner);
        amdOuter->addWidget(scroll);
        m_stack->addWidget(amdWidget);
    }

    // Page 2: NVIDIA
    {
        auto *nvWidget = new QWidget;
        auto *nvLayout = new QVBoxLayout(nvWidget);
        auto *nvLabel = new QLabel(
            "<b>NVIDIA GPU</b><br><br>"
            "This wizard does not automate NVIDIA driver installation as it requires extra care "
            "to avoid conflicts with the nouveau driver.<br><br>"
            "<b>Recommended steps after this wizard completes:</b><br>"
            "1. Ensure RPM Fusion NonFree is enabled (see the Repositories page).<br>"
            "2. Run: <tt>sudo dnf install akmod-nvidia xorg-x11-drv-nvidia-cuda</tt><br>"
            "3. Wait for the kernel module to build (do not reboot immediately).<br>"
            "4. Run: <tt>modinfo -F version nvidia</tt> to confirm the module is built.<br>"
            "5. Reboot.<br><br>"
            "See <a href='https://rpmfusion.org/Howto/NVIDIA'>https://rpmfusion.org/Howto/NVIDIA</a> for full instructions."
        );
        nvLabel->setWordWrap(true);
        nvLabel->setOpenExternalLinks(true);
        nvLayout->addWidget(nvLabel);
        nvLayout->addStretch();
        m_stack->addWidget(nvWidget);
    }

    // Page 3: Skip
    {
        auto *skipWidget = new QWidget;
        auto *skipLayout = new QVBoxLayout(skipWidget);
        auto *skipLabel = new QLabel("No GPU drivers will be installed.\n\nYou can install drivers manually at any time.");
        skipLabel->setWordWrap(true);
        skipLayout->addWidget(skipLabel);
        skipLayout->addStretch();
        m_stack->addWidget(skipWidget);
    }

    outer->addWidget(m_stack);

    connect(m_radioAmd,    &QRadioButton::toggled, this, &GpuPage::onGpuChoice);
    connect(m_radioNvidia, &QRadioButton::toggled, this, &GpuPage::onGpuChoice);
    connect(m_radioSkip,   &QRadioButton::toggled, this, &GpuPage::onGpuChoice);

    // Run AMD install checks concurrently - must use actual package names, not map keys
    const QMap<QString,QString> amdPkgNames = {
        {"mesa_dri",      "mesa-dri-drivers"},
        {"mesa_vulkan",   "mesa-vulkan-drivers"},
        {"vulkan_loader", "vulkan-loader"},
        {"mesa_va",       "mesa-va-drivers"},
        {"linux_fw",      "linux-firmware"},
    };
    QList<QPair<QString, std::function<bool()>>> _checks;
    for (auto it = m_amdBoxes.constBegin(); it != m_amdBoxes.constEnd(); ++it) {
        QString key = it.key();
        QString pkg = amdPkgNames.value(key, key);
        _checks.append({key, [pkg]{ return isDnfInstalled(pkg); }});
    }
    runChecksAsync(this, _checks, [this](QMap<QString,bool> results) {
        applySelectionCheckResults(m_amdBoxes, results, m_checkingLabel);
    });
}

void GpuPage::onGpuChoice()
{
    if      (m_radioAmd->isChecked())    m_stack->setCurrentIndex(1);
    else if (m_radioNvidia->isChecked()) m_stack->setCurrentIndex(2);
    else if (m_radioSkip->isChecked())   m_stack->setCurrentIndex(3);
    else                                 m_stack->setCurrentIndex(0);
}

void GpuPage::selectAllAmd()  { for (auto *cb : m_amdBoxes) cb->setChecked(true); }
void GpuPage::selectNoneAmd() { for (auto *cb : m_amdBoxes) cb->setChecked(false); }

bool GpuPage::validatePage()
{
    QString choice = "none";
    if      (m_radioAmd->isChecked())    choice = "amd";
    else if (m_radioNvidia->isChecked()) choice = "nvidia";
    else if (m_radioSkip->isChecked())   choice = "skip";
    m_wiz->setOpt("gpu/choice", choice);

    if (choice == "amd") {
        for (auto it = m_amdBoxes.constBegin(); it != m_amdBoxes.constEnd(); ++it)
            m_wiz->setOpt(QString("gpu/amd/%1").arg(it.key()), it.value()->isChecked());
    }
    return true;
}
