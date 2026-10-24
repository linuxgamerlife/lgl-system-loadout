#pragma once

#include <gtkmm/window.h>

namespace loadout {

// HELP.md, rendered as rich widgets: headings, paragraphs, bullets and a row of
// example pills.
class HelpWindow final : public Gtk::Window {
public:
    explicit HelpWindow(Gtk::Window &parent);
};

} // namespace loadout
