#include "helperserver.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QTextStream>
#include <sys/stat.h>
#include <cstdlib>
#include <cerrno>
#include <cstring>

// ---------------------------------------------------------------------------
// Allow-list
//
// Only operations explicitly listed here are permitted. The helper refuses
// any message whose "program" field does not appear in this list.
//
// Arg validation rule types:
//   "package"       — RPM/Flatpak name: ^[a-zA-Z0-9_\-\.+]+$
//   "copr-repo"     — user/repo: ^[a-zA-Z0-9_\-]+/[a-zA-Z0-9_\-\.]+$
//   "url-https"     — https:// URL, no shell metacharacters
//   "filepath-run"  — /run/lgl-XXXXXX/filename only (one level deep)
//   "username"      — POSIX lowercase username
//   "fixed:<value>" — argument must be exactly <value>
// ---------------------------------------------------------------------------

namespace {

struct ArgRule {
    QString type;
    QString fixed;
};

struct AllowedOp {
    QString        program;
    QList<ArgRule> argRules;
    bool           variadic = false;
};

ArgRule fixed(const QString &val) { return {"fixed", val}; }
ArgRule rule(const QString &type) { return {type, {}}; }

const QList<AllowedOp> &allowList()
{
    static const QList<AllowedOp> list = {

        // --- dnf install / upgrade ----------------------------------------
        { "/usr/bin/dnf", { fixed("-y"), fixed("install"), rule("package") }, true },
        // RPM Fusion and similar: dnf -y install https://...rpm
        { "/usr/bin/dnf", { fixed("-y"), fixed("install"), rule("url-https") } },
        { "/usr/bin/dnf", { fixed("-y"), fixed("upgrade"), fixed("--refresh") } },
        { "/usr/bin/dnf", { fixed("-y"), fixed("swap"), rule("package"), rule("package") } },
        { "/usr/bin/dnf", { fixed("copr"), fixed("enable"), rule("copr-repo") } },
        { "/usr/bin/dnf", { fixed("copr"), fixed("enable"), fixed("-y"), rule("copr-repo") } },
        { "/usr/bin/dnf", { fixed("config-manager"), fixed("addrepo"), fixed("--from-repofile"), rule("filepath-run") } },
        { "/usr/bin/dnf", { fixed("clean"), fixed("all") } },
        // dnf install with extra flags (--skip-unavailable, --allowerasing)
        { "/usr/bin/dnf", { fixed("-y"), fixed("install"), fixed("--skip-unavailable"), rule("package") } },
        { "/usr/bin/dnf", { fixed("-y"), fixed("install"), fixed("--skip-unavailable"),
                            fixed("--allowerasing"), rule("package") } },
        { "/usr/bin/dnf", { fixed("-y"), fixed("install"), fixed("--allowerasing"), rule("package") } },
        // dnf swap with --allowerasing
        { "/usr/bin/dnf", { fixed("swap"), fixed("-y"),
                            rule("package"), rule("package"), fixed("--allowerasing") } },

        // --- flatpak -------------------------------------------------------
        { "/usr/bin/flatpak", { fixed("install"), fixed("-y"), fixed("--system"),
                                fixed("flathub"), rule("package") } },
        { "/usr/bin/flatpak", { fixed("remote-add"), fixed("--if-not-exists"),
                                fixed("--system"), fixed("flathub"),
                                fixed("https://flathub.org/repo/flathub.flatpakrepo") } },

        // --- rpm (used for bootstrap already-installed check) ---------------
        { "/usr/bin/rpm", { fixed("-q"), fixed("--quiet"), rule("package") }, true /* variadic packages */ },

        // --- curl (download to /run/lgl-* session dir only) ----------------
        { "/usr/bin/curl", { fixed("-fsSL"), fixed("-o"),
                             rule("filepath-run"), rule("url-https") } },

        // --- systemctl -----------------------------------------------------
        { "/usr/bin/systemctl", { fixed("enable"), fixed("--now"), fixed("libvirtd") } },
        { "/usr/bin/systemctl", { fixed("disable"), fixed("--now"),
                                  fixed("NetworkManager-wait-online") } },
        { "/usr/bin/systemctl", { fixed("reboot") } },

        // --- usermod -------------------------------------------------------
        { "/usr/sbin/usermod", { fixed("-aG"), fixed("libvirt"), rule("username") } },

        // --- kpackagetool6 -------------------------------------------------
        { "/usr/bin/kpackagetool6", { fixed("--type"), fixed("kwin/script"),
                                      fixed("--install"), rule("filepath-run") } },
        { "/usr/bin/kpackagetool6", { fixed("--type"), fixed("Plasma/Applet"),
                                      fixed("--install"), rule("filepath-run") } },

        // --- setsebool (CachyOS SELinux) -----------------------------------
        { "/usr/sbin/setsebool", { fixed("-P"),
                                   fixed("domain_kernel_load_modules"), fixed("on") } },

        // --- sudo -u (user-scoped tools: pipx, etc.) -----------------------
        // Only permitted for the validated target user detected at startup.
        // The username arg is validated as "username" type.
        { "/usr/bin/sudo", { fixed("-u"), rule("username"), rule("any"),
                             rule("any"), rule("any") } },
        { "/usr/bin/sudo", { fixed("-u"), rule("username"), rule("any"),
                             rule("any"), rule("any"), rule("any") } },
        { "/usr/bin/sudo", { fixed("-u"), rule("username"), rule("any"),
                             rule("any"), rule("any"), rule("any"), rule("any") } },
        { "/usr/bin/sudo", { fixed("-u"), rule("username"), rule("any"),
                             rule("any"), rule("any"), rule("any"), rule("any"), rule("any") } },
        { "/usr/bin/sudo", { fixed("-u"), rule("username"), rule("any"),
                             rule("any"), rule("any"), rule("any"), rule("any"), rule("any") } },

    };
    return list;
}

// ---------------------------------------------------------------------------
// Argument validation
// ---------------------------------------------------------------------------

bool validateArg(const ArgRule &r, const QString &val)
{
    if (r.type == "fixed") {
        return val == r.fixed;
    }
    if (r.type == "package") {
        static const QRegularExpression re(R"(^[a-zA-Z0-9_\-\.+]+$)");
        return re.match(val).hasMatch() && val.size() <= 255;
    }
    if (r.type == "copr-repo") {
        static const QRegularExpression re(R"(^[a-zA-Z0-9_\-]+/[a-zA-Z0-9_\-\.]+$)");
        return re.match(val).hasMatch() && val.size() <= 128;
    }
    if (r.type == "url-https") {
        static const QRegularExpression re(R"(^https://[a-zA-Z0-9_\-\./\?=&%+:#@]+$)");
        return val.startsWith("https://") && re.match(val).hasMatch() && val.size() <= 512;
    }
    if (r.type == "filepath-run") {
        // Must be exactly /run/lgl-<token>/<filename> — one level deep, no traversal.
        static const QRegularExpression re(R"(^/run/lgl-[a-zA-Z0-9]+/[a-zA-Z0-9_\-\.]+$)");
        return re.match(val).hasMatch() && val.size() <= 256;
    }
    if (r.type == "username") {
        static const QRegularExpression re(R"(^[a-z_][a-z0-9_\-]{0,31}$)");
        return re.match(val).hasMatch();
    }
    if (r.type == "any") {
        // Used for sudo -u <user> <tool> <subcommand> [args].
        // Not used for security-sensitive positional args.
        return val.size() <= 512 && !val.contains('\n') && !val.contains('\0');
    }
    return false;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// HelperServer
// ---------------------------------------------------------------------------

HelperServer::HelperServer(QObject *parent) : QObject(parent) {}

HelperServer::~HelperServer()
{
    cleanup();
}

bool HelperServer::start()
{
    char tmpl[] = "/run/lgl-XXXXXX";
    if (!mkdtemp(tmpl)) {
        QTextStream err(stderr);
        err << "HelperServer: mkdtemp failed: " << strerror(errno) << "\n";
        return false;
    }
    m_socketDir  = QString::fromLocal8Bit(tmpl);
    m_socketPath = m_socketDir + "/lgl-helper.sock";

    // Directory needs execute (traverse) permission for the GUI user.
    // Socket itself is restricted to the connecting user via WorldAccessOption
    // combined with the socket file permissions set by QLocalServer.
    if (chmod(tmpl, 0711) != 0) {
        QTextStream err(stderr);
        err << "HelperServer: chmod failed: " << strerror(errno) << "\n";
        cleanup();
        return false;
    }

    m_server.setSocketOptions(QLocalServer::WorldAccessOption);

    if (!m_server.listen(m_socketPath)) {
        QTextStream err(stderr);
        err << "HelperServer: listen failed: " << m_server.errorString() << "\n";
        cleanup();
        return false;
    }

    connect(&m_server, &QLocalServer::newConnection,
            this,      &HelperServer::onNewConnection);
    return true;
}

void HelperServer::onNewConnection()
{
    QLocalSocket *incoming = m_server.nextPendingConnection();
    if (!incoming) return;

    if (m_client) {
        incoming->disconnectFromServer();
        incoming->deleteLater();
        return;
    }

    m_client = incoming;
    m_server.close();

    connect(m_client, &QLocalSocket::readyRead,
            this,     &HelperServer::onClientDataReady);
    connect(m_client, &QLocalSocket::disconnected,
            this,     &HelperServer::onClientDisconnected);
}

void HelperServer::onClientDataReady()
{
    m_readBuf.append(m_client->readAll());

    int newline;
    while ((newline = m_readBuf.indexOf('\n')) != -1) {
        QByteArray msg = m_readBuf.left(newline).trimmed();
        m_readBuf.remove(0, newline + 1);
        if (!msg.isEmpty())
            processMessage(msg);
    }
}

void HelperServer::onClientDisconnected()
{
    emit sessionEnded();
}

void HelperServer::processMessage(const QByteArray &raw)
{
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseErr);

    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        QJsonObject resp;
        resp["type"]  = "error";
        resp["error"] = "invalid JSON";
        sendToClient(resp);
        return;
    }

