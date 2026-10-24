#pragma once

#include "core/catalog.h"
#include "ui/checker.h"
#include "ui/page.h"

#include <gtkmm/box.h>
#include <gtkmm/checkbutton.h>
#include "ui/cardgrid.h"

#include <gtkmm/flowboxchild.h>
#include <gtkmm/label.h>

#include <functional>
#include <map>
#include <set>
#include <string>

namespace loadout {

class ItemCard;

// GPU manufacturer choice plus the AMD driver cards.
class GpuPage final : public Page {
public:
    GpuPage(Selection &selection, Checker &checker, const SystemInfo &system,
            std::function<void()> selectionChanged);

    void onShown() override;
    void syncFromSelection() override;
    void focusContent() override;

private:
    void buildManufacturerChoice();
    void buildAmd();
    void buildNvidia();
    void buildSkip();
    void updateVisibility();
    void updateWarnings();
    void requestMissingChecks();
    void toggle(const std::string &key);
    void activate(Gtk::FlowBoxChild &child);

    Selection &m_selection;
    Checker &m_checker;
    const SystemInfo &m_system;
    std::function<void()> m_selectionChanged;

    Gtk::CheckButton m_amd{"AMD"};
    Gtk::CheckButton m_nvidia{"NVIDIA"};
    Gtk::CheckButton m_skip{"Skip / Other"};

    Gtk::Box m_amdBox{Gtk::Orientation::VERTICAL, 12};
    Gtk::Box m_nvidiaBox{Gtk::Orientation::VERTICAL, 8};
    Gtk::Box m_skipBox{Gtk::Orientation::VERTICAL, 8};
    CardGrid m_grid;
    Gtk::Box m_warning{Gtk::Orientation::VERTICAL, 6};

    std::map<std::string, ItemCard *> m_cards;
    std::set<std::string> m_requested;
    bool m_syncing{false};
};

} // namespace loadout
