#include <QPalette>
#include "commspage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QApplication>

CommsPage::CommsPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Communication & Productivity");
    setSubTitle("Chat, email, office, and music applications.");
}

void CommsPage::initializePage()
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

    addSection("Flatpak from Flathub");
    addItem("thunderbird", "Thunderbird  (Flatpak)", "Full-featured email, calendar, and contacts client.", false);
    addItem("discord",     "Discord  (Flatpak)",     "Voice, video, and text chat for gaming communities.",        false);
    addItem("vesktop",     "Vesktop  (Flatpak)",     "Unofficial Discord client with Vencord built-in.",           false);
    addItem("spotify",     "Spotify  (Flatpak)",     "Spotify music streaming client.",                            false);


    layout->addStretch();
    outer->addWidget(inner);
    // Run install checks concurrently
    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"thunderbird", []{ return isFlatpakInstalled("org.mozilla.Thunderbird"); }});
    _checks.append({"discord", []{ return isFlatpakInstalled("com.discordapp.Discord"); }});
    _checks.append({"vesktop", []{ return isFlatpakInstalled("dev.vencord.Vesktop"); }});
    _checks.append({"spotify", []{ return isFlatpakInstalled("com.spotify.Client"); }});

    runChecksAsync(this, _checks, [this](QMap<QString,bool> results) {
        applySelectionCheckResults(m_boxes, results, m_checkingLabel);
    });
}

bool CommsPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("comms/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
