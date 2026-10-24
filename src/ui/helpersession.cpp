#include "ui/helpersession.h"

#include "core/process.h"
#include "core/protocol.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <map>

namespace loadout {
namespace {

bool sendAll(const int fd, const std::string &data)
{
    std::size_t written = 0;
    while (written < data.size()) {
        const ssize_t size =
            send(fd, data.data() + written, data.size() - written, MSG_NOSIGNAL);
        if (size <= 0)
            return false;
        written += static_cast<std::size_t>(size);
    }
    return true;
}

std::string lowered(std::string value)
{
    for (char &character : value)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return value;
}

enum class ReadStatus { Line, Timeout, Closed };

ReadStatus readLine(const int fd, std::string &buffer, std::string &line,
                    const std::atomic<bool> &cancelRequested, bool &cancelSent)
{
    while (true) {
        const auto newline = buffer.find('\n');
        if (newline != std::string::npos) {
            line = buffer.substr(0, newline);
            buffer.erase(0, newline + 1);
            while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
                line.pop_back();
            if (line.empty())
                continue;
            return ReadStatus::Line;
        }

        if (cancelRequested && !cancelSent) {
            if (!sendAll(fd, protocol::simpleRequest("cancel") + "\n"))
                return ReadStatus::Closed;
            cancelSent = true;
        }

        pollfd entry{fd, POLLIN, 0};
        const int ready = poll(&entry, 1, 300);
        if (ready == 0)
            return ReadStatus::Timeout;
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready < 0)
            return ReadStatus::Closed;

        char chunk[4096];
        const ssize_t size = recv(fd, chunk, sizeof(chunk), 0);
        if (size <= 0)
            return ReadStatus::Closed;
        buffer.append(chunk, static_cast<std::size_t>(size));
        if (buffer.size() > 2 * 1024 * 1024)
            return ReadStatus::Closed;
    }
}

int connectSocket(const std::string &path)
{
    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::snprintf(address.sun_path, sizeof(address.sun_path), "%s", path.c_str());

    if (connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}
} // namespace

HelperSession::HelperSession()
{
    m_dispatcher.connect([this] { drain(); });
}

HelperSession::~HelperSession()
{
    m_cancelRequested = true;
    if (m_thread.joinable())
        m_thread.join();
    stopHelper();
}

void HelperSession::start(PlanBuilder builder)
{
    if (m_running.exchange(true))
        return;
    if (m_thread.joinable())
        m_thread.join();
    m_cancelRequested = false;
    m_thread = std::thread(&HelperSession::run, this, std::move(builder));
}

void HelperSession::cancel()
{
    m_cancelRequested = true;
}

void HelperSession::post(std::function<void()> task)
{
    {
        const std::lock_guard lock(m_mutex);
        m_posted.push_back(std::move(task));
    }
    m_dispatcher.emit();
}

void HelperSession::drain()
{
    std::deque<std::function<void()>> posted;
    {
        const std::lock_guard lock(m_mutex);
        posted.swap(m_posted);
    }
    for (auto &task : posted)
        task();
}

bool isNonce(const std::string &value)
{
    if (value.size() != 32)
        return false;
    for (const char character : value) {
        const bool digit = character >= '0' && character <= '9';
        const bool lower = character >= 'a' && character <= 'f';
        const bool upper = character >= 'A' && character <= 'F';
        if (!digit && !lower && !upper)
            return false;
    }
    return true;
}

bool HelperSession::launch(std::string &socketPath, std::string &nonce, std::string &error)
{
    const std::string helper = LOADOUT_HELPER_PATH;

    // Build the argument array before forking; the child must not allocate.
    std::vector<char *> arguments;
    arguments.push_back(const_cast<char *>("pkexec"));
    arguments.push_back(const_cast<char *>(helper.c_str()));
    arguments.push_back(nullptr);

    for (int attempt = 0; attempt < 2; ++attempt) {
        if (attempt > 0) {
            for (int tick = 0; tick < 15 && !m_cancelRequested; ++tick)
                usleep(100000);
            if (m_cancelRequested)
                return false;
        }

        int pipeFds[2] = {-1, -1};
        if (pipe2(pipeFds, O_CLOEXEC) != 0) {
            error = "could not create a pipe";
            return false;
        }

        const pid_t child = fork();
        if (child < 0) {
            close(pipeFds[0]);
            close(pipeFds[1]);
            error = "could not start pkexec";
            return false;
        }

        if (child == 0) {
            dup2(pipeFds[1], STDOUT_FILENO);
            close(pipeFds[0]);
            close(pipeFds[1]);
            execv("/usr/bin/pkexec", arguments.data());
            _exit(127);
        }

        close(pipeFds[1]);

        const auto started = std::chrono::steady_clock::now();
        std::string line;
        bool finished = false;
        while (!finished) {
            if (m_cancelRequested) {
                kill(child, SIGTERM);
                break;
            }

            pollfd entry{pipeFds[0], POLLIN, 0};
            const int ready = poll(&entry, 1, 500);
            if (ready < 0 && errno == EINTR)
                continue;
            if (ready <= 0) {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - started)
                                         .count();
                if (ready == 0 && elapsed < 120000)
                    continue;
                break;
            }

            char character = 0;
            const ssize_t size = read(pipeFds[0], &character, 1);
            if (size <= 0)
                break;
            if (character == '\n') {
                finished = true;
                break;
            }
            line += character;
        }
        close(pipeFds[0]);

        if (finished) {
            const auto space = line.rfind(' ');
            if (space != std::string::npos) {
                std::string path = line.substr(0, space);
                std::string token = line.substr(space + 1);
                if (path.rfind("/run/lgl-", 0) == 0 && isNonce(token)) {
                    m_helperPid = child;
                    socketPath = std::move(path);
                    nonce = std::move(token);
                    return true;
                }
            }
        }

        kill(child, SIGTERM);
        int status = 0;
        waitpid(child, &status, 0);
        error = "the privileged helper did not start";
    }

