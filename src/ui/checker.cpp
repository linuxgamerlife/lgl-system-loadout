#include "ui/checker.h"

#include "core/process.h"

#include <utility>

namespace loadout {

Checker::Checker()
{
    m_dispatcher.connect([this] { drain(); });
}

Checker::~Checker()
{
    {
        const std::lock_guard lock(m_mutex);
        m_stop = true;
    }
    m_condition.notify_all();
    if (m_thread.joinable())
        m_thread.join();
}

void Checker::request(const std::string &key, const Check &check)
{
    if (check.any.empty())
        return;
    submit(key, [check] {
        bool matched = false;
        for (const auto &argv : check.any) {
            const auto result = runCommand(argv, 8000);
            if (result.exitCode != 0)
                continue;
            if (!check.outputContains.empty() &&
                result.output.find(check.outputContains) == std::string::npos)
                continue;
            matched = true;
            break;
        }
        return check.negate ? !matched : matched;
    });
}

void Checker::requestCommand(const std::string &key, std::vector<std::string> argv)
{
    submit(key, [argv = std::move(argv)] { return runCommand(argv, 8000).exitCode == 0; });
}

void Checker::submit(std::string key, std::function<bool()> work)
{
    {
        const std::lock_guard lock(m_mutex);
        m_jobs.push_back(Job{std::move(key), std::move(work)});
        ++m_pending;
    }
    m_condition.notify_one();

    if (!m_thread.joinable())
        m_thread = std::thread(&Checker::work, this);
}

void Checker::work()
{
    while (true) {
        Job job;
        {
            std::unique_lock lock(m_mutex);
            m_condition.wait(lock, [this] { return m_stop || !m_jobs.empty(); });
            if (m_stop && m_jobs.empty())
                return;
            job = std::move(m_jobs.front());
            m_jobs.pop_front();
        }

        const bool installed = job.work();
        const std::string key = job.key;
        {
            const std::lock_guard lock(m_mutex);
            m_posted.push_back([this, key, installed] {
                m_result.emit(key, installed);
                if (--m_pending == 0)
                    m_idle.emit();
            });
        }
        m_dispatcher.emit();
    }
}

void Checker::drain()
{
    std::deque<std::function<void()>> posted;
    {
        const std::lock_guard lock(m_mutex);
        posted.swap(m_posted);
    }
    for (auto &task : posted)
        task();
}

} // namespace loadout
