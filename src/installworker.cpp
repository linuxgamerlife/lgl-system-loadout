#include "installworker.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QProcess>
#include <QUrl>

InstallWorker::InstallWorker(QObject *parent) : QObject(parent) {}

void InstallWorker::setSteps(const QList<InstallStep> &steps) { m_steps = steps; }
void InstallWorker::cancel() { m_cancelled = true; }

bool InstallWorker::runCheck(const QStringList &cmd)
{
    if (cmd.isEmpty()) return false;
    QProcess p;
    p.start(cmd.first(), cmd.mid(1));
    if (!p.waitForFinished(5000)) { p.kill(); return false; }
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

static QStringList dnfInstallTargets(const QStringList &args)
{
    // `dnf` is often wrapped by `sudo`, so pull out only the package operands
    // that come after the install/swap subcommand.
    const int opIndex = args.indexOf("install");
    if (opIndex >= 0) {
        QStringList targets;
        for (int i = opIndex + 1; i < args.size(); ++i) {
            const QString &arg = args[i];
            if (arg.startsWith('-'))
                continue;
            targets << arg;
        }
        return targets;
    }

    const int swapIndex = args.indexOf("swap");
    if (swapIndex < 0) return {};

    QStringList targets;
    bool skippedRemoveSpec = false;
    for (int i = swapIndex + 1; i < args.size(); ++i) {
        const QString &arg = args[i];
        if (arg.startsWith('-') || arg.contains("://"))
            continue;
        if (!skippedRemoveSpec) {
            skippedRemoveSpec = true;
            continue;
        }
        targets << arg;
    }
    return targets;
}

static QString rpmQueryNameForTarget(const QString &target)
{
    // Normalize a local path or download URL back into the package name that
    // `rpm -q` understands.
    QString fileName = target.contains("://")
        ? QUrl(target).fileName()
        : QFileInfo(target).fileName();

    if (fileName.endsWith(".rpm"))
        fileName.chop(4);

    // RPM filenames are typically `name-version-release.arch`; when we only
    // have the download URL, strip the trailing version-ish suffix so the
    // installed package name can still be verified.
    static const QRegularExpression kVersionSuffix(R"(^(.*?)-[0-9].*$)");
    const auto match = kVersionSuffix.match(fileName);
    if (match.hasMatch() && !match.captured(1).isEmpty())
        return match.captured(1);

    return fileName;
}

static bool packagesAreInstalled(const QStringList &pkgs)
{
    if (pkgs.isEmpty()) return false;

    for (const QString &pkg : pkgs) {
        const QString queryName = rpmQueryNameForTarget(pkg);
        if (queryName.isEmpty())
            return false;
        QProcess p;
        p.start("rpm", {"-q", "--quiet", queryName});
        if (!p.waitForFinished(4000)) { p.kill(); return false; }
        if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0)
            return false;
    }
    return true;
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

        const bool isKPackageToolCommand =
            (program == "kpackagetool6") ||
            (program == "sudo" && args.contains("kpackagetool6"));

        // Treat kpackagetool6 exit 4 (already exists) as success only when the
        // command is actually installing a package via kpackagetool6.
        if (!ok && code == 4 && isKPackageToolCommand) {
            emit logLine(QString("INFO: Already installed (exit 4), treating as success: %1")
                         .arg(step.description));
            ok = true;
        }

        // Treat dnf exit 7 (RPM scriptlet failure) as success-with-warning only
        // after verifying the intended packages are actually present.
        if (!ok && code == 7 && program == "dnf") {
            const QStringList targets = dnfInstallTargets(args);
            if (packagesAreInstalled(targets)) {
                emit logLine(QString("WARNING: RPM scriptlet failed (exit 7) but the target package(s) are installed: %1")
                             .arg(step.description));
                emit logLine("WARNING: This is usually non-fatal. Verify the app works after reboot.");
                ok = true;
            }
        }

        // Treat bash exit 7 as success-with-warning for Chrome repo setup steps
        // (Google Chrome's %post scriptlet may fail to import its signing key mid-transaction)
        if (!ok && code == 7 && program == "bash" &&
            (step.id == "chrome_repo" || step.id == "chrome")) {
            emit logLine(QString("WARNING: Chrome step exited with code 7 (scriptlet/key import issue): %1")
                         .arg(step.description));
            emit logLine("WARNING: Chrome is likely installed correctly. Verify after reboot.");
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
