#include "ui/pages/welcomepage.h"

#include <gtkmm/label.h>

namespace loadout {
namespace {

void addLabel(Gtk::Box &box, const std::string &text, const char *cssClass, bool wrap = true)
{
    auto *label = Gtk::make_managed<Gtk::Label>(text);
    label->set_wrap(wrap);
    label->set_xalign(0.0f);
    if (cssClass)
        label->add_css_class(cssClass);
    box.append(*label);
}

} // namespace

WelcomePage::WelcomePage(Checker &checker, const SystemInfo &system)
    : m_checker(checker)
    , m_system(system)
{
    m_warning.add_css_class("disk-banner");
    m_warning.add_css_class("negative");
    m_warning.set_visible(false);
    addLabel(m_warning, "POLKIT HAS NOT LOADED THE HELPER ACTION", "section-label");
    addLabel(m_warning,
             "The privileged helper or its polkit policy is not available yet. This usually "
             "happens when the app is opened straight from the software centre right after "
             "installation, before polkit has reloaded. Close the app and open it again from "
             "your application menu.",
             nullptr);
    append(m_warning);

    auto *scope = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    scope->add_css_class("lo-card");
    addLabel(*scope, "INSTALLS ONLY", "section-label");
    addLabel(*scope,
             "This wizard installs. It never removes itself, and nothing it adds is uninstalled "
             "for you. A selected step may replace a conflicting package, and the review list "
             "shows every step before anything runs.",
             nullptr);
    append(*scope);

    buildSystemCard();
    buildExplanationCard();

    m_checker.signalResult().connect([this](const std::string &key, const bool available) {
        if (key != "polkit")
            return;
        m_warning.set_visible(!available);
    });
}

void WelcomePage::buildSystemCard()
{
    auto *card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    card->add_css_class("lo-card");
    addLabel(*card, "SYSTEM", "section-label");
    addLabel(*card, "Fedora " + m_system.fedoraVersion, "mono");
    addLabel(*card,
             "Target user: " + m_system.targetUser +
                 ". User-level tools are installed for this account.",
             "muted");
    append(*card);
}

void WelcomePage::buildExplanationCard()
{
    auto *card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    card->add_css_class("lo-card");
    addLabel(*card, "HOW THIS WORKS", "section-label");
    addLabel(*card,
             "Nothing is selected by default. Every item shows whether it is already on your "
             "system before you commit, and every step is listed before you start.",
             nullptr);
    addLabel(*card,
             "You can move between steps at any time before the installation starts, and your "
             "choices are kept. Once installation starts it runs to completion.",
             nullptr);
    addLabel(*card, "A working internet connection is required.", nullptr);
    addLabel(*card,
             "Each step starts with a list. Select an item to include it, and select it again "
             "to leave it out. If a selection needs something else, a warning appears with a "
             "button that selects it for you.",
             "muted");
    append(*card);
}

void WelcomePage::onShown()
{
    if (m_requested)
        return;
    m_requested = true;
    m_checker.requestCommand("polkit",
                             {"/usr/bin/pkaction", "--action-id",
                              "com.linuxgamerlife.lgl-system-loadout.run-helper"});
}

} // namespace loadout
