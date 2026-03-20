// commandexecutor.cpp
//
// Graceful cancellation (Step 4 / Bug 2):
//   On cancel, SIGTERM is sent first and the process is given 8 seconds to exit
//   cleanly. If it does not, SIGKILL is sent as a last resort.
//   This matches the two-stage shutdown design agreed in the 1.1.0 plan.

#include "commandexecutor.h"
#include <QDateTime>
#include <QProcess>

CommandExecutor::CommandExecutor(QObject *parent) : ICommandExecutor(parent) {}

int CommandExecutor::execute(const QString &program,
                             const QStringList &args,
                             std::function<void(const QString &line)> outputLine,
                             const std::atomic<bool> *cancelFlag)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);

    QObject::connect(&proc, &QProcess::readyReadStandardOutput, [&]() {
        const QString out = QString::fromLocal8Bit(proc.readAllStandardOutput());
        // Split on \n first to get logical lines.
        // Within each line, tools like flatpak use \r to update progress in
        // place. We only emit the final \r-separated segment so the log shows
        // the last state rather than every intermediate percentage update.
        for (const QString &chunk : out.split('\n', Qt::SkipEmptyParts)) {
            if (chunk.contains('\r')) {
                const QStringList parts = chunk.split('\r', Qt::SkipEmptyParts);
                if (!parts.isEmpty())
                    outputLine(parts.last().trimmed());
            } else {
                outputLine(chunk);
            }
        }
    });

    proc.start(program, args);

    if (!proc.waitForStarted(5000)) {
        outputLine(QString("ERROR: could not start: %1").arg(program));
        return -1;
    }

    // Poll loop — 300 ms intervals.
    // This allows cancellation to be detected promptly without spinning.
    bool   cancelInitiated    = false;
    qint64 terminateTimestamp = 0;

    while (!proc.waitForFinished(300)) {
        if (cancelFlag && cancelFlag->load()) {
            if (!cancelInitiated) {
                // Stage 1 — send SIGTERM and note the time.
                outputLine("INFO: Cancellation requested. Sending SIGTERM...");
                proc.terminate();
                cancelInitiated    = true;
                terminateTimestamp = QDateTime::currentMSecsSinceEpoch();
            } else {
                // Stage 2 — if 8 s have elapsed since SIGTERM, force-kill.
                const qint64 elapsed =
                    QDateTime::currentMSecsSinceEpoch() - terminateTimestamp;
                if (elapsed >= 8000) {
                    outputLine("INFO: Process did not exit after SIGTERM. Sending SIGKILL.");
                    proc.kill();
                    proc.waitForFinished(2000);
                    return -1;
                }
            }
        }
    }

    // Flush any remaining output after the process exits.
    const QString remaining = QString::fromLocal8Bit(proc.readAllStandardOutput());
    for (const QString &chunk : remaining.split('\n', Qt::SkipEmptyParts))
        outputLine(chunk);

    if (proc.exitStatus() != QProcess::NormalExit) return -1;
    return proc.exitCode();
}
