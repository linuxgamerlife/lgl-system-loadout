#include "helper/server.h"

#include "core/policy.h"
#include "core/protocol.h"

#include <poll.h>
#include <pwd.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace loadout::helper {
namespace {

constexpr const char *kSocketName = "/lgl-helper.sock";
constexpr int kStartupTimeoutMs = 60000;
constexpr int kAuthTimeoutMs = 5000;
constexpr std::size_t kMaxLine = 1024 * 1024;

std::string socketPathFor(const std::string &directory)
{
    return directory + kSocketName;
}

bool parseUid(const char *text, uid_t &uid)
{
    if (!text || !*text)
        return false;
    for (const char *character = text; *character; ++character)
        if (*character < '0' || *character > '9')
            return false;
    try {
        const auto value = std::stoul(text);
        if (value == 0 || value > std::numeric_limits<uid_t>::max())
            return false;
        uid = static_cast<uid_t>(value);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

std::string makeNonce()
{
    unsigned char bytes[16];
    std::size_t filled = 0;
    while (filled < sizeof(bytes)) {
        const ssize_t size = getrandom(bytes + filled, sizeof(bytes) - filled, 0);
        if (size < 0) {
            if (errno == EINTR)
                continue;
            return {};
        }
        if (size == 0)
            return {};
        filled += static_cast<std::size_t>(size);
    }

    static const char *hex = "0123456789abcdef";
    std::string nonce;
    nonce.reserve(sizeof(bytes) * 2);
    for (const unsigned char byte : bytes) {
        nonce.push_back(hex[byte >> 4]);
        nonce.push_back(hex[byte & 0x0f]);
    }
    return nonce;
}

} // namespace

Server::~Server()
{
    cleanup();
}

bool Server::start(std::string &socketPath)
{
    // The helper is only ever launched by pkexec, which sets PKEXEC_UID to the
    // user that authenticated. The socket is owned by that user so other local
    // accounts cannot even connect, let alone race for the session.
    if (!parseUid(std::getenv("PKEXEC_UID"), m_expectedUid)) {
        std::fprintf(stderr,
                     "lgl-system-loadout-helper: PKEXEC_UID is missing or invalid\n");
        return false;
    }
    const passwd *entry = getpwuid(m_expectedUid);
    if (!entry || !entry->pw_name || !*entry->pw_name) {
        std::fprintf(stderr, "lgl-system-loadout-helper: could not resolve the caller\n");
        return false;
    }
    m_expectedUser = entry->pw_name;

    char directory[] = "/run/lgl-XXXXXX";
    if (!mkdtemp(directory)) {
        std::fprintf(stderr, "lgl-system-loadout-helper: mkdtemp failed: %s\n",
                     std::strerror(errno));
        return false;
    }
    m_directory = directory;

    // The GUI user needs traverse permission, but must not be able to list.
    if (chmod(m_directory.c_str(), 0711) != 0) {
        std::fprintf(stderr, "lgl-system-loadout-helper: chmod failed: %s\n",
                     std::strerror(errno));
        cleanup();
        return false;
    }

    m_socketPath = socketPathFor(m_directory);
    m_listenFd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (m_listenFd < 0) {
        std::fprintf(stderr, "lgl-system-loadout-helper: socket failed: %s\n",
                     std::strerror(errno));
        cleanup();
        return false;
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, m_socketPath.c_str(), sizeof(address.sun_path) - 1);

    if (bind(m_listenFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        std::fprintf(stderr, "lgl-system-loadout-helper: bind failed: %s\n", std::strerror(errno));
        cleanup();
        return false;
    }

    // The caller may connect, nobody else may. The pkexec caller still has to
    // prove it holds the nonce printed on the helper's stdout.
    if (chown(m_socketPath.c_str(), m_expectedUid, static_cast<gid_t>(-1)) != 0 ||
        chmod(m_socketPath.c_str(), 0600) != 0) {
        std::fprintf(stderr, "lgl-system-loadout-helper: could not protect the socket: %s\n",
                     std::strerror(errno));
        cleanup();
        return false;
    }

    if (listen(m_listenFd, 8) != 0) {
        std::fprintf(stderr, "lgl-system-loadout-helper: listen failed: %s\n",
                     std::strerror(errno));
        cleanup();
        return false;
    }

    m_nonce = makeNonce();
    if (m_nonce.empty()) {
        std::fprintf(stderr, "lgl-system-loadout-helper: could not generate a session nonce\n");
        cleanup();
        return false;
    }

    socketPath = m_socketPath;
    return true;
}

bool Server::acceptClient()
{
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(kStartupTimeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        pollfd entry{m_listenFd, POLLIN, 0};
        const int ready = poll(&entry, 1, 500);
        if (ready == 0)
            continue;
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }

        const int client = accept4(m_listenFd, nullptr, nullptr, SOCK_CLOEXEC);
        if (client < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }

        ucred credentials{};
        socklen_t length = sizeof(credentials);
        if (getsockopt(client, SOL_SOCKET, SO_PEERCRED, &credentials, &length) != 0 ||
            credentials.uid != m_expectedUid) {
            std::fprintf(stderr,
                         "lgl-system-loadout-helper: rejecting a client that is not the pkexec "
                         "caller\n");
            close(client);
            continue;
        }

        m_clientFd = client;
        m_buffer.clear();
        std::string line;
        if (!readMessage(line, kAuthTimeoutMs)) {
            close(m_clientFd);
            m_clientFd = -1;
            m_buffer.clear();
            continue;
        }

        const auto request = protocol::parseRequest(line);
        if (!request.valid || request.type != "auth" || request.nonce != m_nonce) {
            std::fprintf(stderr,
                         "lgl-system-loadout-helper: rejecting a client without the session "
                         "nonce\n");
            close(m_clientFd);
            m_clientFd = -1;
            m_buffer.clear();
            continue;
        }

        sendLine(protocol::simpleRequest("auth_ok"));
        return true;
    }

    std::fprintf(stderr, "lgl-system-loadout-helper: timed out waiting for the GUI\n");
    return false;
}

bool Server::readMessage(std::string &line, const int timeoutMs)
{
    const auto deadline = timeoutMs > 0
                              ? std::chrono::steady_clock::now() +
                                    std::chrono::milliseconds(timeoutMs)
                              : std::chrono::steady_clock::time_point::max();

    while (true) {
        const auto newline = m_buffer.find('\n');
        if (newline != std::string::npos) {
            std::string candidate = m_buffer.substr(0, newline);
            m_buffer.erase(0, newline + 1);
            while (!candidate.empty() && (candidate.back() == '\r' || candidate.back() == ' '))
                candidate.pop_back();
            if (candidate.empty())
                continue;
            line = std::move(candidate);
            return true;
        }

        const auto now = std::chrono::steady_clock::now();
        if (timeoutMs > 0 && now >= deadline)
            return false;

        int waitMs = 500;
        if (timeoutMs > 0) {
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            waitMs = static_cast<int>(std::max<long long>(1, remaining));
            waitMs = std::min(waitMs, 500);
        }

        pollfd entry{m_clientFd, POLLIN, 0};
        const int ready = poll(&entry, 1, waitMs);
        if (ready == 0)
            continue;
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready < 0)
            return false;

        char chunk[4096];
        const ssize_t size = recv(m_clientFd, chunk, sizeof(chunk), 0);
        if (size <= 0)
            return false;
        m_buffer.append(chunk, static_cast<std::size_t>(size));
        if (m_buffer.size() > kMaxLine)
            return false;
    }
}

void Server::pumpClient()
{
    while (m_clientFd >= 0) {
        char chunk[4096];
        const ssize_t size = recv(m_clientFd, chunk, sizeof(chunk), MSG_DONTWAIT);
        if (size <= 0)
            break;
        m_buffer.append(chunk, static_cast<std::size_t>(size));
        if (m_buffer.size() > kMaxLine) {
            m_buffer.clear();
            break;
        }
    }

    auto lines = protocol::takeLines(m_buffer);
    if (lines.empty())
        return;

    for (const auto &line : lines) {
        const auto request = protocol::parseRequest(line);
        if (request.type == "cancel") {
            m_cancelRequested = true;
            sendLine(protocol::simpleRequest("cancel_ack"));
        }
    }
}

void Server::handle(const std::string &raw)
{
    const auto request = protocol::parseRequest(raw);

    if (!request.valid) {
        sendLine("{\"type\":\"error\",\"error\":\"invalid request\"}");
        return;
    }

    if (request.type == "shutdown") {
        sendLine(protocol::simpleRequest("shutdown_ack"));
        m_done = true;
        return;
    }

    if (request.type == "cancel") {
        m_cancelRequested = true;
        sendLine(protocol::simpleRequest("cancel_ack"));
        return;
    }

    if (request.type != "execute") {
        sendLine("{\"type\":\"error\",\"error\":\"unknown message type\"}");
        return;
    }

    std::string error;
    if (!policy::validate(request.program, request.args, error, m_expectedUser)) {
        sendLine("{\"type\":\"rejected\",\"requestId\":\"" + protocol::escape(request.requestId) +
                 "\",\"error\":\"" + protocol::escape(error) + "\"}");
        return;
    }

    sendLine("{\"type\":\"started\",\"requestId\":\"" + protocol::escape(request.requestId) +
             "\"}");

    std::vector<std::string> argv{request.program};
    argv.insert(argv.end(), request.args.begin(), request.args.end());

    m_cancelRequested = false;
    const std::string requestId = protocol::escape(request.requestId);
    const int exitCode = m_executor.run(
        argv,
        [this, requestId](const std::string &line) {
            sendLine("{\"type\":\"output\",\"requestId\":\"" + requestId + "\",\"line\":\"" +
                     protocol::escape(line) + "\"}");
        },
        [this] { return m_cancelRequested; }, [this] { pumpClient(); });

    sendLine("{\"type\":\"finished\",\"requestId\":\"" + requestId + "\",\"exitCode\":" +
             std::to_string(exitCode) + "}");
}

void Server::sendLine(const std::string &json)
{
    if (m_clientFd < 0)
        return;
    const std::string data = json + "\n";
    std::size_t written = 0;
    while (written < data.size()) {
        const ssize_t size =
            send(m_clientFd, data.data() + written, data.size() - written, MSG_NOSIGNAL);
        if (size <= 0)
            return;
        written += static_cast<std::size_t>(size);
    }
}

void Server::serve()
{
    if (!acceptClient()) {
        cleanup();
        return;
    }

    while (!m_done) {
        std::string line;
        if (!readMessage(line))
            break;
        handle(line);
    }

    cleanup();
}

void Server::cleanup()
{
    if (m_clientFd >= 0) {
        close(m_clientFd);
        m_clientFd = -1;
    }
    if (m_listenFd >= 0) {
        close(m_listenFd);
        m_listenFd = -1;
    }
    if (!m_socketPath.empty()) {
        unlink(m_socketPath.c_str());
        m_socketPath.clear();
    }
    if (!m_directory.empty()) {
        std::error_code error;
        std::filesystem::remove_all(m_directory, error);
        m_directory.clear();
    }
}

} // namespace loadout::helper
