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
    // Rebuild the page each time so Refresh can repopulate the installed-state
    // badges from a clean layout.
    clearWidgetLayout(this);
    m_boxes.clear();

    const QString tu = m_wiz->targetUser();

    auto *outer = new QVBoxLayout(this);
    auto toolbarUi = makeSelectionToolbar(this, this,
        [this] { initializePage(); },
        [this] { for (auto *cb : m_boxes) cb->setChecked(true); },
        [this] { for (auto *cb : m_boxes) cb->setChecked(false); });
    m_checkingLabel = toolbarUi.checkingLabel;
    outer->addWidget(toolbarUi.widget);

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
    _checks.append({"kzones",       [_tu]{ return isKwinScriptInstalled("kzones", _tu); }});
    _checks.append({"panel_colorizer", [_tu]{ return isPlasmaAppletInstalled("com.github.luisbocanegra.panel.colorizer", _tu); }});
    runChecksAsync(this, _checks, [this](QMap<QString,bool> results) {
        applySelectionCheckResults(m_boxes, results, m_checkingLabel);
    });
}

bool ThemingPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("theming/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
