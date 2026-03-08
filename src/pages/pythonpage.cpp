#include <QPalette>
#include "pythonpage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QApplication>
#include <QProcess>

static bool isPipxToolInstalled(const QString &user, const QString &tool) {
    QProcess p;
    p.start("sudo", {"-u", user, "pipx", "list", "--short"});
    p.waitForFinished(5000);
    return QString::fromUtf8(p.readAllStandardOutput()).contains(tool);
}

PythonPage::PythonPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Python & CLI Dev Tools");
    setSubTitle("Python runtime, package managers, and useful command-line utilities installed via pipx.");
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

    addItem("python3", "python3",       "Python 3 interpreter.",                              false);
    addItem("pip",     "python3-pip",   "pip - the Python package installer.",                false);
    addItem("pipx",    "pipx",          "Installs Python CLI apps in isolated environments.", false);

    auto *sep = new QFrame; sep->setFrameShape(QFrame::HLine); layout->addWidget(sep);
    auto *note = new QLabel("<i>The following are installed via pipx into the target user's environment.</i>");
    note->setWordWrap(true); layout->addWidget(note); layout->addSpacing(4);

    addItem("tldr",  "tldr  (via pipx)",   "Simplified man pages with quick examples.", isPipxToolInstalled(tu, "tldr"));
    addItem("ytdlp", "yt-dlp  (via pipx)", "Feature-rich audio/video downloader.",      isPipxToolInstalled(tu, "yt-dlp"));

    layout->addStretch();
    outer->addWidget(inner);
    // Run install checks concurrently
    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"python3", []{ return isDnfInstalled("python3"); }});
    _checks.append({"pip", []{ return isDnfInstalled("python3-pip"); }});
    _checks.append({"pipx", []{ return isDnfInstalled("pipx"); }});
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

bool PythonPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("python/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
