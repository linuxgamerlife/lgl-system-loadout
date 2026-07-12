#include <QPalette>
#include "pythonpage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QLineEdit>
#include <QFont>
#include <QApplication>

PythonPage::PythonPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Development Tools");
    setSubTitle("Python runtime, package managers, editors, and useful command-line utilities.");
}

void PythonPage::initializePage()
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

    auto addItem = [&](const QString &key, const QString &label, const QString &desc, bool installed) {
        auto *cb = makeItemRow(inner, layout, label, installed);
        layout->addWidget(makeDescLabel(inner, desc)); layout->addSpacing(4);
        m_boxes[key] = cb;
    };

    addItem("pip",     "python3-pip",   "pip - the Python package installer.",                false);
    addItem("pipx",    "pipx",          "Installs Python CLI apps in isolated environments.", false);

    auto *sep2 = new QFrame; sep2->setFrameShape(QFrame::HLine); layout->addWidget(sep2);
    auto *note2 = new QLabel("<i>The following are installed as Flatpaks from Flathub.</i>");
    note2->setWordWrap(true); layout->addWidget(note2); layout->addSpacing(4);

    addItem("zed",            "Zed  (Flatpak)",            "Fast, collaborative code editor.",         false);
    auto *zedNote = new QLabel(
        "<span style='color:#cc7700;'>The flatpak is not as up to date as the one on their site. "
        "To install that, run the command below in a terminal. "
        "You will need to keep this updated in-app, and it will not be updated the usual way.</span>"
    );
    zedNote->setWordWrap(true);
    layout->addWidget(zedNote);

    auto *zedCmdWidget = new QWidget;
    auto *zedCmdLayout = new QHBoxLayout(zedCmdWidget);
    zedCmdLayout->setContentsMargins(0, 0, 0, 0);
    const QString zedCmd = "curl -f https://zed.dev/install.sh | sh";
    auto *zedCmdEdit = new QLineEdit(zedCmd);
    zedCmdEdit->setReadOnly(true);
    zedCmdEdit->setFont(QFont("monospace"));
    auto *zedCopyBtn = new QPushButton("Copy");
    connect(zedCopyBtn, &QPushButton::clicked, this, [zedCmd] { copyToClipboard(zedCmd); });
    zedCmdLayout->addWidget(zedCmdEdit, 1);
    zedCmdLayout->addWidget(zedCopyBtn);
    layout->addWidget(zedCmdWidget);

    auto *zedDevNote = new QLabel;
    zedDevNote->setWordWrap(true);
    zedDevNote->setVisible(false);
    layout->addWidget(zedDevNote);
    layout->addSpacing(4);

    addItem("github_desktop", "GitHub Desktop  (Flatpak)", "Graphical client for managing GitHub repositories.", false);

    layout->addStretch();
    outer->addWidget(inner);
    // Run install checks concurrently
    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"pip", []{ return isDnfInstalled("python3-pip"); }});
    _checks.append({"pipx", []{ return isDnfInstalled("pipx"); }});
    _checks.append({"zed", []{ return isFlatpakInstalled("dev.zed.Zed"); }});
    _checks.append({"zed_dev", [tu]{ return isZedDevInstalled(tu); }});
    _checks.append({"github_desktop", []{ return isFlatpakInstalled("io.github.shiftey.Desktop"); }});
    runChecksAsync(this, _checks, [this, zedDevNote](QMap<QString,bool> results) {
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
        if (results.value("zed_dev", false)) {
            zedDevNote->setText(
                "<span style='color:#3db03d;'>✓ The dev-site version of Zed is installed "
                "(~/.local/bin/zed).</span>"
            );
            zedDevNote->setVisible(true);
        }
        if (m_checkingLabel) m_checkingLabel->setVisible(false);
    });
}

bool PythonPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("python/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
