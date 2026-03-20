// Test suite for InstallWorker — adapted for 1.1.0 socket-based architecture.
//
// In 1.1.0 the worker communicates with the privileged helper over a Unix
// socket rather than spawning processes directly. Full integration tests
// require the helper binary to be running. The tests below cover the
// worker's logic (step sequencing, error counting, cancellation, thread
// affinity) using a mock socket server that simulates helper responses.

#include <QtTest>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include "../src/installworker.h"

// ---------------------------------------------------------------------------
// MockHelperServer — simulates the privileged helper over a Unix socket.
// Responds to execute requests based on the program name:
//   "true"  → exit 0
//   "false" → exit 1
//   others  → exit 0
// ---------------------------------------------------------------------------
class MockHelperServer : public QObject
{
    Q_OBJECT
public:
    explicit MockHelperServer(QObject *parent = nullptr) : QObject(parent) {}

    bool start()
    {
        m_dir = new QTemporaryDir;
        if (!m_dir->isValid()) return false;
        m_socketPath = m_dir->path() + "/mock-helper.sock";
        connect(&m_server, &QLocalServer::newConnection,
                this, &MockHelperServer::onNewConnection);
        return m_server.listen(m_socketPath);
    }

    QString socketPath() const { return m_socketPath; }

private slots:
    void onNewConnection()
    {
        auto *client = m_server.nextPendingConnection();
        if (!client) return;
        connect(client, &QLocalSocket::readyRead, this, [this, client] {
            m_buf.append(client->readAll());
            int nl;
            while ((nl = m_buf.indexOf('\n')) != -1) {
                const QByteArray raw = m_buf.left(nl).trimmed();
                m_buf.remove(0, nl + 1);
                if (raw.isEmpty()) continue;
                handleMessage(client, QJsonDocument::fromJson(raw).object());
            }
        });
    }

    void handleMessage(QLocalSocket *client, const QJsonObject &msg)
    {
        const QString type = msg["type"].toString();
        const QString requestId = msg["requestId"].toString();

        if (type == "shutdown") {
            sendJson(client, {{"type", "shutdown_ack"}});
            return;
        }
        if (type == "execute") {
            sendJson(client, {{"type", "started"}, {"requestId", requestId}});
            const QString program = msg["program"].toString();
            int exitCode = 0;
            if (program == "false") exitCode = 1;
            sendJson(client, {{"type", "finished"},
                               {"requestId", requestId},
                               {"exitCode", exitCode}});
        }
    }

