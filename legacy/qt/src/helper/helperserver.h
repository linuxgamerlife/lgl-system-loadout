#pragma once
// HelperServer — Unix socket server for the privileged helper.
//
// Responsibilities:
//   - Create a randomised socket path under /run/lgl-XXXXXX/
//   - Accept exactly one client connection (the GUI process)
//   - Receive newline-delimited JSON messages from the client
//   - Validate each message against the operation allow-list
//   - Dispatch validated operations to CommandExecutor
//   - Handle verify_hash requests internally (no subprocess)
//   - Stream output lines back to the client as JSON
//   - Clean up the socket directory on destruction
//
// Message types handled:
//   execute      — run a validated command, stream output
//   verify_hash  — verify SHA-256 of a file at a /run/lgl-* path
//   shutdown     — clean exit
//
// Thread safety:
//   All work happens on the Qt event loop thread. No additional threading.

#include "commandexecutor.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QString>
#include <atomic>

class HelperServer : public QObject
{
    Q_OBJECT

public:
    explicit HelperServer(QObject *parent = nullptr);
    ~HelperServer() override;

    // Start the server. Returns false if socket creation fails.
    bool start();

    QString socketPath() const { return m_socketPath; }

    // Called by the worker's cancellation path. Thread-safe (atomic).
    // Causes the currently executing command to be terminated gracefully.
    void requestCancel() { m_cancelFlag.store(true); }

signals:
    void sessionEnded();

private slots:
    void onNewConnection();
    void onClientDataReady();
    void onClientDisconnected();

private:
    void processMessage(const QByteArray &raw);
    void sendToClient(const QJsonObject &obj);

    bool validateOperation(const QJsonObject &msg,
                           QString &program,
                           QStringList &args,
                           QString &errorOut);

    void cleanup();

    QLocalServer    m_server;
    QLocalSocket   *m_client     = nullptr;
    CommandExecutor m_executor;

    QString         m_socketDir;
    QString         m_socketPath;

    QByteArray      m_readBuf;

    // Cancellation flag — written by requestCancel() (potentially from a
    // signal delivered on the main thread) and read by CommandExecutor on the
    // event loop thread. std::atomic<bool> prevents a data race.
    std::atomic<bool> m_cancelFlag{false};
};
