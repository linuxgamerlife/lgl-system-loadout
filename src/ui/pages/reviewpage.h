#pragma once

#include "core/selection.h"
#include "core/system.h"
#include "ui/page.h"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/listbox.h>

namespace loadout {

class ReviewPage final : public Page {
public:
    ReviewPage(Selection &selection, const SystemInfo &system);
    void onShown() override;
    bool canAdvance() const override { return m_diskOk || m_proceeded; }

private:
    void rebuild();
    void buildRequirements();

    Selection &m_selection;
    const SystemInfo &m_system;
    Gtk::Box m_diskBanner{Gtk::Orientation::VERTICAL, 6};
    Gtk::Box m_requirements{Gtk::Orientation::VERTICAL, 6};
    Gtk::ListBox m_steps;
    Gtk::Button m_proceed{"Proceed Anyway (not recommended)"};
    bool m_diskOk{true};
    bool m_proceeded{false};
};

} // namespace loadout
