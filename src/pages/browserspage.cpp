#include <QPalette>
#include "browserspage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QApplication>

BrowsersPage::BrowsersPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Browsers");
    setSubTitle("Web browsers. Third-party browsers will have their repositories added automatically.");
}

void BrowsersPage::initializePage()
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

    auto *inner = new QWidget; auto *layout = new QVBoxLayout(inner); layout->setSpacing(8);

    auto addSection = [&](const QString &t) {
        auto *l = new QLabel(QString("<b>%1</b>").arg(t)); layout->addWidget(l);
        auto *s = new QFrame; s->setFrameShape(QFrame::HLine); layout->addWidget(s);
    };
    auto addItem = [&](const QString &key, const QString &label, const QString &desc, bool installed) {
        auto *cb = makeItemRow(inner, layout, label, installed);
        layout->addWidget(makeDescLabel(inner, desc)); layout->addSpacing(4);
        m_boxes[key] = cb;
    };

    addSection("From Fedora Repos");
    addItem("firefox",  "Firefox",  "Mozilla Firefox - ships with Fedora.",               false);
    addItem("chromium", "Chromium", "Open-source Chrome base. No Google account required.", false);

    addSection("Third-Party (repo added automatically)");
    addItem("chrome",  "Google Chrome", "Full Chrome with Widevine DRM and Google sync.",  false);
    addItem("brave",   "Brave",         "Privacy-focused browser with built-in ad blocking.", false);
    addItem("vivaldi",    "Vivaldi",                    "Highly customisable Chromium-based browser.",                   false);

    addSection("Privacy-focused (Flatpak from Flathub)");
    addItem("librewolf", "LibreWolf  (Flatpak)", "Firefox fork focused on privacy, security, and freedom.", false);

    layout->addStretch();
    outer->addWidget(inner);
    // Run install checks concurrently - page shows immediately, badges update async
    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"firefox", []{ return isDnfInstalled("firefox"); }});
    _checks.append({"chromium", []{ return isDnfInstalled("chromium"); }});
    _checks.append({"chrome", []{ return isDnfInstalled("google-chrome-stable"); }});
    _checks.append({"brave", []{ return isDnfInstalled("brave-browser"); }});
    _checks.append({"vivaldi",   []{ return isDnfInstalled("vivaldi-stable"); }});
    _checks.append({"librewolf", []{ return isFlatpakInstalled("io.gitlab.librewolf-community"); }});
    runChecksAsync(this, _checks, [this](QMap<QString,bool> results) {
        applySelectionCheckResults(m_boxes, results, m_checkingLabel);
    });
}

bool BrowsersPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("browsers/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
