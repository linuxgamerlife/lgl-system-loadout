#include "ui/pages/donepage.h"

#include <gtkmm/alertdialog.h>
#include <gtkmm/scrolledwindow.h>

namespace loadout {

DonePage::DonePage(Notify notify)
    : m_notify(std::move(notify))
{
    m_summaryLabel.set_wrap(true);
    m_summaryLabel.set_xalign(0.0f);
    append(m_summaryLabel);

    m_detailCard.add_css_class("lo-card");
    m_detailCard.add_css_class("flush");
    m_detailCard.set_visible(false);

    auto *heading = Gtk::make_managed<Gtk::Label>("FAILED STEPS");
    heading->set_xalign(0.0f);
    heading->add_css_class("section-label");
    heading->add_css_class("card-head");
    m_detailCard.append(*heading);

    m_detail.set_editable(false);
    m_detail.set_cursor_visible(false);
    m_detail.add_css_class("install-log");
    auto *scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroller->set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    scroller->set_child(m_detail);
    scroller->set_size_request(-1, 260);
    m_detailCard.append(*scroller);
    append(m_detailCard);

    auto *buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    m_copyErrors.add_css_class("lo-small");
    m_copyErrors.signal_clicked().connect([this] {
        copy(m_summary.failedSteps, "Failed steps");
    });
    buttons->append(m_copyErrors);

    m_copyLog.add_css_class("lo-small");
    m_copyLog.signal_clicked().connect([this] { copy(m_summary.fullLog, "Full log"); });
    buttons->append(m_copyLog);

    m_rebootButton.add_css_class("lo-primary");
    m_rebootButton.add_css_class("suggested-action");
    m_rebootButton.set_hexpand(true);
    m_rebootButton.set_halign(Gtk::Align::END);
    m_rebootButton.signal_clicked().connect([this] { reboot(); });
    buttons->append(m_rebootButton);
    append(*buttons);

    m_session.signalFailed().connect([this](const std::string &message) {
        m_notify(message, false);
    });
    m_session.signalDone().connect([this](const int errors) {
        if (errors != 0)
            m_notify("The reboot command did not complete. Reboot manually.", false);
    });
}

void DonePage::setSummary(const InstallSummary &summary)
{
    m_summary = summary;
}

void DonePage::onShown()
{
    if (m_summary.errorCount == 0) {
        m_summaryLabel.set_text(
            "All selected packages and tools are installed.\n\n"
            "A reboot is recommended so that everything takes effect, especially if the "
            "CachyOS kernel, GPU drivers, or virtualisation packages were installed.");
        m_detailCard.set_visible(false);
        m_copyErrors.set_visible(false);
        return;
    }

    m_summaryLabel.set_text(
        std::to_string(m_summary.errorCount) +
        " step(s) did not complete successfully. Their output is below; you can copy it to "
        "report the problem. Anything that installed successfully stays installed, and a "
        "reboot is still recommended.");
    m_detail.get_buffer()->set_text(m_summary.failedSteps.empty() ? "(No detail available)"
                                                                 : m_summary.failedSteps);
    m_detailCard.set_visible(true);
    m_copyErrors.set_visible(true);
}

void DonePage::copy(const std::string &text, const std::string &what)
{
    if (text.empty()) {
        m_notify(what + " is empty.", false);
        return;
    }
    get_clipboard()->set_text(text);
    m_notify(what + " copied to the clipboard.", true);
}

void DonePage::reboot()
{
    auto dialog = Gtk::AlertDialog::create();
    dialog->set_message("Reboot now?");
    dialog->set_detail("Save any open files first.");
    dialog->set_buttons({"Cancel", "Reboot"});
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
        m_session.start([](const std::string &) {
            return std::vector<InstallStep>{
                InstallStep{"reboot", "Reboot", {"/usr/bin/systemctl", "reboot"}}};
        });
    });
}

} // namespace loadout
