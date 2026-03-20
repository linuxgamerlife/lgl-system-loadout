#pragma once
#include <functional>
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
        if (!p.waitForFinished(6000)) { p.kill(); continue; }
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
    if (!p.waitForFinished(6000)) { p.kill(); return false; }
    return p.exitCode() == 0;
}

// OpenH264 support removed
inline bool isOpenH264Enabled() { return false; }

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
    auto *watcher = new QFutureWatcher<QMap<QString,bool>>(context);
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

inline void clearWidgetLayout(QWidget *page)
{
    if (!page || !page->layout()) return;
    // QWizardPage reuses the same instance, so Refresh needs a full teardown
    // before we rebuild the checkboxes and badges.
    QLayout *layout = page->layout();
    QLayoutItem *item = nullptr;
    while ((item = layout->takeAt(0))) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    delete layout;
}

struct SelectionToolbar {
    QWidget *widget = nullptr;
    QLabel *checkingLabel = nullptr;
    QPushButton *refreshBtn = nullptr;
    QPushButton *selectAllBtn = nullptr;
    QPushButton *selectNoneBtn = nullptr;
};

inline SelectionToolbar makeSelectionToolbar(
    QWidget *parent,
    QObject *context,
    const std::function<void()> &onRefresh,
    const std::function<void()> &onSelectAll,
    const std::function<void()> &onSelectNone)
{
    SelectionToolbar toolbarUi;
    toolbarUi.widget = new QWidget(parent);
    auto *toolbar = new QHBoxLayout(toolbarUi.widget);
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->addStretch();

    toolbarUi.selectAllBtn = makeToolbarBtn("Select All", toolbarUi.widget);
    toolbarUi.selectNoneBtn = makeToolbarBtn("Select None", toolbarUi.widget);
    toolbarUi.checkingLabel = new QLabel("  Checking...", toolbarUi.widget);
    toolbarUi.checkingLabel->setStyleSheet("color: palette(highlight); font-style: italic;");
    toolbarUi.checkingLabel->setVisible(true);
    toolbarUi.refreshBtn = makeToolbarBtn("Refresh", toolbarUi.widget);
    toolbarUi.refreshBtn->setToolTip("Re-check installed status of all items");

    QObject::connect(toolbarUi.selectAllBtn, &QPushButton::clicked, context, [onSelectAll] {
        if (onSelectAll) onSelectAll();
    });
    QObject::connect(toolbarUi.selectNoneBtn, &QPushButton::clicked, context, [onSelectNone] {
        if (onSelectNone) onSelectNone();
    });
    QObject::connect(toolbarUi.refreshBtn, &QPushButton::clicked, context, [onRefresh] {
        if (onRefresh) onRefresh();
    });

    toolbar->addSpacing(8);
    toolbar->addWidget(toolbarUi.refreshBtn);
    toolbar->addSpacing(4);
    toolbar->addWidget(toolbarUi.checkingLabel);
    toolbar->addWidget(toolbarUi.selectAllBtn);
    toolbar->addWidget(toolbarUi.selectNoneBtn);
    return toolbarUi;
}

inline void setInstalledBadge(QLabel *badge, bool installed)
{
    if (!badge) return;
    badge->setText(installed ? "[Installed]" : "[Not Installed]");
    badge->setStyleSheet(installed
        ? "color: #3db03d; font-weight: bold; font-size: 8pt;"
        : "color: #cc7700; font-weight: bold; font-size: 8pt;");
}

inline void applySelectionCheckResults(const QMap<QString, QCheckBox*> &boxes,
                                       const QMap<QString, bool> &results,
                                       QLabel *checkingLabel)
{
    // Only rows that have a matching key get updated; section headers and
    // tweak-only rows stay untouched.
    for (auto it = results.constBegin(); it != results.constEnd(); ++it) {
        auto boxIt = boxes.constFind(it.key());
        if (boxIt == boxes.constEnd() || !boxIt.value()) continue;
        auto *row = boxIt.value()->parentWidget();
        if (!row) continue;
        auto *lbl = row->findChild<QLabel*>("badge");
        setInstalledBadge(lbl, it.value());
    }
    if (checkingLabel) checkingLabel->setVisible(false);
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