    const QJsonObject msg  = doc.object();
    const QString     type = msg["type"].toString();

    if (type == "shutdown") {
        QJsonObject resp;
        resp["type"] = "shutdown_ack";
        sendToClient(resp);
        if (m_client) m_client->flush();
        emit sessionEnded();
        return;
    }

    if (type == "execute") {
        QString     program;
        QStringList args;
        QString     error;

        if (!validateOperation(msg, program, args, error)) {
            QJsonObject resp;
            resp["type"]      = "rejected";
            resp["requestId"] = msg["requestId"];
            resp["error"]     = error;
            sendToClient(resp);
            return;
        }

        {
            QJsonObject resp;
            resp["type"]      = "started";
            resp["requestId"] = msg["requestId"];
            sendToClient(resp);
        }

        // Reset cancel flag for this execution.
        m_cancelFlag.store(false);

        const QString requestId = msg["requestId"].toString();
        const int exitCode = m_executor.execute(program, args,
            [&](const QString &line) {
                QJsonObject out;
                out["type"]      = "output";
                out["requestId"] = requestId;
                out["line"]      = line;
                sendToClient(out);
            },
            &m_cancelFlag);

        QJsonObject resp;
        resp["type"]      = "finished";
        resp["requestId"] = requestId;
        resp["exitCode"]  = exitCode;
        sendToClient(resp);
        return;
    }