    void sendJson(QLocalSocket *client, const QJsonObject &obj)
    {
        QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        data.append('\n');
        client->write(data);
        client->flush();
    }

private:
    QLocalServer  m_server;
    QTemporaryDir *m_dir = nullptr;
    QString        m_socketPath;
    QByteArray     m_buf;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static InstallStep makeStep(const QString &id, const QString &program)
{
    return InstallStep{id, QString("Step %1").arg(id), {program}};
}

struct RunResult {
    int errorCount = -1;
    QStringList startedIds;
    QStringList skippedIds;
    QMap<QString, QPair<bool,int>> stepResults;
    QStringList logLines;
};

static RunResult runWorker(const QList<InstallStep> &steps,
                           const QString &socketPath,
                           std::function<void(InstallWorker*)> beforeStart = {})
{
    RunResult result;

    auto *thread = new QThread;
    auto *worker = new InstallWorker;
    worker->setSteps(steps);
    worker->setSocketPath(socketPath);
    worker->moveToThread(thread);

    QObject::connect(thread, &QThread::started,  worker, &InstallWorker::run);
    QObject::connect(worker, &InstallWorker::allDone, thread, &QThread::quit);
    QObject::connect(worker, &InstallWorker::allDone, worker, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    QObject::connect(worker, &InstallWorker::stepStarted,  qApp,
        [&](const QString &id, const QString &) { result.startedIds << id; });
    QObject::connect(worker, &InstallWorker::stepSkipped,  qApp,
        [&](const QString &id, const QString &) { result.skippedIds << id; });
    QObject::connect(worker, &InstallWorker::stepFinished, qApp,
        [&](const QString &id, bool ok, int code) { result.stepResults[id] = {ok, code}; });
    QObject::connect(worker, &InstallWorker::logLine, qApp,
        [&](const QString &line) { result.logLines << line; });
    QObject::connect(worker, &InstallWorker::allDone, qApp,
        [&](int n) { result.errorCount = n; });

    if (beforeStart) beforeStart(worker);
    thread->start();

    QElapsedTimer t; t.start();
    while (result.errorCount == -1 && t.elapsed() < 10000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    return result;
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestInstallWorker : public QObject
{
    Q_OBJECT

private:
    MockHelperServer *m_mock = nullptr;
    QString           m_socketPath;

private slots:

    void initTestCase()
    {
        m_mock = new MockHelperServer(this);
        QVERIFY2(m_mock->start(), "Mock helper server failed to start");
        m_socketPath = m_mock->socketPath();
    }

    void test_singleSuccessStep()
    {
        const auto r = runWorker({makeStep("s1", "true")}, m_socketPath);
        QCOMPARE(r.errorCount, 0);
        QVERIFY(r.stepResults.value("s1").first);
    }

    void test_singleFailStep()
    {
        const auto r = runWorker({makeStep("f1", "false")}, m_socketPath);
        QCOMPARE(r.errorCount, 1);
        QVERIFY(!r.stepResults.value("f1").first);
    }

    void test_multipleStepsErrorCount()
    {
        const QList<InstallStep> steps = {
            makeStep("t1", "true"),
            makeStep("f1", "false"),
            makeStep("t2", "true"),
            makeStep("f2", "false"),
        };
        const auto r = runWorker(steps, m_socketPath);
        QCOMPARE(r.errorCount, 2);
    }

    void test_stepSequenceOrder()
    {
        const QList<InstallStep> steps = {
            makeStep("a", "true"),
            makeStep("b", "true"),
            makeStep("c", "true"),
        };
        const auto r = runWorker(steps, m_socketPath);
        QCOMPARE(r.startedIds, QStringList({"a", "b", "c"}));
    }

    void test_optionalFailDoesNotIncrementErrorCount()
    {
        InstallStep s = makeStep("opt", "false");
        s.optional = true;
        const auto r = runWorker({s}, m_socketPath);
        QCOMPARE(r.errorCount, 0);
    }

    void test_noopStepSucceeds()
    {
        InstallStep s{"nop", "No-op", {}};
        const auto r = runWorker({s}, m_socketPath);
        QCOMPARE(r.errorCount, 0);
        QVERIFY(r.stepResults.value("nop").first);
    }

    void test_allowedExitCodes()
    {
        InstallStep s = makeStep("allowed", "false");
        s.allowedExitCodes = {1};
        const auto r = runWorker({s}, m_socketPath);
        QCOMPARE(r.errorCount, 0);
        QVERIFY(r.stepResults.value("allowed").first);
    }

    void test_cancelStopsProcessing()
    {
        QList<InstallStep> steps;
        for (int i = 0; i < 10; ++i)
            steps << makeStep(QString("step_%1").arg(i), "true");

        const auto r = runWorker(steps, m_socketPath, [](InstallWorker *w) {
            w->cancel();
        });

        QVERIFY(r.startedIds.size() < 10);
        QCOMPARE(r.errorCount, 0);
    }

    void test_allDoneArrivesOnMainThread()
    {
        Qt::HANDLE mainThread = QThread::currentThreadId();
        Qt::HANDLE signalThread = nullptr;
        bool done = false;

        auto *thread = new QThread;
        auto *worker = new InstallWorker;
        worker->setSteps({makeStep("t", "true")});
        worker->setSocketPath(m_socketPath);
        worker->moveToThread(thread);

        QObject::connect(thread, &QThread::started,  worker, &InstallWorker::run);
        QObject::connect(worker, &InstallWorker::allDone, thread, &QThread::quit);
        QObject::connect(worker, &InstallWorker::allDone, worker, &QObject::deleteLater);
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        QObject::connect(worker, &InstallWorker::allDone, qApp,
            [&](int) { signalThread = QThread::currentThreadId(); done = true; },
            Qt::QueuedConnection);

        thread->start();

        QElapsedTimer t; t.start();
        while (!done && t.elapsed() < 5000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        QVERIFY(done);
        QCOMPARE(signalThread, mainThread);
    }
};

QTEST_MAIN(TestInstallWorker)
#include "tst_installworker.moc"
