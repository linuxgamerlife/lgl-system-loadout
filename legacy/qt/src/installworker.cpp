// installworker.cpp — privileged operation dispatcher
//
// Talks to lgl-system-loadout-helper over a QLocalSocket.
// Runs on its own QThread (see installpage.cpp).
//
// A single persistent connection is kept open for the entire install session.
// The helper accepts exactly one client — do not disconnect between steps.

#include "installworker.h"
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

InstallWorker::InstallWorker(QObject *parent) : QObject(parent) {}

void InstallWorker::setSteps(const QList<InstallStep> &steps) { m_steps = steps; }
void InstallWorker::setSocketPath(const QString &path) { m_socketPath = path; }
void InstallWorker::cancel() { m_cancelled = true; }

// ---------------------------------------------------------------------------
// Core send/receive — uses a persistent socket passed in from run()
// ---------------------------------------------------------------------------

int InstallWorker::sendRequest(QLocalSocket &sock,
                               const QJsonObject &msg,
                               std::function<void(const QString &)> outputCallback)
{
    if (sock.state() != QLocalSocket::ConnectedState) {
        outputCallback("ERROR: helper socket not connected");
        return -1;
    }

    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    data.append('\n');
    sock.write(data);
    sock.flush();

    QByteArray readBuf;
    int result = -1;

    while (true) {
        if (!sock.waitForReadyRead(300)) {
            if (m_cancelled) {
                QJsonObject cancelMsg;
                cancelMsg["type"] = "cancel";
                QByteArray cd = QJsonDocument(cancelMsg).toJson(QJsonDocument::Compact);
                cd.append('\n');
                sock.write(cd);
                sock.flush();
                sock.waitForReadyRead(2000);
                return -1;
            }
            if (sock.state() != QLocalSocket::ConnectedState) {
                outputCallback("ERROR: helper disconnected unexpectedly");
                return -1;
            }
            continue;
        }

        readBuf.append(sock.readAll());

        int newline;
        while ((newline = readBuf.indexOf('\n')) != -1) {
            const QByteArray raw = readBuf.left(newline).trimmed();
            readBuf.remove(0, newline + 1);
            if (raw.isEmpty()) continue;

            QJsonParseError parseErr;
            const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseErr);
            if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
                outputCallback("ERROR: malformed response from helper");
                continue;
            }

            const QJsonObject resp = doc.object();
            const QString     type = resp["type"].toString();

            if (type == "output") {
                outputCallback(resp["line"].toString());
            } else if (type == "finished") {
                result = resp["exitCode"].toInt(-1);
                return result;
            } else if (type == "rejected") {
                outputCallback(QString("ERROR: helper rejected operation: %1")
                               .arg(resp["error"].toString()));
                return -1;
            } else if (type == "cancel_ack") {
                return -1;
            } else if (type == "error") {
                outputCallback(QString("ERROR: helper: %1")
                               .arg(resp["error"].toString()));
                return -1;
            }
            // "started" is informational — keep reading.
        }
    }
}

bool InstallWorker::runCheck(QLocalSocket &sock, const QStringList &cmd)
{
    if (cmd.isEmpty()) return false;
    QJsonObject msg;
    msg["type"]      = "execute";
    msg["requestId"] = QString::number(++m_requestCounter);
    msg["program"]   = cmd.first();
    QJsonArray args;
    for (int i = 1; i < cmd.size(); ++i) args.append(cmd[i]);
    msg["args"] = args;
    return sendRequest(sock, msg, [](const QString &) {}) == 0;
}

// ---------------------------------------------------------------------------
// Main run loop
// ---------------------------------------------------------------------------

void InstallWorker::run()
{
    // Open a single persistent connection for the entire session.
    QLocalSocket sock;
    sock.connectToServer(m_socketPath);
    if (!sock.waitForConnected(5000)) {
        emit logLine(QString("ERROR: could not connect to helper socket: %1")
                     .arg(sock.errorString()));
        emit allDone(1);
        return;
    }

    int errorCount = 0;

    for (const InstallStep &step : m_steps) {
        if (m_cancelled) break;

        emit stepStarted(step.id, step.description);
        emit logLine(QString("\n==> %1").arg(step.description));

        if (!step.alreadyInstalledCheck.isEmpty()) {
            if (runCheck(sock, step.alreadyInstalledCheck)) {
                emit logLine(QString("INFO: Already installed, skipping: %1")
                             .arg(step.description));
                emit stepSkipped(step.id, step.description);
                continue;
            }
        }

        if (step.command.isEmpty()) {
            emit stepFinished(step.id, true, 0);
            continue;
        }

        QJsonObject msg;
        msg["type"]      = "execute";
        msg["requestId"] = QString::number(++m_requestCounter);
        msg["program"]   = step.command.first();
        QJsonArray args;
        for (int i = 1; i < step.command.size(); ++i) args.append(step.command[i]);
        msg["args"] = args;

        int code = sendRequest(sock, msg, [&](const QString &line) {
            emit logLine(line);
        });

        const bool ok = (code == 0) || step.allowedExitCodes.contains(code);

        if (!ok) {
            emit logLine(QString("FAILED (exit %1): %2").arg(code).arg(step.description));
            if (!step.optional) errorCount++;
        } else {
            if (code != 0)
                emit logLine(QString("WARNING (exit %1, treated as success): %2")
                             .arg(code).arg(step.description));
            else
                emit logLine(QString("OK: %1").arg(step.description));
        }
        emit stepFinished(step.id, ok, code);
    }

    // Shut down the helper cleanly.
    QJsonObject shutdownMsg;
    shutdownMsg["type"] = "shutdown";
    QByteArray sd = QJsonDocument(shutdownMsg).toJson(QJsonDocument::Compact);
    sd.append('\n');
    sock.write(sd);
    sock.flush();
    sock.waitForReadyRead(2000);
    sock.disconnectFromServer();

    emit allDone(errorCount);
}
