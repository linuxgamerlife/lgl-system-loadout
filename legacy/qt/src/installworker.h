#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QSet>
#include <QStringList>
#include <atomic>
#include <functional>

// InstallStep — describes a single install operation.
//
// command[0] is the program; rest are arguments.
// An empty command is a no-op marker (counts as success immediately).
//
// allowedExitCodes: non-zero exit codes to treat as success.
//   Declared per-step so exceptions are visible at the call site:
//     kpackagetool6 exit 4  — "already installed"
//     dnf exit 7            — RPM scriptlet failure (package still installed)
//     scx-* exit 1          — benign exit on some kernel configs
//
// verifyPath / expectedHash: reserved for future use.
struct InstallStep {
    QString     id;
    QString     description;
    QStringList command;              // command[0]=program, rest=args
    bool        optional             = false;
    QStringList alreadyInstalledCheck = {};  // if exit 0, step is skipped
    QSet<int>   allowedExitCodes      = {};  // non-zero exits treated as success
};

class InstallWorker : public QObject {
    Q_OBJECT
public:
    explicit InstallWorker(QObject *parent = nullptr);

    void setSteps(const QList<InstallStep> &steps);
    void setSocketPath(const QString &path);

public slots:
    void run();
    void cancel();

signals:
    void stepStarted(const QString &id, const QString &description);
    void stepFinished(const QString &id, bool success, int exitCode);
    void stepSkipped(const QString &id, const QString &description);
    void logLine(const QString &line);
    void allDone(int errorCount);

private:
    bool runCheck(QLocalSocket &sock, const QStringList &cmd);
    int  sendRequest(QLocalSocket &sock, const QJsonObject &msg,
                     std::function<void(const QString &)> outputCallback);

    QList<InstallStep>  m_steps;
    QString             m_socketPath;

    // Written from main thread (cancel()), read from worker thread (run()).
    // std::atomic<bool> prevents a data race.
    std::atomic<bool>   m_cancelled{false};

    int                 m_requestCounter = 0;
};
