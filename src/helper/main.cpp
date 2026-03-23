// lgl-system-loadout-helper — privileged helper entry point
//
// This binary is launched exclusively via pkexec and runs as root for the
// duration of the install session. It must never be run directly by the user.
//
// Launch flow:
//   1. pkexec elevates this binary (polkit prompts once, no _keep caching).
//   2. Helper creates a QLocalServer on a randomised socket path under /run.
//   3. Socket path is written to stdout so the GUI can connect.
//   4. Helper accepts exactly one client connection (the GUI process).
//   5. GUI sends newline-delimited JSON operation messages.
//   6. Helper validates each message against the allow-list and executes.
//   7. Helper streams output lines back as JSON.
//   8. On "shutdown" message or client disconnect, helper exits cleanly.
//
// Security properties:
//   - Socket created with permissions 0600, owned by root.
//   - Only one client connection accepted per session.
//   - All operations validated against a strict allow-list before execution.
//   - No shell invocation — commands executed via direct execvp-style QProcess.
//   - Socket directory cleaned up on exit regardless of success or failure.

#include "helperserver.h"
#include <QCoreApplication>
#include <QTextStream>
#include <unistd.h>
#include <QFile>

int main(int argc, char *argv[])
{
    // Verify we are actually running as root. pkexec guarantees this, but an
    // explicit check prevents misuse if the binary is invoked directly.
    if (geteuid() != 0) {
        QTextStream err(stderr);
        err << "lgl-system-loadout-helper: must be run as root via pkexec\n";
        return 1;
    }

    QCoreApplication app(argc, argv);
    app.setApplicationName("lgl-system-loadout-helper");
    app.setApplicationVersion("1.1.1");

    HelperServer server;
    if (!server.start()) {
        QTextStream err(stderr);
        err << "lgl-system-loadout-helper: failed to start socket server\n";
        return 1;
    }

    // Write socket path to stdout — pkexec forwards stdout to the GUI process.
    QTextStream out(stdout);
    out << server.socketPath() << Qt::endl;
    out.flush();

    // Connect shutdown signal to quit the event loop cleanly.
    QObject::connect(&server, &HelperServer::sessionEnded,
                     &app,    &QCoreApplication::quit);

    return app.exec();
}
