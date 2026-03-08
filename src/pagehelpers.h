#pragma once
#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QProcess>
#include <QMap>
#include <QString>
#include <QPushButton>
#include <QSizePolicy>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>
#include <QThread>
#include <QFutureWatcher>
#include <QMutex>
#include <QtConcurrent/QtConcurrent>

// ---- Smooth scrolling QScrollArea ----
// Fixes mouse wheel scroll being intercepted by the wizard navigation
// and makes scrolling feel immediate and responsive.
class SmoothScrollArea : public QScrollArea
{
public:
    explicit SmoothScrollArea(QWidget *parent = nullptr) : QScrollArea(parent)
    {
        setFocusPolicy(Qt::NoFocus);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        verticalScrollBar()->setSingleStep(20);
    }

protected:
    void wheelEvent(QWheelEvent *e) override
    {
        // Route wheel events directly to the vertical scrollbar,
        // bypassing the wizard's own event handling.
        QScrollBar *vbar = verticalScrollBar();
        int delta = e->angleDelta().y();
        vbar->setValue(vbar->value() - delta / 2);
        e->accept();
    }
};

// ---- Install status checks ----

// Uses dnf list --installed for reliable detection even when rpm -q is ambiguous
inline bool isDnfInstalled(const QString &pkg)
{
    QProcess p;
    // rpm -q is fast (milliseconds). The bash wrapper was adding ~100ms overhead per call.
    p.start("rpm", {"-q", "--quiet", pkg});
    if (!p.waitForFinished(3000)) { p.kill(); return false; }
    if (p.exitCode() == 0) return true;
    // Fallback: glob match for packages with arch suffix (e.g. steam.x86_64)
    QProcess p2;
    p2.start("bash", {"-c", QString("rpm -qa --queryformat '%{NAME}\\n' 2>/dev/null | grep -qx '%1'").arg(pkg)});
    if (!p2.waitForFinished(3000)) { p2.kill(); return false; }
    return p2.exitCode() == 0;
}

// For packages where multiple name variants may be installed (e.g. scx-tools vs scx-tools-git)
inline bool isDnfInstalledAny(const QStringList &pkgs)
{
    for (const QString &pkg : pkgs) {
        QProcess p;
        p.start("rpm", {"-q", "--quiet", pkg});
        p.waitForFinished(6000);
        if (p.exitCode() == 0) return true;
    }
    return false;
}

inline bool isFlatpakInstalled(const QString &appId)
{
    // Check both system and user installations
    // Running as root means --system scope; we need to check user scope too
    QProcess p;
    p.start("bash", {"-c",
        QString("flatpak info --system '%1' 2>/dev/null || flatpak info --user '%1' 2>/dev/null").arg(appId)});
    p.waitForFinished(6000);
    return p.exitCode() == 0;
}

// OpenH264 support removed
inline bool isOpenH264Enabled() { return false; }

inline bool isKwinScriptInstalled(const QString &name, const QString &user)
{
    QProcess p;
    p.start("bash", {"-c",
        QString("[ -d /home/%1/.local/share/kwin/scripts/%2 ] || "
                "[ -d /usr/share/kwin/scripts/%2 ] || "
                "sudo -u %1 kpackagetool6 --type KWin/Script --show %2 2>/dev/null")
        .arg(user, name)});
    p.waitForFinished(8000);
    return p.exitCode() == 0;
}

inline bool isPlasmaAppletInstalled(const QString &name, const QString &user)
{
    // Check filesystem directly - most reliable when running as root
    // Plasmoids install to ~/.local/share/plasma/plasmoids/<name>
    QProcess p;
    p.start("bash", {"-c",
        QString("[ -d /home/%1/.local/share/plasma/plasmoids/%2 ] || "
                "[ -d /usr/share/plasma/plasmoids/%2 ] || "
                "sudo -u %1 kpackagetool6 --type Plasma/Applet --show %2 2>/dev/null")
        .arg(user, name)});
    p.waitForFinished(8000);
    return p.exitCode() == 0;
}

// ---- Async badge checker ----
// Runs a list of check functions concurrently in a thread pool,
// then calls `onDone(results)` on the main thread when all are complete.
// This eliminates the per-page lag caused by sequential rpm -q calls.
//
// Usage:
//   runChecksAsync(this, {
//       {"steam",  []{ return isDnfInstalled("steam"); }},
//       {"heroic", []{ return isFlatpakInstalled("com.heroicgameslauncher.hgl"); }},
//   }, [this](QMap<QString,bool> r) {
//       // called on main thread - populate badges here
//       m_boxes["steam"]->setProperty("installed", r["steam"]);
//   });
inline void runChecksAsync(
    QObject *context,
    QList<QPair<QString, std::function<bool()>>> checks,
    std::function<void(QMap<QString,bool>)> onDone)
{
    auto *watcher = new QFutureWatcher<QMap<QString,bool>>(context);
    QObject::connect(watcher, &QFutureWatcher<QMap<QString,bool>>::finished,
        context, [watcher, onDone]() {
            onDone(watcher->result());
            watcher->deleteLater();
        });
    auto future = QtConcurrent::run([checks]() -> QMap<QString,bool> {
        // Run all checks in parallel using QtConcurrent::blockingMappedReduced
        QMap<QString,bool> results;
        QMutex mutex;
        QtConcurrent::blockingMap(checks,
            [&](const QPair<QString, std::function<bool()>> &item) {
                bool val = item.second();
                QMutexLocker lock(&mutex);
                results[item.first] = val;
            });
        return results;
    });
    watcher->setFuture(future);
}
inline QCheckBox* makeItemRow(QWidget *parent, QLayout *layout,
                              const QString &label,
                              bool installed,
                              bool showBadge = true)
{
    auto *row    = new QWidget(parent);
    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(8);

    auto *cb = new QCheckBox(label, row);
    rowLay->addWidget(cb, 1);

    if (showBadge) {
        auto *badge = new QLabel(installed ? "[Installed]" : "[Not Installed]", row);
        badge->setMinimumWidth(110);
        badge->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        badge->setStyleSheet(installed
            ? "color: #3db03d; font-weight: bold; font-size: 8pt;"
            : "color: #cc7700; font-weight: bold; font-size: 8pt;");
        rowLay->addWidget(badge);
    }

    layout->addWidget(row);
    return cb;
}

// Toolbar button helper - consistent sizing that adapts to theme font
inline QPushButton* makeToolbarBtn(const QString &text, QWidget *parent = nullptr)
{
    auto *btn = new QPushButton(text, parent);
    btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    btn->setMinimumWidth(btn->fontMetrics().horizontalAdvance(text) + 24);
    return btn;
}

// Description label that reads well on both light and dark themes
inline QLabel* makeDescLabel(QWidget *parent, const QString &desc)
{
    auto *dl = new QLabel("    " + desc, parent);
    dl->setWordWrap(true);
    auto pal = dl->palette();
    auto c = pal.color(QPalette::WindowText);
    c.setAlphaF(0.55f);
    pal.setColor(QPalette::WindowText, c);
    dl->setPalette(pal);
    return dl;
}
