#pragma once

#include "ui/checker.h"
#include "ui/page.h"

#include <gtkmm/box.h>

namespace loadout {

class WelcomePage final : public Page {
public:
    WelcomePage(Checker &checker, const SystemInfo &system);
    void onShown() override;

private:
    void buildSystemCard();
    void buildExplanationCard();

    Checker &m_checker;
    const SystemInfo &m_system;
    Gtk::Box m_warning{Gtk::Orientation::VERTICAL, 6};
    bool m_requested{false};
};

} // namespace loadout
