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
    _checks.append({"chrome", []{ return isFlatpakInstalled("com.google.Chrome"); }});
    _checks.append({"brave", []{ return isDnfInstalled("brave-browser"); }});
    _checks.append({"vivaldi",   []{ return isDnfInstalled("vivaldi-stable"); }});
    _checks.append({"librewolf", []{ return isFlatpakInstalled("io.gitlab.librewolf-community"); }});
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

bool BrowsersPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("browsers/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
