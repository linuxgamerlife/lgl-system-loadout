#include "ui/pages/installpage.h"

#include <gtkmm/alertdialog.h>
#include <gtkmm/scrolledwindow.h>

namespace loadout {

InstallPage::InstallPage(Selection &selection, const SystemInfo &system, Notify notify)
    : m_selection(selection)
    , m_system(system)
    , m_notify(std::move(notify))
{
    // Let the log take the full height of the page instead of sitting in a
    // fixed box with empty space below it.
    set_valign(Gtk::Align::FILL);

    m_status.set_wrap(true);
    m_status.set_xalign(0.0f);
    m_status.set_text("Preparing…");
    append(m_status);

    m_progress.set_show_text(true);
    append(m_progress);

    // The gate: nothing starts, and no password is requested, until this is
    // pressed. Kept above the log so it is visible without scrolling.
    auto *gate = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    gate->set_halign(Gtk::Align::END);
    m_startButton.add_css_class("suggested-action");
    m_startButton.signal_clicked().connect([this] { start(); });
    gate->append(m_startButton);
    append(*gate);

    auto *columns = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    columns->set_vexpand(true);

    auto *stepsCard = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    stepsCard->add_css_class("lo-card");
    stepsCard->add_css_class("flush");
    stepsCard->set_size_request(320, -1);

    auto *stepsHeading = Gtk::make_managed<Gtk::Label>("STEPS");
    stepsHeading->set_xalign(0.0f);
    stepsHeading->add_css_class("section-label");
    stepsHeading->add_css_class("card-head");
    stepsCard->append(*stepsHeading);

    m_stepList.set_selection_mode(Gtk::SelectionMode::NONE);
    m_stepList.add_css_class("install-steps");
    auto *stepsScroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    stepsScroller->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    stepsScroller->set_child(m_stepList);
    stepsScroller->set_vexpand(true);
    stepsCard->append(*stepsScroller);
    columns->append(*stepsCard);

    auto *logCard = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    logCard->add_css_class("lo-card");
    logCard->add_css_class("flush");
    logCard->set_hexpand(true);

    auto *logHeading = Gtk::make_managed<Gtk::Label>("OUTPUT");
    logHeading->set_xalign(0.0f);
    logHeading->add_css_class("section-label");
    logHeading->add_css_class("card-head");
    logCard->append(*logHeading);

    m_log.set_editable(false);
    m_log.set_cursor_visible(false);
    m_log.set_monospace(false);
    // Wrap long package-manager lines instead of clipping them at the edge.
    m_log.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    m_log.add_css_class("install-log");
    auto *logScroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    logScroller->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    logScroller->set_child(m_log);
    logScroller->set_vexpand(true);
    logCard->append(*logScroller);
    columns->append(*logCard);

    columns->set_size_request(-1, 300);
    append(*columns);

    auto *footer = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    footer->set_halign(Gtk::Align::END);
    m_cancelButton.add_css_class("lo-small");
    m_cancelButton.set_sensitive(false);
    m_cancelButton.set_visible(false);
    m_cancelButton.signal_clicked().connect([this] { askCancel(); });
    footer->append(m_cancelButton);
    append(*footer);

    wireSession();
}

void InstallPage::wireSession()
{
    m_session.signalStepStarted().connect(
        [this](const std::string &id, const std::string &description) {
            m_currentStep = id;
            m_status.set_text("Running: " + description);
            if (auto *chip = m_chips[id]) {
                chip->set_text("RUNNING");
                chip->set_visible(true);
            }
            appendLog("\n==> " + description);
        });

    m_session.signalLog().connect([this](const std::string &line) {
        appendLog(line);
        if (!m_currentStep.empty())
            m_stepLogs[m_currentStep] += line + "\n";
    });

    m_session.signalStepFinished().connect(
        [this](const std::string &id, const bool ok, const int code) {
            ++m_doneSteps;
            m_progress.set_fraction(m_totalSteps > 0
                                        ? static_cast<double>(m_doneSteps) / m_totalSteps
                                        : 0.0);
            if (auto *chip = m_chips[id]) {
                chip->set_text(ok ? "OK" : "FAILED");
                chip->remove_css_class("neutral");
                chip->add_css_class(ok ? "positive" : "negative");
                chip->set_visible(true);
            }
            appendLog(ok ? "OK" : "FAILED (exit " + std::to_string(code) + ")");
        });

    m_session.signalStepSkipped().connect(
        [this](const std::string &id, const std::string &reason) {
            ++m_doneSteps;
            m_progress.set_fraction(m_totalSteps > 0
                                        ? static_cast<double>(m_doneSteps) / m_totalSteps
                                        : 0.0);
            if (auto *chip = m_chips[id]) {
                chip->set_text(reason == "already installed" ? "ALREADY INSTALLED" : "SKIPPED");
                chip->set_visible(true);
            }
        });

    m_session.signalFailed().connect([this](const std::string &message) {
        m_status.set_text(message);
        appendLog("ERROR: " + message);
    });

    m_session.signalDone().connect([this](const int errors) {
        m_running = false;
        m_done = true;
        m_cancelButton.set_sensitive(false);
        m_summary.errorCount = errors;
        m_summary.fullLog = m_log.get_buffer()->get_text();

        std::string failed;
        for (const auto &[id, description] : m_descriptions) {
            const auto chip = m_chips.find(id);
            if (chip == m_chips.end() || !chip->second)
                continue;
            if (chip->second->get_text() != "FAILED")
                continue;
            failed += "Step: " + description + "\n" + m_stepLogs[id] + "\n---\n\n";
        }
        m_summary.failedSteps = failed;

        m_status.set_text(errors == 0
                              ? "Installation complete with no errors."
                              : "Installation complete with " + std::to_string(errors) +
                                    " error(s). See the output and the final step.");
        stateChanged();
    });
}

// Opening this step only shows what will run. The privileged helper, and the
// password prompt, start when the user explicitly chooses Start Installation.
void InstallPage::onShown()
{
    if (m_started)
        return;
    prepare();
}

void InstallPage::prepare()
{
    // The real plan is built on the worker thread once the helper's session
    // directory is known; this copy only fills the step list.
    const auto displayPlan = buildPlan(m_selection, m_system, "");
    m_totalSteps = static_cast<int>(displayPlan.size());
    m_progress.set_show_text(true);

    while (auto *child = m_stepList.get_first_child())
        m_stepList.remove(*child);
    m_chips.clear();
    m_descriptions.clear();
    for (const auto &step : displayPlan)
        addStepRow(step);

    m_status.set_text("Ready to install. Nothing has started yet.");
    m_startButton.set_visible(true);
    m_cancelButton.set_visible(false);
    stateChanged();
}

void InstallPage::start()
{
    if (m_started)
        return;
    if (!m_stepList.get_first_child())
        prepare();

    m_started = true;
    m_running = true;
    m_done = false;
    m_doneSteps = 0;
    m_progress.set_fraction(0.0);
    m_startButton.set_visible(false);
    m_cancelButton.set_visible(true);
    m_cancelButton.set_sensitive(true);
    m_status.set_text("Starting the privileged helper. Enter your password when asked.");
    stateChanged();

    m_session.start([this](const std::string &directory) {
        return buildPlan(m_selection, m_system, directory);
    });
}

void InstallPage::addStepRow(const InstallStep &step)
{
    auto *row = Gtk::make_managed<Gtk::ListBoxRow>();
    row->set_selectable(false);
    auto *line = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);

    auto *description = Gtk::make_managed<Gtk::Label>(step.description);
    description->set_xalign(0.0f);
    description->set_wrap(true);
    description->set_hexpand(true);
    line->append(*description);

    auto *chip = Gtk::make_managed<Gtk::Label>();
    chip->add_css_class("lo-chip");
    chip->add_css_class("neutral");
    chip->set_visible(false);
    chip->set_valign(Gtk::Align::START);
    line->append(*chip);

    row->set_child(*line);
    m_stepList.append(*row);
    m_chips[step.id] = chip;
    m_descriptions[step.id] = step.description;
}

void InstallPage::appendLog(const std::string &text)
{
    auto buffer = m_log.get_buffer();
    buffer->insert(buffer->end(), text + "\n");
    auto end = buffer->end();
    m_log.scroll_to(end);
}

void InstallPage::askCancel()
{
    auto dialog = Gtk::AlertDialog::create();
    dialog->set_message("Stop the installation?");
    dialog->set_detail("A package that is being installed right now may be left half-configured. "
                     "Anything already installed stays installed.");
    dialog->set_buttons({"Keep Going", "Stop"});
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
        m_status.set_text("Stopping…");
        m_session.cancel();
    });
}

} // namespace loadout
