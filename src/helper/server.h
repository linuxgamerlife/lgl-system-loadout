#pragma once

#include "helper/executor.h"

#include <sys/types.h>

#include <string>

namespace loadout::helper {

// Unix-socket server for the privileged helper.
//
// Creates a randomised directory under /run, binds a socket owned by the pkexec
// caller, then waits for that caller to prove it holds the per-session nonce
// printed on the helper's stdout. Only then does it accept commands, all of
// which are validated against the allow-list. It serves exactly one client.
class Server final {
public:
    Server() = default;
    ~Server();

    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    // Creates the directory and listening socket. The path and nonce are
    // returned so main() can print them for the GUI to read.
    bool start(std::string &socketPath);

    // The per-session secret the GUI must send before any command.
    const std::string &nonce() const { return m_nonce; }

    // Blocks until shutdown or the client disconnects.
    void serve();

private:
    bool acceptClient();
    bool readMessage(std::string &line, int timeoutMs = 0);
    void pumpClient();
    void handle(const std::string &raw);
    void sendLine(const std::string &json);
    void cleanup();

    int m_listenFd{-1};
    int m_clientFd{-1};
    uid_t m_expectedUid{0};
    std::string m_expectedUser;
    std::string m_nonce;
    std::string m_directory;
    std::string m_socketPath;
    std::string m_buffer;
    Executor m_executor;
    bool m_cancelRequested{false};
    bool m_done{false};
};

} // namespace loadout::helper
