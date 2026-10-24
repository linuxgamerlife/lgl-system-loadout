#include "ui/helpwindow.h"

#include <giomm/resource.h>
#include <gtkmm/box.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>

#include <sstream>
#include <string>

namespace loadout {
namespace {

Glib::ustring escaped(const std::string &text)
{
    gchar *escapedText = g_markup_escape_text(text.c_str(), static_cast<gssize>(text.size()));
    Glib::ustring result = escapedText ? escapedText : "";
    g_free(escapedText);
    return result;
}

// Escapes the line and turns **emphasis** into bold, which is all HELP.md needs.
Glib::ustring markup(const std::string &text)
{
    const Glib::ustring safe = escaped(text);
    Glib::ustring result;
    bool bold = false;
    std::size_t position = 0;
    while (position <= safe.size()) {
        const auto marker = safe.find("**", position);
        const auto chunk = marker == Glib::ustring::npos
                               ? safe.substr(position)
                               : safe.substr(position, marker - position);
        if (bold) {
            result += "<b>";
            result += chunk;
            result += "</b>";
        } else {
            result += chunk;
        }
        if (marker == Glib::ustring::npos)
            break;
        bold = !bold;
        position = marker + 2;
    }
    return result;
}

Gtk::Label *makeLabel(const Glib::ustring &text, const char *cssClass)
{
    auto *label = Gtk::make_managed<Gtk::Label>();
    label->set_markup(text);
    label->set_xalign(0.0f);
    label->set_wrap(true);
    label->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
    label->add_css_class(cssClass);
    return label;
}

// The three kinds of pill a card can show, used as an inline example.
Gtk::Widget *pills()
{
    auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    row->set_margin_top(6);
    row->set_margin_bottom(6);

    for (const auto &[text, cssClass] :
         {std::pair{"FEDORA", "source"}, {"LGL'S CHOICE", "accent"}, {"INSTALLED", "positive"}}) {
        auto *chip = Gtk::make_managed<Gtk::Label>(text);
        chip->add_css_class("lo-chip");
        chip->add_css_class(cssClass);
        row->append(*chip);
    }
    return row;
}

} // namespace

HelpWindow::HelpWindow(Gtk::Window &parent)
{
    set_title("LGL System Loadout Help");
    set_transient_for(parent);
    set_default_size(700, 620);
    set_hide_on_close(true);

    auto *header = Gtk::make_managed<Gtk::HeaderBar>();
    auto *title = Gtk::make_managed<Gtk::Label>("HELP");
    title->add_css_class("wordmark");
    header->set_title_widget(*title);
    set_titlebar(*header);

    auto *content = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    content->set_margin(24);
    content->set_valign(Gtk::Align::START);

    try {
        const auto bytes = Gio::Resource::lookup_data_global("/com/linuxgamerlife/loadout/HELP.md");
        gsize size = 0;
        const auto *data = static_cast<const char *>(bytes->get_data(size));

        std::istringstream lines(std::string(data, size));
        std::string line;
        while (std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line.empty())
                continue;

            if (line.rfind("# ", 0) == 0) {
                content->append(*makeLabel(markup(line.substr(2)), "help-title"));
            } else if (line.rfind("## ", 0) == 0) {
                content->append(*makeLabel(markup(line.substr(3)), "help-heading"));
            } else if (line == "{{pills}}") {
                content->append(*pills());
            } else if (line.rfind("- ", 0) == 0) {
                auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
                auto *dot = Gtk::make_managed<Gtk::Label>("•");
                dot->set_valign(Gtk::Align::START);
                dot->add_css_class("help-body");
                row->append(*dot);
                auto *text = makeLabel(markup(line.substr(2)), "help-body");
                text->set_hexpand(true);
                row->append(*text);
                content->append(*row);
            } else {
                content->append(*makeLabel(markup(line), "help-body"));
            }
        }
    } catch (const Glib::Error &) {
        content->append(*makeLabel("The help file could not be opened.", "help-body"));
    }

    auto *scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroller->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scroller->set_child(*content);
    set_child(*scroller);
}

} // namespace loadout
