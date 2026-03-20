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
    if (layout()) {
        QLayoutItem *item;
        while ((item = layout()->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        delete layout();
    }
    m_boxes.clear();

    auto *outerLayout = new QVBoxLayout(this);

    auto *toolbarWidget = new QWidget;
    auto *toolbar = new QHBoxLayout(toolbarWidget);
    toolbar->setContentsMargins(0,0,0,0);
    toolbar->addStretch();
    auto *allBtn  = makeToolbarBtn("Select All");
    auto *noneBtn = makeToolbarBtn("Select None");
    connect(allBtn,  &QPushButton::clicked, this, &GamingPage::selectAll);
    connect(noneBtn, &QPushButton::clicked, this, &GamingPage::selectNone);
    m_checkingLabel = new QLabel("  Checking...");
    m_checkingLabel->setStyleSheet("color: palette(highlight); font-style: italic;");
    m_checkingLabel->setVisible(true);
    auto *refreshBtn = makeToolbarBtn("Refresh");
    refreshBtn->setToolTip("Re-check installed status of all items");
    connect(refreshBtn, &QPushButton::clicked, this, [this] { initializePage(); });
    toolbar->addSpacing(8);
    toolbar->addWidget(refreshBtn);
    toolbar->addSpacing(4);
    toolbar->addWidget(m_checkingLabel);
    toolbar->addWidget(allBtn);
    toolbar->addWidget(noneBtn);
    outerLayout->addWidget(toolbarWidget);

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
        {"heroic",    "Heroic Games Launcher", "Open-source launcher for Epic Games Store and GOG.",             "heroic-games-launcher-bin"},
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
    checks.append({"heroic",     []{ return isDnfInstalled("heroic-games-launcher-bin"); }});
    checks.append({"protonup",   []{ return isFlatpakInstalled("net.davidotek.pupgui2"); }});
    checks.append({"protonplus", []{ return isFlatpakInstalled("com.vysp3r.ProtonPlus"); }});
    checks.append({"flatseal",   []{ return isFlatpakInstalled("com.github.tchx84.Flatseal"); }});

    runChecksAsync(this, checks, [this](QMap<QString,bool> results) {
        for (auto it = results.constBegin(); it != results.constEnd(); ++it) {
            if (m_boxes.contains(it.key())) {
                auto *cb = m_boxes[it.key()];
                // Update the badge label next to the checkbox
                auto *row = cb->parentWidget();
                if (row) {
                    auto *lbl = row->findChild<QLabel*>("badge");
                    if (lbl) {
                        lbl->setText(it.value() ? "[Installed]" : "[Not Installed]");
                        lbl->setStyleSheet(it.value()
                            ? "color: #3db03d; font-weight: bold; font-size: 8pt;"
                            : "color: #cc7700; font-weight: bold; font-size: 8pt;");
                    }
                }
            }
        }
        if (m_checkingLabel) m_checkingLabel->setVisible(false);
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
