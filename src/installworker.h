#pragma once
#include <QObject>
#include <QStringList>

struct InstallStep {
    QString     id;
    QString     description;
    QStringList command;   // command[0]=program, rest=args; empty=no-op marker
    bool        optional = false;
    // If non-empty, run this check first. If exit 0, step is skipped as already done.
    QStringList alreadyInstalledCheck;
};

class InstallWorker : public QObject {
    Q_OBJECT
public:
    explicit InstallWorker(QObject *parent = nullptr);
    void setSteps(const QList<InstallStep> &steps);

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
    bool runCheck(const QStringList &cmd);
    QList<InstallStep> m_steps;
    bool               m_cancelled = false;
};
