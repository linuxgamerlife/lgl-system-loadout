#include <QPalette>
#include "kineticwepage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QPushButton>
#include <QApplication>

KineticWEPage::KineticWEPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("KineticWE");
    setSubTitle("A tiling KWin Wayland compositor with native window tiling.");
}

void KineticWEPage::initializePage()
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
    auto *allBtn  = makeToolbarBtn("Select All");
    auto *noneBtn = makeToolbarBtn("Select None");
    connect(allBtn,  &QPushButton::clicked, this, [this]{ for (auto *cb : m_boxes) cb->setChecked(true); });
    connect(noneBtn, &QPushButton::clicked, this, [this]{ for (auto *cb : m_boxes) cb->setChecked(false); });
    m_checkingLabel = new QLabel("  Checking...");
    m_checkingLabel->setStyleSheet("color: palette(highlight); font-style: italic;");
    m_checkingLabel->setVisible(true);
    auto *refreshBtn = makeToolbarBtn("Refresh");
    refreshBtn->setToolTip("Re-check installed status of all items");
    connect(refreshBtn, &QPushButton::clicked, this, [this]{ initializePage(); });
    toolbar->addSpacing(8);
    toolbar->addWidget(refreshBtn);
    toolbar->addSpacing(4);
    toolbar->addWidget(m_checkingLabel);
    toolbar->addWidget(allBtn);
    toolbar->addWidget(noneBtn);
    outer->addWidget(toolbarWidget);

    auto *scroll = new SmoothScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *inner  = new QWidget;
    auto *layout = new QVBoxLayout(inner);
    layout->setSpacing(8);

    auto *infoBox    = new QFrame;
    infoBox->setFrameShape(QFrame::StyledPanel);
    auto *infoLayout = new QVBoxLayout(infoBox);
    auto *infoLabel  = new QLabel(
        "<b>About KineticWE</b><br><br>"
        "A tiling KWin Wayland compositor with native window tiling, distributed via COPR. "
        "Installing KineticWE will install it alongside your current desktop environment - "
        "you will be able to choose between them from your display manager's session menu.<br><br>"
        "After installing, reboot and select <b>KineticWE</b> from your display manager's session "
        "menu, or run <tt>start-kineticwe</tt> from a TTY."
    );
    infoLabel->setWordWrap(true);
    infoLayout->addWidget(infoLabel);
    layout->addWidget(infoBox);
    layout->addSpacing(4);

    auto *warnBox    = new QFrame;
    warnBox->setFrameShape(QFrame::StyledPanel);
    warnBox->setStyleSheet("QFrame { border: 2px solid #cc3300; border-radius: 4px; }");
    auto *warnLayout = new QVBoxLayout(warnBox);
    auto *warnLabel  = new QLabel(
        "<b>⚠ IMPORTANT</b><br><br>"
        "The <tt>kineticwe</tt> package <b>provides</b> and <b>obsoletes</b> Fedora's stock "
        "<tt>kwin</tt>, <tt>kwin-common</tt>, and <tt>kwin-libs</tt> packages. It replaces the stock "
        "KWin compositor rather than installing alongside it.<br><br>"
        "This means if you already have a desktop environment with KWin enabled (KDE Plasma), "
        "KineticWE's functionality will be enabled in that session. Turn off Tiling in KDE Settings "
        "if needed.<br><br>"
        "It's rare, but if you find yourself stuck on a black screen after rebooting, reboot again."
    );
    warnLabel->setWordWrap(true);
    warnLayout->addWidget(warnLabel);
    layout->addWidget(warnBox);
    layout->addSpacing(8);

    auto addItem = [&](const QString &key, const QString &label,
                       const QString &desc, bool installed) {
        auto *cb = makeItemRow(inner, layout, label, installed);
        layout->addWidget(makeDescLabel(inner, desc));
        layout->addSpacing(4);
        m_boxes[key] = cb;
    };

    addItem("install", "KineticWE",
        "Installs kineticwe and noctalia-git via COPR, obsoleting the stock KWin packages.", false);

    layout->addStretch();
    scroll->setWidget(inner);
    outer->addWidget(scroll);

    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"install", []{ return isDnfInstalled("kineticwe"); }});

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

bool KineticWEPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("kineticwe/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
