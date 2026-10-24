#pragma once

#include "core/plan.h"
#include "core/selection.h"
#include "core/system.h"
#include "ui/helpersession.h"
#include "ui/page.h"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/textview.h>

#include <map>
#include <string>

namespace loadout {

struct InstallSummary final {
    int errorCount{0};
    std::string failedSteps;
    std::string fullLog;
};

class InstallPage final : public Page {
public:
    InstallPage(Selection &selection, const SystemInfo &system, Notify notify);

    void onShown() override;
    bool canAdvance() const override { return m_done; }
    bool navigationLocked() const override { return m_running; }
    const InstallSummary &summary() const { return m_summary; }

private:
    void prepare();
    void start();
    void askCancel();
    void appendLog(const std::string &text);
    void addStepRow(const InstallStep &step);
    void wireSession();

    Selection &m_selection;
    const SystemInfo &m_system;
    Notify m_notify;
    HelperSession m_session;

    Gtk::Label m_status;
    Gtk::ProgressBar m_progress;
    Gtk::ListBox m_stepList;
    Gtk::TextView m_log;
    Gtk::Button m_startButton{"Start Installation"};
    Gtk::Button m_cancelButton{"Stop"};

    std::map<std::string, Gtk::Label *> m_chips;
    std::map<std::string, std::string> m_descriptions;
    std::map<std::string, std::string> m_stepLogs;
    std::string m_currentStep;
    InstallSummary m_summary;

    int m_totalSteps{0};
    int m_doneSteps{0};
    bool m_started{false};
    bool m_running{false};
    bool m_done{false};
};

} // namespace loadout
