#pragma once

#include "core/system.h"
#include "ui/helpersession.h"
#include "ui/page.h"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>
#include <sigc++/sigc++.h>

#include <string>

namespace loadout {

// The optional system update, its Flatpak follow-up, and the reboot gate when a
// new kernel was installed.
class UpdatePage final : public Page {
public:
    UpdatePage(const SystemInfo &system, Notify notify);
    ~UpdatePage() override;

    void onShown() override;
    bool canAdvance() const override { return m_complete; }
    bool navigationLocked() const override { return m_busy; }

private:
    enum class Phase { Idle, System, Flatpak, Reboot };

    void buildBody();
    void startUpdate();
    void startFlatpak();
    void finishUpdate();
    void markComplete();
    void confirmSkip();
    void reboot();
    void appendLog(const std::string &text);
    void setBusy(bool busy);
    std::string kernelList() const;
    void wireSession();

    const SystemInfo &m_system;
    Notify m_notify;
    HelperSession m_session;

    Phase m_phase{Phase::Idle};
    bool m_busy{false};
    bool m_complete{false};
    std::string m_kernelsBefore;

    Gtk::Button m_updateButton{"Update Now"};
    Gtk::Button m_skipButton{"Skip (not recommended)"};
    Gtk::Label m_status;
    Gtk::ProgressBar m_progress;
    Gtk::TextView m_log;
    Gtk::ScrolledWindow *m_logScroller{nullptr};
    Gtk::Box m_flatpakBox{Gtk::Orientation::HORIZONTAL, 8};
    Gtk::Button m_flatpakYes{"Yes"};
    Gtk::Button m_flatpakNo{"No"};
    Gtk::Box m_kernelBox{Gtk::Orientation::HORIZONTAL, 8};
    Gtk::Label m_kernelLabel;
    Gtk::Button m_rebootButton{"Reboot Now"};
    Gtk::Button m_continueButton{"Continue Anyway"};
    sigc::connection m_pulse;
};

} // namespace loadout
