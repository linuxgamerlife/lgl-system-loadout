#include <QPalette>
#include "contentpage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QPushButton>
#include <QApplication>

ContentPage::ContentPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Content Creation");
    setSubTitle("Tools for video, audio, graphics, and 3D creation.");
}

void ContentPage::initializePage()
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
    auto *inner = new QWidget; auto *layout = new QVBoxLayout(inner); layout->setSpacing(8);

    auto addItem = [&](const QString &key, const QString &label, const QString &desc, bool installed) {
        auto *cb = makeItemRow(inner, layout, label, installed);
        layout->addWidget(makeDescLabel(inner, desc)); layout->addSpacing(4);
        m_boxes[key] = cb;
    };

    addItem("obs",      "OBS Studio",         "Industry standard for screen recording and live streaming.", false);
    addItem("kdenlive", "Kdenlive",            "KDE's powerful non-linear video editor.",                    false);
    addItem("gimp",     "GIMP",                "Full-featured raster graphics editor.",                      false);
    addItem("inkscape", "Inkscape",            "Professional vector graphics editor.",                       false);
    addItem("audacity", "Audacity",            "Multi-track audio editor and recorder.",                     false);

    auto *sep = new QFrame; sep->setFrameShape(QFrame::HLine); layout->addWidget(sep);
    auto *note = new QLabel("<i>The following are installed via Flatpak from Flathub.</i>");
    note->setWordWrap(true); layout->addWidget(note); layout->addSpacing(4);

    addItem("blender",  "Blender  (Flatpak)", "Free and open-source 3D creation suite.", false);

    layout->addStretch();
    scroll->setWidget(inner);
    outer->addWidget(scroll);
    // Run install checks concurrently
    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"obs", []{ return isDnfInstalled("obs-studio"); }});
    _checks.append({"kdenlive", []{ return isDnfInstalled("kdenlive"); }});
    _checks.append({"gimp", []{ return isDnfInstalled("gimp"); }});
    _checks.append({"inkscape", []{ return isDnfInstalled("inkscape"); }});
    _checks.append({"audacity", []{ return isDnfInstalled("audacity"); }});
    _checks.append({"blender", []{ return isFlatpakInstalled("org.blender.Blender"); }});
    runChecksAsync(this, _checks, [this](QMap<QString,bool> results) {
        applySelectionCheckResults(m_boxes, results, m_checkingLabel);
    });
}

bool ContentPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("content/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
