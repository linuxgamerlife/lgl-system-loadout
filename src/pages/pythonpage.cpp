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

static bool isPipxToolInstalled(const QString &user, const QString &tool) {
    QProcess p;
    p.start("sudo", {"-u", user, "pipx", "list", "--short"});
    if (!p.waitForFinished(5000)) { p.kill(); return false; }
    return QString::fromUtf8(p.readAllStandardOutput()).contains(tool);
}

PythonPage::PythonPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Python & CLI Dev Tools");
    setSubTitle("Python runtime, package managers, and useful command-line utilities installed via pipx.");
}

void PythonPage::initializePage()
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

    auto addItem = [&](const QString &key, const QString &label, const QString &desc, bool installed) {
        auto *cb = makeItemRow(inner, layout, label, installed);
        layout->addWidget(makeDescLabel(inner, desc)); layout->addSpacing(4);
        m_boxes[key] = cb;
    };

    addItem("pip",     "python3-pip",   "pip - the Python package installer.",                false);
    addItem("pipx",    "pipx",          "Installs Python CLI apps in isolated environments.", false);

    auto *sep = new QFrame; sep->setFrameShape(QFrame::HLine); layout->addWidget(sep);
    auto *note = new QLabel("<i>The following are installed via pipx into the target user's environment.</i>");
    note->setWordWrap(true); layout->addWidget(note); layout->addSpacing(4);

    // These user-level tools are checked asynchronously because `pipx list`
    // has to run under the target account rather than the root wizard user.
    addItem("tldr",  "tldr  (via pipx)",   "Simplified man pages with quick examples.", false);
    addItem("ytdlp", "yt-dlp  (via pipx)", "Feature-rich audio/video downloader.",      false);

    layout->addStretch();
    outer->addWidget(inner);
    // Run install checks concurrently
    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"pip", []{ return isDnfInstalled("python3-pip"); }});
    _checks.append({"pipx", []{ return isDnfInstalled("pipx"); }});
    _checks.append({"tldr", [tu]{ return isPipxToolInstalled(tu, "tldr"); }});
    _checks.append({"ytdlp", [tu]{ return isPipxToolInstalled(tu, "yt-dlp"); }});
    runChecksAsync(this, _checks, [this](QMap<QString,bool> results) {
        applySelectionCheckResults(m_boxes, results, m_checkingLabel);
    });
}

bool PythonPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("python/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
