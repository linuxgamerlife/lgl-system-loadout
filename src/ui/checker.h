#pragma once

#include "core/plan.h"

#include <glibmm/dispatcher.h>
#include <sigc++/sigc++.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace loadout {

// Runs installed-state probes on a worker thread, one at a time, so the UI is
// never blocked and concurrent rpm processes do not trip over each other.
class Checker final {
public:
    Checker();
    ~Checker();
    Checker(const Checker &) = delete;
    Checker &operator=(const Checker &) = delete;

    void request(const std::string &key, const Check &check);
    void requestCommand(const std::string &key, std::vector<std::string> argv);

    sigc::signal<void(const std::string &, bool)> &signalResult() { return m_result; }
    sigc::signal<void()> &signalIdle() { return m_idle; }

private:
    struct Job final {
        std::string key;
        std::function<bool()> work;
    };

    void submit(std::string key, std::function<bool()> work);
    void work();
    void drain();

    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::deque<Job> m_jobs;
    std::deque<std::function<void()>> m_posted;
    bool m_stop{false};
    std::thread m_thread;
    Glib::Dispatcher m_dispatcher;
    std::atomic<int> m_pending{0};
    sigc::signal<void(const std::string &, bool)> m_result;
    sigc::signal<void()> m_idle;
};

} // namespace loadout
