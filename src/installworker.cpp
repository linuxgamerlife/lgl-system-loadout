#include "installworker.h"
#include <QProcess>

InstallWorker::InstallWorker(QObject *parent) : QObject(parent) {}

void InstallWorker::setSteps(const QList<InstallStep> &steps) { m_steps = steps; }
void InstallWorker::cancel() { m_cancelled = true; }

bool InstallWorker::runCheck(const QStringList &cmd)
{
    if (cmd.isEmpty()) return false;
    QProcess p;
    p.start(cmd.first(), cmd.mid(1));
    p.waitForFinished(5000);
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

void InstallWorker::run()
{
    int errorCount = 0;

    for (const InstallStep &step : m_steps) {
        if (m_cancelled) break;

        emit stepStarted(step.id, step.description);
        emit logLine(QString("\n==> %1").arg(step.description));

        // Pre-flight: check if already installed/done
        if (!step.alreadyInstalledCheck.isEmpty()) {
            if (runCheck(step.alreadyInstalledCheck)) {
                emit logLine(QString("INFO: Already installed, skipping: %1").arg(step.description));
                emit stepSkipped(step.id, step.description);
                continue;
            }
        }

        if (step.command.isEmpty()) {
            emit stepFinished(step.id, true, 0);
            continue;
        }

        QString     program = step.command.first();
        QStringList args    = step.command.mid(1);

        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);

        QObject::connect(&proc, &QProcess::readyReadStandardOutput, [&]() {
            QString out = QString::fromLocal8Bit(proc.readAllStandardOutput());
            for (const QString &ln : out.split('\n', Qt::SkipEmptyParts))
                emit logLine(ln);
        });

        proc.start(program, args);

        if (!proc.waitForStarted(5000)) {
            emit logLine(QString("ERROR: could not start: %1").arg(program));
            emit stepFinished(step.id, false, -1);
            if (!step.optional) errorCount++;
            continue;
        }

        while (!proc.waitForFinished(300)) {
            if (m_cancelled) { proc.kill(); break; }
        }

        int  code = proc.exitCode();
        bool ok   = (proc.exitStatus() == QProcess::NormalExit && code == 0);

        // Treat kpackagetool6 exit 4 (already exists) as success
        if (!ok && code == 4 &&
            (program == "kpackagetool6" || program == "sudo")) {
            emit logLine(QString("INFO: Already installed (exit 4), treating as success: %1")
                         .arg(step.description));
            ok = true;
        }

        // Treat dnf exit 7 (RPM scriptlet failure) as success-with-warning.
        // The package is installed; the scriptlet failing is usually non-fatal
        // (e.g. Brave's post-install desktop integration script failing in some envs).
        if (!ok && code == 7 && program == "dnf") {
            emit logLine(QString("WARNING: RPM scriptlet failed (exit 7) but package should be installed: %1")
                         .arg(step.description));
            emit logLine("WARNING: This is usually non-fatal. Verify the app works after reboot.");
            ok = true;
        }

        if (!ok) {
            emit logLine(QString("FAILED (exit %1): %2").arg(code).arg(step.description));
            if (!step.optional) errorCount++;
        } else {
            emit logLine(QString("OK: %1").arg(step.description));
        }
        emit stepFinished(step.id, ok, code);
    }

    emit allDone(errorCount);
}
