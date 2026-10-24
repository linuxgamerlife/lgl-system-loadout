#pragma once
// commandexecutor.h
//
// ICommandExecutor — abstract interface for executing a single validated command.
//
// The real implementation (CommandExecutor) runs the command via QProcess.
// Tests use MockCommandExecutor (defined in tests/) to inject controlled
// responses without spawning real processes.
//
// Both implementations live in the helper binary only. The GUI never executes
// commands directly — it sends requests to the helper over the Unix socket.

#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>
#include <functional>

class ICommandExecutor : public QObject
{
    Q_OBJECT
public:
    explicit ICommandExecutor(QObject *parent = nullptr) : QObject(parent) {}
    ~ICommandExecutor() override = default;

    // Execute program with args. Calls outputLine for each line of output.
    // Returns exit code, or -1 if the process could not be started or was killed.
    // cancelFlag is polled periodically; if it becomes true, the process is
    // terminated gracefully (SIGTERM first, then SIGKILL after 8 s).
    virtual int execute(const QString &program,
                        const QStringList &args,
                        std::function<void(const QString &line)> outputLine,
                        const std::atomic<bool> *cancelFlag = nullptr) = 0;
};

// ---------------------------------------------------------------------------
// Real implementation — used in the production helper binary.
// ---------------------------------------------------------------------------
class CommandExecutor : public ICommandExecutor
{
    Q_OBJECT
public:
    explicit CommandExecutor(QObject *parent = nullptr);

    int execute(const QString &program,
                const QStringList &args,
                std::function<void(const QString &line)> outputLine,
                const std::atomic<bool> *cancelFlag = nullptr) override;
};
