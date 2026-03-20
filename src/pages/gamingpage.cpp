#include <QCheckBox>
#include <QPalette>
#include "gamingpage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QScrollArea>
#include <QPushButton>
#include <QApplication>

GamingPage::GamingPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Gaming");
    setSubTitle("Game launchers, compatibility tools, performance overlays, and utilities.");
}

void GamingPage::initializePage()
{
    // Rebuild the page each time so Refresh can repopulate the installed-state
    // badges from a clean layout.
    clearWidgetLayout(this);
    m_boxes.clear();

    auto *outerLayout = new QVBoxLayout(this);

    auto toolbarUi = makeSelectionToolbar(this, this,
        [this] { initializePage(); },
        [this] { selectAll(); },
        [this] { selectNone(); });
    m_checkingLabel = toolbarUi.checkingLabel;
    outerLayout->addWidget(toolbarUi.widget);

    auto *scroll = new SmoothScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *inner  = new QWidget;
    auto *layout = new QVBoxLayout(inner);
    layout->setSpacing(4);

    auto addSection = [&](const QString &title) {
        if (layout->count() > 0) layout->addSpacing(6);
        auto *lbl = new QLabel(QString("<b>%1</b>").arg(title));
        layout->addWidget(lbl);
        auto *sep = new QFrame; sep->setFrameShape(QFrame::HLine);
        layout->addWidget(sep);
    };

    struct Item { QString key, label, desc, appId = {}; };
    const QList<Item> dnfItems = {
        {"steam",        "Steam",        "Valve's game distribution platform. Includes Proton for running Windows games on Linux."},
        {"lutris",       "Lutris",       "Open gaming platform for managing games from GOG, itch.io, and more."},
        {"wine",         "Wine",         "Compatibility layer for running Windows applications and games on Linux. Installs both 64-bit and 32-bit packages for full application compatibility."},
        {"protontricks", "Protontricks", "Winetricks wrapper for managing Proton-managed Steam games. Requires Steam to have been launched and fully set up at least once before use."},
        {"mangohud",     "MangoHud",     "In-game performance overlay showing FPS, CPU/GPU usage, temperatures, and frame timing."},
        {"goverlay",     "GOverlay",     "Graphical frontend for configuring MangoHud and other overlays."},
        {"vkBasalt",     "vkBasalt",     "Vulkan post-processing layer for games - adds CAS sharpening, FXAA, SMAA, and LUT support."},
    };
    const QList<Item> flatpakItems = {
        {"heroic",    "Heroic Games Launcher  (Flatpak)", "Open-source launcher for Epic Games Store and GOG.",             "com.heroicgameslauncher.hgl"},
        {"protonup",  "ProtonUp-Qt  (Flatpak)",           "GUI for managing Proton-GE and other compatibility tool versions.", "net.davidotek.pupgui2"},
        {"protonplus","ProtonPlus  (Flatpak)",            "Alternative tool for managing Proton versions.",                   "com.vysp3r.ProtonPlus"},
        {"flatseal",  "Flatseal  (Flatpak)",              "Graphical tool for managing Flatpak application permissions.",     "com.github.tchx84.Flatseal"},
    };

    // Build sections with placeholder badges (shown while async checks run)
    addSection("Game Launchers");
    for (const auto &it : QList<Item>{dnfItems[0], dnfItems[1], flatpakItems[0]}) {
        auto *cb = makeItemRow(inner, layout, it.label, false);
        layout->addWidget(makeDescLabel(inner, it.desc)); layout->addSpacing(2);
        m_boxes[it.key] = cb;
    }
    addSection("Windows Compatibility");
    for (const auto &it : QList<Item>{dnfItems[2], dnfItems[3]}) {
        auto *cb = makeItemRow(inner, layout, it.label, false);
        layout->addWidget(makeDescLabel(inner, it.desc)); layout->addSpacing(2);
        m_boxes[it.key] = cb;
    }
    addSection("Proton & Compatibility Tools");
    for (const auto &it : QList<Item>{flatpakItems[1], flatpakItems[2]}) {
        auto *cb = makeItemRow(inner, layout, it.label, false);
        layout->addWidget(makeDescLabel(inner, it.desc)); layout->addSpacing(2);
        m_boxes[it.key] = cb;
    }
    addSection("Performance & Monitoring");
    for (const auto &it : QList<Item>{dnfItems[4], dnfItems[5], dnfItems[6]}) {
        auto *cb = makeItemRow(inner, layout, it.label, false);
        layout->addWidget(makeDescLabel(inner, it.desc)); layout->addSpacing(2);
        m_boxes[it.key] = cb;
    }
    addSection("Flatpak Utilities");
    for (const auto &it : QList<Item>{flatpakItems[3]}) {
        auto *cb = makeItemRow(inner, layout, it.label, false);
        layout->addWidget(makeDescLabel(inner, it.desc)); layout->addSpacing(2);
        m_boxes[it.key] = cb;
    }

    auto *note = new QLabel(
        "<i>Flatpak and the Flathub remote will be configured automatically "
        "if any Flatpak item is selected here or elsewhere in this wizard.</i>"
    );
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();
    scroll->setWidget(inner);
    outerLayout->addWidget(scroll);

    // Run all install checks concurrently - page is shown immediately,
    // badges update when checks complete (typically < 1 second)
    QList<QPair<QString, std::function<bool()>>> checks;
    for (const auto &it : dnfItems)
        checks.append({it.key, [key=it.key]{ return isDnfInstalled(key); }});
    checks.append({"heroic",     []{ return isFlatpakInstalled("com.heroicgameslauncher.hgl"); }});
    checks.append({"protonup",   []{ return isFlatpakInstalled("net.davidotek.pupgui2"); }});
    checks.append({"protonplus", []{ return isFlatpakInstalled("com.vysp3r.ProtonPlus"); }});
    checks.append({"flatseal",   []{ return isFlatpakInstalled("com.github.tchx84.Flatseal"); }});

    runChecksAsync(this, checks, [this](QMap<QString,bool> results) {
        applySelectionCheckResults(m_boxes, results, m_checkingLabel);
    });
}

void GamingPage::selectAll()  { for (auto *cb : m_boxes) cb->setChecked(true); }
void GamingPage::selectNone() { for (auto *cb : m_boxes) cb->setChecked(false); }

bool GamingPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("gaming/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
