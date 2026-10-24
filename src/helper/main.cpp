// lgl-system-loadout-helper - privileged helper entry point.
//
// Launched only by pkexec, which elevates it and sets PKEXEC_UID to the calling
// user. The helper creates a Unix socket under /run, prints its path on stdout,
// accepts exactly one client, and runs only the operations on its allow-list.
//
// The GUI never runs as root.

#include "helper/server.h"

#include <unistd.h>

#include <cstdio>

int main()
{
    if (geteuid() != 0) {
        std::fprintf(stderr, "lgl-system-loadout-helper: must be run through pkexec\n");
        return 1;
    }

    loadout::helper::Server server;
    std::string socketPath;
    if (!server.start(socketPath)) {
        std::fprintf(stderr, "lgl-system-loadout-helper: could not start the socket server\n");
        return 1;
    }

    std::printf("%s %s\n", socketPath.c_str(), server.nonce().c_str());
    std::fflush(stdout);

    server.serve();
    return 0;
}
