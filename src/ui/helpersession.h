#pragma once

#include "core/plan.h"

#include <glibmm/dispatcher.h>
#include <sigc++/sigc++.h>
#include <sys/types.h>

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace loadout {

// One privileged session: launches the helper through pkexec, connects to its
// socket, and runs an install plan on a worker thread. Every signal is emitted
// on the main thread.
class HelperSession final : public sigc::trackable {
public:
    // Called with the helper's session directory so the plan can place
    // temporary files in a root-owned location.
    using PlanBuilder = std::function<std::vector<InstallStep>(const std::string &socketDir)>;

    HelperSession();
    ~HelperSession();
    HelperSession(const HelperSession &) = delete;
    HelperSession &operator=(const HelperSession &) = delete;

    void start(PlanBuilder builder);
    void cancel();
    bool running() const { return m_running.load(); }

    sigc::signal<void(const std::string &, const std::string &)> &signalStepStarted()
    {
        return m_stepStarted;
    }
    sigc::signal<void(const std::string &, bool, int)> &signalStepFinished()
    {
        return m_stepFinished;
    }
    sigc::signal<void(const std::string &, const std::string &)> &signalStepSkipped()
    {
        return m_stepSkipped;
    }
    sigc::signal<void(const std::string &)> &signalLog() { return m_log; }
    sigc::signal<void(const std::string &)> &signalFailed() { return m_failed; }
    sigc::signal<void(int)> &signalDone() { return m_done; }

private:
    void run(PlanBuilder builder);
    void post(std::function<void()> task);
    void drain();
    bool launch(std::string &socketPath, std::string &nonce, std::string &error);
    bool authenticate(const std::string &nonce);
    void stopHelper();

    std::thread m_thread;
    Glib::Dispatcher m_dispatcher;
    std::mutex m_mutex;
    std::deque<std::function<void()>> m_posted;
    std::atomic<bool> m_cancelRequested{false};
    std::atomic<bool> m_running{false};
    int m_fd{-1};
    int m_requestCounter{0};
    pid_t m_helperPid{-1};

    sigc::signal<void(const std::string &, const std::string &)> m_stepStarted;
    sigc::signal<void(const std::string &, bool, int)> m_stepFinished;
    sigc::signal<void(const std::string &, const std::string &)> m_stepSkipped;
    sigc::signal<void(const std::string &)> m_log;
    sigc::signal<void(const std::string &)> m_failed;
    sigc::signal<void(int)> m_done;
};

} // namespace loadout
