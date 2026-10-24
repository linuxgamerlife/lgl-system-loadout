#include "ui/pages/updatepage.h"

#include "core/process.h"

#include <glibmm/main.h>
#include <gtkmm/alertdialog.h>
#include <gtkmm/scrolledwindow.h>

namespace loadout {
namespace {

void addLabel(Gtk::Box &box, const std::string &text, const char *cssClass)
{
    auto *label = Gtk::make_managed<Gtk::Label>(text);
    label->set_wrap(true);
    label->set_xalign(0.0f);
    if (cssClass)
        label->add_css_class(cssClass);
    box.append(*label);
}

} // namespace

UpdatePage::UpdatePage(const SystemInfo &system, Notify notify)
    : m_system(system)
    , m_notify(std::move(notify))
{
    set_valign(Gtk::Align::FILL);
    buildBody();
    wireSession();
}

UpdatePage::~UpdatePage()
{
    m_pulse.disconnect();
}

void UpdatePage::buildBody()
{
    auto *info = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    info->add_css_class("lo-card");
    addLabel(*info, "WHY UPDATE FIRST", "section-label");
    addLabel(*info,
             "Installing software on top of an outdated system can cause package conflicts, "
             "dependency errors, and instability. A full update puts the system in a clean, "
             "consistent state before anything new is added. The first repository refresh can "
             "take a few minutes on a slow connection; later updates use the cached metadata.",
             nullptr);
    append(*info);

    auto *buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    m_updateButton.add_css_class("lo-primary");
    m_updateButton.add_css_class("suggested-action");
    m_updateButton.signal_clicked().connect([this] { startUpdate(); });
    buttons->append(m_updateButton);

    m_skipButton.add_css_class("lo-small");
    m_skipButton.signal_clicked().connect([this] { confirmSkip(); });
    buttons->append(m_skipButton);

    append(*buttons);

    m_status.set_wrap(true);
    m_status.set_xalign(0.0f);
    m_status.add_css_class("muted");
    append(m_status);

    m_progress.set_visible(false);
    m_progress.set_pulse_step(0.08);
    append(m_progress);

    m_log.set_editable(false);
    m_log.set_cursor_visible(false);
    m_log.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    m_log.add_css_class("install-log");
    auto *logScroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    logScroller->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    logScroller->set_child(m_log);
    logScroller->set_vexpand(true);
    logScroller->set_size_request(-1, 180);
    logScroller->set_visible(false);
    logScroller->add_css_class("lo-card");
    m_logScroller = logScroller;

    m_flatpakBox.set_visible(false);
    auto *flatpakLabel = Gtk::make_managed<Gtk::Label>("Update Flatpaks now as well?");
    flatpakLabel->set_xalign(0.0f);
    flatpakLabel->set_hexpand(true);
    m_flatpakBox.append(*flatpakLabel);
    m_flatpakYes.add_css_class("lo-small");
    m_flatpakYes.signal_clicked().connect([this] { startFlatpak(); });
    m_flatpakBox.append(m_flatpakYes);
    m_flatpakNo.add_css_class("lo-small");
    m_flatpakNo.signal_clicked().connect([this] { finishUpdate(); });
    m_flatpakBox.append(m_flatpakNo);
    append(m_flatpakBox);

    m_kernelBox.set_visible(false);
    m_kernelLabel.set_wrap(true);
    m_kernelLabel.set_xalign(0.0f);
    m_kernelLabel.set_hexpand(true);
    m_kernelLabel.add_css_class("disk-banner");
    m_kernelLabel.add_css_class("neutral");
    m_kernelBox.append(m_kernelLabel);
    m_rebootButton.add_css_class("lo-small");
    m_rebootButton.signal_clicked().connect([this] { reboot(); });
    m_kernelBox.append(m_rebootButton);
    m_continueButton.add_css_class("lo-small");
    m_continueButton.signal_clicked().connect([this] { markComplete(); });
    m_kernelBox.append(m_continueButton);
    append(m_kernelBox);

    // The prompts stay above the log so they are visible without scrolling;
    // the log then fills the remaining space.
    append(*logScroller);
}

void UpdatePage::wireSession()
{
    m_session.signalStepStarted().connect(
        [this](const std::string &, const std::string &description) {
            m_status.set_text("Running: " + description);
            appendLog("\n==> " + description);
        });
    m_session.signalLog().connect([this](const std::string &line) { appendLog(line); });
    m_session.signalStepFinished().connect(
        [this](const std::string &, const bool ok, const int code) {
            if (ok)
                appendLog("OK");
            else
                appendLog("FAILED (exit " + std::to_string(code) + ")");
        });
    m_session.signalFailed().connect([this](const std::string &message) {
        appendLog("ERROR: " + message);
        m_status.set_text(message);
    });
    m_session.signalDone().connect([this](const int errors) {
        setBusy(false);
        if (m_phase == Phase::System) {
            m_phase = Phase::Idle;
            m_status.set_text(errors == 0
                                  ? "System update complete."
                                  : "The update finished with errors. Check the log above.");
            m_flatpakBox.set_visible(true);
            if (errors == 0) {
                m_updateButton.set_sensitive(false);
                m_skipButton.set_sensitive(false);
            }
        } else if (m_phase == Phase::Flatpak) {
            m_phase = Phase::Idle;
            m_status.set_text(errors == 0 ? "Flatpak update complete."
                                          : "The Flatpak update finished with errors.");
            m_flatpakBox.set_visible(false);
            finishUpdate();
        } else if (m_phase == Phase::Reboot) {
            m_phase = Phase::Idle;
            m_status.set_text("The reboot command did not complete. Reboot manually.");
        }
    });
}

void UpdatePage::setBusy(const bool busy)
{
    m_busy = busy;
    m_updateButton.set_sensitive(!busy && !m_complete);
    m_skipButton.set_sensitive(!busy && !m_complete);
    m_progress.set_visible(busy);
    if (m_logScroller && busy)
        m_logScroller->set_visible(true);
    m_pulse.disconnect();
    if (busy) {
        m_pulse = Glib::signal_timeout().connect(
            [this] {
                m_progress.pulse();
                return true;
            },
            200);
    }
    stateChanged();
}

void UpdatePage::startUpdate()
{
    m_kernelsBefore = kernelList();
    m_phase = Phase::System;
    setBusy(true);
    m_status.set_text("Launching the privileged helper…");
    appendLog("==> System update");

    m_session.start([](const std::string &) {
        return std::vector<InstallStep>{
            InstallStep{"system_update", "System update: dnf upgrade --refresh",
                        {"/usr/bin/dnf", "-y", "upgrade", "--refresh"}}};
    });
}

void UpdatePage::startFlatpak()
{
    m_flatpakBox.set_visible(false);
    m_phase = Phase::Flatpak;
    setBusy(true);
    m_status.set_text("Updating Flatpaks…");

    m_session.start([](const std::string &) {
        return std::vector<InstallStep>{
            InstallStep{"flatpak_update", "Flatpak update",
                        {"/usr/bin/flatpak", "update", "-y", "--system"}, true}};
    });
}

void UpdatePage::finishUpdate()
{
    if (kernelList() != m_kernelsBefore) {
        m_kernelLabel.set_text("A new kernel was installed. Reboot before the new kernel takes "
                               "effect; continuing now is not recommended.");
        m_kernelBox.set_visible(true);
        m_status.set_text("Update complete.");
        return;
    }

    markComplete();
}

void UpdatePage::markComplete()
{
    m_complete = true;
    m_updateButton.set_sensitive(false);
    m_skipButton.set_sensitive(false);
    m_flatpakBox.set_visible(false);
    m_kernelBox.set_visible(false);
    stateChanged();
}

std::string UpdatePage::kernelList() const
{
    return runCommand({"/usr/bin/rpm", "-q", "kernel", "kernel-cachyos", "--queryformat",
                       "%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n"},
                      5000)
        .output;
}

void UpdatePage::confirmSkip()
{
    auto dialog = Gtk::AlertDialog::create();
    dialog->set_message("Skip the system update?");
    dialog->set_detail("Installing on top of an outdated system can cause package conflicts, "
                       "dependency errors, and instability. Updating first is strongly "
                       "recommended.");
    dialog->set_buttons({"Cancel", "Skip Update"});
    dialog->set_cancel_button(0);
    dialog->set_default_button(0);

    auto *window = hostWindow();
    if (!window)
        return;

    dialog->choose(*window, [this, dialog](const Glib::RefPtr<Gio::AsyncResult> &result) {
        try {
            if (dialog->choose_finish(result) != 1)
                return;
        } catch (const Glib::Error &) {
            return;
        }
        m_status.set_text("System update skipped. Continue at your own risk.");
        markComplete();
    });
}

void UpdatePage::reboot()
{
    auto dialog = Gtk::AlertDialog::create();
    dialog->set_message("Reboot now?");
    dialog->set_detail("Save any open files first. After rebooting, open LGL System Loadout again "
                     "to continue with your selections.");
    dialog->set_buttons({"Cancel", "Reboot"});
    dialog->set_cancel_button(0);
    dialog->set_default_button(0);

    auto *window = hostWindow();
    if (!window)
        return;

    dialog->choose(*window, [this, dialog](const Glib::RefPtr<Gio::AsyncResult> &result) {
        int choice = 0;
        try {
            choice = dialog->choose_finish(result);
        } catch (const Glib::Error &) {
            return;
        }
        if (choice != 1)
            return;

        m_phase = Phase::Reboot;
        m_status.set_text("Rebooting…");
        m_session.start([](const std::string &) {
            return std::vector<InstallStep>{
                InstallStep{"reboot", "Reboot", {"/usr/bin/systemctl", "reboot"}}};
        });
    });
}

void UpdatePage::appendLog(const std::string &text)
{
    auto buffer = m_log.get_buffer();
    buffer->insert(buffer->end(), text + "\n");
    auto end = buffer->end();
    m_log.scroll_to(end);
}

void UpdatePage::onShown()
{
}

} // namespace loadout
