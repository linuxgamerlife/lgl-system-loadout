#include "helper/executor.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>

namespace loadout::helper {
namespace {

constexpr int kPollMs = 150;
constexpr int kTermGraceMs = 8000;
constexpr std::size_t kMaxLine = 1024 * 1024;

// Emits complete lines. Flatpak and friends use \r to update progress in place,
// so only the text after the last \r of a line is reported.
void flushLines(std::string &buffer, const Executor::LineSink &sink, const bool flushAll)
{
    std::size_t newline = 0;
    while ((newline = buffer.find('\n')) != std::string::npos) {
        std::string line = buffer.substr(0, newline);
        buffer.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const auto carriageReturn = line.rfind('\r');
        if (carriageReturn != std::string::npos)
            line = line.substr(carriageReturn + 1);
        if (!line.empty())
            sink(line);
    }

    if (!buffer.empty()) {
        const auto carriageReturn = buffer.rfind('\r');
        if (carriageReturn != std::string::npos) {
            std::string line = buffer.substr(carriageReturn + 1);
            buffer.erase(0, carriageReturn + 1);
            if (!line.empty())
                sink(line);
        }
    }

    if (buffer.size() > kMaxLine) {
        sink(buffer.substr(0, kMaxLine));
        buffer.erase(0, kMaxLine);
    }

    if (flushAll && !buffer.empty()) {
        sink(buffer);
        buffer.clear();
    }
}

} // namespace

int Executor::run(const std::vector<std::string> &argv, const LineSink &onLine,
                  const CancelCheck &cancelled, const Pump &pump)
{
    if (argv.empty())
        return -1;

    // Build the argument array before forking; the helper is single-threaded
    // today, but this keeps the child free of allocations either way.
    std::vector<char *> arguments;
    arguments.reserve(argv.size() + 1);
    for (const auto &argument : argv)
        arguments.push_back(const_cast<char *>(argument.c_str()));
    arguments.push_back(nullptr);

    int pipeFds[2] = {-1, -1};
    if (pipe2(pipeFds, O_CLOEXEC) != 0) {
        onLine("ERROR: could not create a pipe for the command");
        return -1;
    }

    const pid_t child = fork();
    if (child < 0) {
        close(pipeFds[0]);
        close(pipeFds[1]);
        onLine("ERROR: could not start the command");
        return -1;
    }

    if (child == 0) {
        dup2(pipeFds[1], STDOUT_FILENO);
        dup2(pipeFds[1], STDERR_FILENO);
        close(pipeFds[0]);
        close(pipeFds[1]);

        execv(arguments[0], arguments.data());
        _exit(127);
    }

    close(pipeFds[1]);
    const int flags = fcntl(pipeFds[0], F_GETFL, 0);
    fcntl(pipeFds[0], F_SETFL, flags | O_NONBLOCK);

    std::string buffer;
    bool cancelSent = false;
    bool exited = false;
    int status = 0;
    auto cancelStarted = std::chrono::steady_clock::now();

    while (!exited) {
        pump();

        char chunk[4096];
        while (true) {
            const ssize_t size = read(pipeFds[0], chunk, sizeof(chunk));
            if (size > 0) {
                buffer.append(chunk, static_cast<std::size_t>(size));
                flushLines(buffer, onLine, false);
                continue;
            }
            if (size < 0 && errno == EINTR)
                continue;
            break;
        }

        if (cancelled()) {
            if (!cancelSent) {
                onLine("INFO: Cancellation requested. Sending SIGTERM.");
                kill(child, SIGTERM);
                cancelSent = true;
                cancelStarted = std::chrono::steady_clock::now();
            } else {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - cancelStarted)
                                         .count();
                if (elapsed >= kTermGraceMs) {
                    onLine("INFO: The command did not exit after SIGTERM. Sending SIGKILL.");
                    kill(child, SIGKILL);
                }
            }
        }

        const pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child)
            exited = true;
        else
            usleep(kPollMs * 1000);
    }

    char chunk[4096];
    while (true) {
        const ssize_t size = read(pipeFds[0], chunk, sizeof(chunk));
        if (size > 0) {
            buffer.append(chunk, static_cast<std::size_t>(size));
            continue;
        }
        if (size < 0 && errno == EINTR)
            continue;
        break;
    }
    flushLines(buffer, onLine, true);
    close(pipeFds[0]);

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

} // namespace loadout::helper
