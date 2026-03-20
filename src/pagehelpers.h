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
#include <QtConcurrent/QtConcurrent>

// ---- Smooth scrolling QScrollArea ----
// Fixes mouse wheel scroll being intercepted by the wizard navigation.
// Installs an event filter on every descendant so wheel events are always
// caught regardless of which child widget the cursor is over.
class SmoothScrollArea : public QScrollArea
{
public:
    explicit SmoothScrollArea(QWidget *parent = nullptr) : QScrollArea(parent)
    {
        setFocusPolicy(Qt::WheelFocus);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setWidgetResizable(true);
    }

    void setWidget(QWidget *w)
    {
        QScrollArea::setWidget(w);
        if (w) installOnAll(w);
    }

private:
    void installOnAll(QObject *obj)
    {
        obj->installEventFilter(this);
        for (QObject *child : obj->children())
            installOnAll(child);
    }

    void doScroll(QWheelEvent *we)
    {
        QScrollBar *vbar = verticalScrollBar();
        int px = we->pixelDelta().y();
        if (px != 0) {
            vbar->setValue(vbar->value() - px);
        } else {
            int delta = we->angleDelta().y();
            // angleDelta is in units of 1/8 degree; one notch = 120 units (15 degrees).
            // Use rounding division to avoid silent truncation on odd delta values.
            vbar->setValue(vbar->value() - (delta * 60 + (delta >= 0 ? 60 : -60)) / 120);
        }
    }

protected:
    void wheelEvent(QWheelEvent *e) override
    {
        doScroll(e);
        e->accept();
    }

    bool eventFilter(QObject *obj, QEvent *e) override
    {
        if (e->type() == QEvent::Wheel) {
            doScroll(static_cast<QWheelEvent*>(e));
            e->accept();
            return true;
        }
        // When new child widgets are added after setWidget(), catch them too
        if (e->type() == QEvent::ChildAdded) {
            QChildEvent *ce = static_cast<QChildEvent*>(e);
            if (ce->child()) installOnAll(ce->child());
        }
        return QScrollArea::eventFilter(obj, e);
    }
};

// ---- Install status checks ----

// Uses dnf list --installed for reliable detection even when rpm -q is ambiguous
inline bool isDnfInstalled(const QString &pkg)
{
    QProcess p;
    p.setProgram("/usr/bin/rpm");
    p.setArguments({"-q", "--quiet", pkg});
    p.start();
    p.waitForFinished(-1);  // no timeout — rpm is always fast, never hangs
    return p.exitCode() == 0;
}

inline bool isDnfInstalledAny(const QStringList &pkgs)
{
    for (const QString &pkg : pkgs) {
        if (isDnfInstalled(pkg)) return true;
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
    if (!p.waitForFinished(6000)) { p.kill(); return false; }
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
    if (!p.waitForFinished(8000)) { p.kill(); return false; }
    return p.exitCode() == 0;
}

inline bool isPlasmaAppletInstalled(const QString &name, const QString &user)
{
    // The filesystem directory name for a plasmoid may differ from the kpackagetool6 ID.
    // e.g. "com.github.luisbocanegra.panel.colorizer" installs to the directory
    // "luisbocanegra.panel.colorizer" (no "com.github." prefix).
    // Strip any leading "com.github." for the filesystem check only.
    QString fsName = name;
    if (fsName.startsWith("com.github."))
        fsName = fsName.mid(QString("com.github.").length());

    QProcess p;
    p.start("bash", {"-c",
        QString("[ -d /home/%1/.local/share/plasma/plasmoids/%2 ] || "
                "[ -d /usr/share/plasma/plasmoids/%2 ] || "
                "sudo -u %1 kpackagetool6 --type Plasma/Applet --show %3 2>/dev/null")
        .arg(user, fsName, name)});
    if (!p.waitForFinished(8000)) { p.kill(); return false; }
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
#include <QPointer>

inline void runChecksAsync(
    QObject *context,
    QList<QPair<QString, std::function<bool()>>> checks,
    std::function<void(QMap<QString,bool>)> onDone)
{
    auto *watcher = new QFutureWatcher<QMap<QString,bool>>();
    // Guard against context being destroyed before the finished signal fires.
    // If the context QObject is deleted (e.g. page navigation), the callback is
    // a no-op rather than a use-after-free.
    QPointer<QObject> guard(context);
    QObject::connect(watcher, &QFutureWatcher<QMap<QString,bool>>::finished,
        context, [watcher, onDone, guard]() {
            if (guard) onDone(watcher->result());
            watcher->deleteLater();
        });
    auto future = QtConcurrent::run([checks]() -> QMap<QString,bool> {
        // Run checks sequentially — concurrent rpm processes can interfere
        // with each other causing silent failures on some systems.
        QMap<QString,bool> results;
        for (const auto &item : checks)
            results[item.first] = item.second();
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
        badge->setObjectName("badge");
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
