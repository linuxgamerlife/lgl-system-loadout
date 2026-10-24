#include "core/process.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>

namespace loadout {
namespace {

constexpr std::size_t kMaxOutput = 8 * 1024 * 1024;

void appendChunk(std::string &output, const char *data, const ssize_t size)
{
    if (size <= 0 || output.size() >= kMaxOutput)
        return;
    const std::size_t room = kMaxOutput - output.size();
    output.append(data, std::min(room, static_cast<std::size_t>(size)));
}

} // namespace

CommandResult runCommand(const std::vector<std::string> &argv, const int timeoutMs,
                         const std::function<bool()> &cancelled)
{
    CommandResult result;
    if (argv.empty())
        return result;

    // Build the argument array before forking: allocating after fork in a
    // multithreaded process can deadlock on a malloc lock held by another thread.
    std::vector<char *> arguments;
    arguments.reserve(argv.size() + 1);
    for (const auto &argument : argv)
        arguments.push_back(const_cast<char *>(argument.c_str()));
    arguments.push_back(nullptr);

    int pipeFds[2] = {-1, -1};
    if (pipe2(pipeFds, O_CLOEXEC) != 0)
        return result;

    const pid_t child = fork();
    if (child < 0) {
        close(pipeFds[0]);
        close(pipeFds[1]);
        return result;
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

    const auto started = std::chrono::steady_clock::now();
    bool killed = false;
    char buffer[4096];

    while (true) {
        if (cancelled && cancelled()) {
            result.cancelled = true;
            killed = true;
            kill(child, SIGKILL);
            break;
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - started)
                                 .count();
        if (timeoutMs > 0 && elapsed >= timeoutMs) {
            result.timedOut = true;
            killed = true;
            kill(child, SIGKILL);
            break;
        }

        pollfd entry{pipeFds[0], POLLIN, 0};
        const int ready = poll(&entry, 1, 200);
        if (ready > 0) {
            const ssize_t size = read(pipeFds[0], buffer, sizeof(buffer));
            if (size > 0) {
                appendChunk(result.output, buffer, size);
                continue;
            }
            if (size < 0 && errno == EINTR)
                continue;
            break;
        }
        if (ready < 0 && errno != EINTR)
            break;
    }

    close(pipeFds[0]);

    // If the child closed stdout but is still running, the read loop exits
    // early. Reap it with a deadline rather than blocking forever.
    int status = 0;
    bool reaped = false;
    while (true) {
        const pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child) {
            reaped = true;
            break;
        }
        if (waited < 0) {
            if (errno == EINTR)
                continue;
            break;
        }

        if (!killed) {
            if (cancelled && cancelled()) {
                result.cancelled = true;
                killed = true;
                kill(child, SIGKILL);
            } else {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - started)
                                         .count();
                if (timeoutMs > 0 && elapsed >= timeoutMs) {
                    result.timedOut = true;
                    killed = true;
                    kill(child, SIGKILL);
                }
            }
        }

        usleep(20000);
    }

    if (!reaped || result.timedOut || result.cancelled) {
        result.exitCode = -1;
    } else if (WIFEXITED(status)) {
        result.exitCode = WEXITSTATUS(status);
    } else {
        result.exitCode = -1;
    }
    return result;
}

} // namespace loadout
