#include <QPalette>
#include "themingpage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QApplication>

ThemingPage::ThemingPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Customisation & Theming");
    setSubTitle("Customise the look and feel of your KDE Plasma desktop. These tools are KDE Plasma specific.");
}

void ThemingPage::initializePage()
{
    if (layout()) {
        QLayoutItem *i; while ((i = layout()->takeAt(0))) { if (i->widget()) i->widget()->deleteLater(); delete i; }
        delete layout();
    }
    m_boxes.clear();

    const QString tu = m_wiz->targetUser();

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

    auto *inner = new QWidget; auto *layout = new QVBoxLayout(inner); layout->setSpacing(8);

    auto *noteBox = new QFrame; noteBox->setFrameShape(QFrame::StyledPanel);
    auto *noteLayout = new QVBoxLayout(noteBox);
    auto *noteLabel = new QLabel(
        "<b>Note:</b> KZones and Panel Colorizer are KDE Plasma components. "
        "They require KDE Plasma to be installed and will be installed for user <tt>" + tu + "</tt>."
    );
    noteLabel->setWordWrap(true); noteLayout->addWidget(noteLabel); layout->addWidget(noteBox); layout->addSpacing(8);

    auto addSection = [&](const QString &t) {
        auto *l = new QLabel(QString("<b>%1</b>").arg(t)); layout->addWidget(l);
        auto *s = new QFrame; s->setFrameShape(QFrame::HLine); layout->addWidget(s);
    };
    auto addItem = [&](const QString &key, const QString &label, const QString &desc, bool installed) {
        auto *cb = makeItemRow(inner, layout, label, installed);
        layout->addWidget(makeDescLabel(inner, desc)); layout->addSpacing(4);
        m_boxes[key] = cb;
    };

    addSection("Qt Theme Engine");
    addItem("kvantum",         "kvantum",          "SVG-based theme engine for Qt applications.",                    false);
    addItem("kvantum_manager", "kvantum-manager",  "Graphical configuration tool for Kvantum.",                      false);

    addSection("KDE Plasma Extensions");
    addItem("kzones",          "KZones  (KWin Script)",
            "Window tiling zones for KWin. Define custom snap zones and snap windows with keyboard shortcuts.",
            false);
    addItem("panel_colorizer", "Panel Colorizer  (Plasmoid)",
            "Adds custom colours, gradients, and transparency effects to the Plasma panel.",
            false);

    layout->addStretch();
    outer->addWidget(inner);
    // Run install checks concurrently
    QString _tu = m_wiz ? m_wiz->targetUser() : QString();
    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"kvantum", []{ return isDnfInstalled("kvantum"); }});
    _checks.append({"kvantum_manager", []{ return isDnfInstalled("kvantum"); }});
    _checks.append({"kzones",       [_tu]{ return isKwinScriptInstalled("kzones", _tu); }});
    _checks.append({"panel_colorizer", [_tu]{ return isPlasmaAppletInstalled("com.github.luisbocanegra.panel.colorizer", _tu); }});
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

bool ThemingPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("theming/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
