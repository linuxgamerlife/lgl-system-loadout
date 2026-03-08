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

bool ContentPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("content/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