    if (type == "cancel") {
        // GUI is requesting cancellation of the current command.
        m_cancelFlag.store(true);
        QJsonObject resp;
        resp["type"] = "cancel_ack";
        sendToClient(resp);
        return;
    }

    QJsonObject resp;
    resp["type"]  = "error";
    resp["error"] = QString("unknown message type: %1").arg(type);
    sendToClient(resp);
}


void HelperServer::sendToClient(const QJsonObject &obj)
{
    if (!m_client) return;
    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    data.append('\n');
    m_client->write(data);
    m_client->flush();
}

bool HelperServer::validateOperation(const QJsonObject &msg,
                                     QString &program,
                                     QStringList &args,
                                     QString &errorOut)
{
    if (!msg["program"].isString() || !msg["args"].isArray()) {
        errorOut = "message missing program or args";
        return false;
    }

    const QString    reqProgram = msg["program"].toString();
    const QJsonArray reqArgs    = msg["args"].toArray();

    QStringList argList;
    argList.reserve(reqArgs.size());
    for (const QJsonValue &v : reqArgs) {
        if (!v.isString()) {
            errorOut = "args must be strings";
            return false;
        }
        argList.append(v.toString());
    }

    for (const AllowedOp &op : allowList()) {
        if (op.program != reqProgram) continue;

        const int ruleCount = op.argRules.size();
        const int argCount  = argList.size();

        if (!op.variadic) {
            if (argCount != ruleCount) continue;
        } else {
            if (argCount < ruleCount) continue;
        }

        bool match = true;
        for (int i = 0; i < argCount; ++i) {
            const ArgRule &r = (i < ruleCount) ? op.argRules[i]
                                                : op.argRules.last();
            if (!validateArg(r, argList[i])) {
                match = false;
                break;
            }
        }
        if (!match) continue;

        program = reqProgram;
        args    = argList;
        return true;
    }

    errorOut = QString("operation not permitted: %1").arg(reqProgram);
    return false;
}

void HelperServer::cleanup()
{
    if (!m_socketDir.isEmpty()) {
        QDir dir(m_socketDir);
        dir.removeRecursively();
        m_socketDir.clear();
        m_socketPath.clear();
    }
}
