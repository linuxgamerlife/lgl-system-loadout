#pragma once

#include "core/catalog.h"
#include "ui/checker.h"
#include "ui/page.h"

#include <gtkmm/box.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/label.h>
#include <gtkmm/spinner.h>

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace loadout {

class ItemCard;
class CardGrid;

// The generic catalogue page: notes, a responsive grid of package cards with
// installed state, Select all/None, Refresh, and a banner for unmet
// requirements. Sections may also carry manual install methods, shown as cards
// with their command.
class SelectPage final : public Page {
public:
    SelectPage(const catalog::Page &definition, Selection &selection, Checker &checker,
               const SystemInfo &system, std::function<void()> selectionChanged, Notify notify);

    void onShown() override;
    void syncFromSelection() override;
    void focusContent() override;

private:
    void addExtra(const catalog::PageExtras &extras);
    void addNote(const catalog::Note &note);
    void addSection(const catalog::Section &section);
    void addCard(const catalog::Item &item, CardGrid &grid);
    void addManual(const catalog::ManualOption &option, CardGrid &grid);
    void activate(Gtk::FlowBoxChild &child);
    void toggle(const std::string &key);
    void setAll(bool selected);
    void updateWarnings();
    void requestMissingChecks();

    const catalog::Page &m_definition;
    Selection &m_selection;
    Checker &m_checker;
    const SystemInfo &m_system;
    std::function<void()> m_selectionChanged;
    Notify m_notify;

    Gtk::Box m_warning{Gtk::Orientation::VERTICAL, 6};
    Gtk::Spinner m_spinner;
    Gtk::Label m_checking;

    std::map<std::string, ItemCard *> m_cards;
    CardGrid *m_firstGrid{nullptr};
    std::set<std::string> m_requested;

    std::map<std::string, ItemCard *> m_manualCards;
    std::map<std::string, std::vector<std::vector<std::string>>> m_manualDetect;
    std::map<std::string, std::string> m_manualContains;
    std::set<std::string> m_manualRequested;
};

} // namespace loadout