    return false;
}

bool HelperSession::authenticate(const std::string &nonce)
{
    if (!sendAll(m_fd, protocol::authRequest(nonce) + "\n"))
        return false;

    std::string buffer;
    std::string line;
    bool cancelSent = false;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(15000);

    while (std::chrono::steady_clock::now() < deadline) {
        const auto status = readLine(m_fd, buffer, line, m_cancelRequested, cancelSent);
        if (status == ReadStatus::Closed)
            return false;
        if (status == ReadStatus::Timeout)
            continue;
        const auto response = protocol::parseResponse(line);
        return response.kind == protocol::Response::Kind::AuthOk;
    }

    return false;
}

void HelperSession::stopHelper()
{
    if (m_helperPid <= 0)
        return;

    const pid_t pid = m_helperPid;
    m_helperPid = -1;

    kill(pid, SIGTERM);
    int status = 0;
    for (int tick = 0; tick < 20; ++tick) {
        if (waitpid(pid, &status, WNOHANG) == pid)
            return;
        usleep(100000);
    }
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
}

void HelperSession::run(PlanBuilder builder)
{
    std::string socketPath;
    std::string nonce;
    std::string error;
    if (!launch(socketPath, nonce, error)) {
        post([this, error] {
            m_failed.emit(error);
            m_running = false;
            m_done.emit(1);
        });
        return;
    }

    auto steps = builder(socketPath.substr(0, socketPath.rfind('/')));

    m_fd = connectSocket(socketPath);
    if (m_fd < 0) {
        post([this] {
            m_failed.emit("could not connect to the privileged helper");
            m_running = false;
            m_done.emit(1);
        });
        stopHelper();
        return;
    }

    if (!authenticate(nonce)) {
        post([this] {
            m_failed.emit("the privileged helper rejected the session");
            m_running = false;
            m_done.emit(1);
        });
        close(m_fd);
        m_fd = -1;
        stopHelper();
        return;
    }

    std::string buffer;
    std::string requestId = std::to_string(++m_requestCounter);

    const auto execute = [&](const std::vector<std::string> &argv, const bool stream) -> int {
        if (!sendAll(m_fd, protocol::executeRequest(requestId, argv) + "\n"))
            return -1;

        bool cancelSent = false;
        while (true) {
            std::string line;
            const auto status = readLine(m_fd, buffer, line, m_cancelRequested, cancelSent);
            if (status == ReadStatus::Closed)
                return -1;
            if (status == ReadStatus::Timeout)
                continue;

            const auto response = protocol::parseResponse(line);
            if (response.kind == protocol::Response::Kind::Output) {
                if (stream) {
                    const std::string text = response.line;
                    post([this, text] { m_log.emit(text); });
                }
            } else if (response.kind == protocol::Response::Kind::Finished) {
                return response.exitCode;
            } else if (response.kind == protocol::Response::Kind::Rejected ||
                       response.kind == protocol::Response::Kind::Error) {
                const std::string text = response.error;
                if (stream)
                    post([this, text] { m_log.emit("ERROR: " + text); });
                return -1;
            } else if (response.kind == protocol::Response::Kind::CancelAck) {
                return -1;
            }
        }
    };

    int errorCount = 0;
    std::map<std::string, bool> stepOk;

    for (const auto &step : steps) {
        if (m_cancelRequested)
            break;

        const std::string id = step.id;

        // A dependent step (publishing an AppImage after a download) only runs
        // when the step it depends on succeeded.
        if (!step.dependsOn.empty()) {
            const auto dependency = stepOk.find(step.dependsOn);
            if (dependency == stepOk.end() || !dependency->second) {
                post([this, id] { m_stepSkipped.emit(id, "skipped after a failed step"); });
                continue;
            }
        }

        const std::string description = step.description;
        post([this, id, description] { m_stepStarted.emit(id, description); });

        requestId = std::to_string(++m_requestCounter);

        if (step.local) {
            // Unprivileged work in the user's home, e.g. AppImages in
            // ~/Applications. Never sent to the root helper.
            const auto result = runCommand(step.command, 900000,
                                           [this] { return m_cancelRequested.load(); });
            std::size_t start = 0;
            while (start <= result.output.size()) {
                const auto newline = result.output.find('\n', start);
                std::string line = result.output.substr(
                    start, newline == std::string::npos ? std::string::npos : newline - start);
                while (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (!line.empty()) {
                    const std::string text = line;
                    post([this, text] { m_log.emit(text); });
                }
                if (newline == std::string::npos)
                    break;
                start = newline + 1;
            }

            bool ok = result.exitCode == 0;
            int code = result.exitCode;
            if (ok && !step.expectedSha256.empty()) {
                const auto sum = runCommand({"/usr/bin/sha256sum", step.verifyPath}, 60000,
                                            [this] { return m_cancelRequested.load(); });
                std::string digest;
                if (sum.exitCode == 0)
                    digest = sum.output.substr(0, sum.output.find_first_of(" \t\r\n"));
                if (lowered(digest) != lowered(step.expectedSha256)) {
                    ok = false;
                    code = -1;
                    const std::string text =
                        "ERROR: checksum verification failed for " + step.verifyPath;
                    post([this, text] { m_log.emit(text); });
                }
            }
            stepOk[id] = ok;
            if (!ok && !step.optional)
                ++errorCount;
            post([this, id, ok, code] { m_stepFinished.emit(id, ok, code); });
            continue;
        }

        if (!step.alreadyInstalledCheck.empty()) {
            if (execute(step.alreadyInstalledCheck, false) == 0) {
                stepOk[id] = true;
                post([this, id] { m_stepSkipped.emit(id, "already installed"); });
                continue;
            }
        }

        if (step.command.empty()) {
            stepOk[id] = true;
            post([this, id] { m_stepFinished.emit(id, true, 0); });
            continue;
        }

        const int code = execute(step.command, true);
        const bool ok = code == 0 || step.allowedExitCodes.count(code) > 0;
        stepOk[id] = ok;
        if (!ok && !step.optional)
            ++errorCount;
        post([this, id, ok, code] { m_stepFinished.emit(id, ok, code); });
    }

    sendAll(m_fd, protocol::simpleRequest("shutdown") + "\n");
    ::shutdown(m_fd, SHUT_RDWR);
    close(m_fd);
    m_fd = -1;
    stopHelper();

    post([this, errorCount] {
        m_running = false;
        m_done.emit(errorCount);
    });
}

} // namespace loadout
