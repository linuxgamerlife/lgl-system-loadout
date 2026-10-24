#include "ui/pages/choicepage.h"

#include "ui/cardgrid.h"
#include "ui/itemcard.h"

#include <gtkmm/button.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/flowboxchild.h>
#include <gtkmm/label.h>

#include <algorithm>

namespace loadout {

ChoicePage::ChoicePage(Selection &selection, Checker &checker, const SystemInfo &system,
                       std::function<void()> selectionChanged)
    : m_selection(selection)
    , m_checker(checker)
    , m_system(system)
    , m_selectionChanged(std::move(selectionChanged))
{
    auto *toolbar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    toolbar->set_halign(Gtk::Align::END);

    auto *refresh = Gtk::make_managed<Gtk::Button>("Refresh");
    refresh->add_css_class("lo-small");
    refresh->set_tooltip_text("Re-check installed state and reload icons");
    refresh->signal_clicked().connect([this] { refreshChecks(); });
    toolbar->append(*refresh);

    auto *all = Gtk::make_managed<Gtk::Button>("Select All");
    all->add_css_class("lo-small");
    all->set_tooltip_text("Choose the recommended option in each group");
    all->signal_clicked().connect([this] { selectAll(); });
    toolbar->append(*all);

    auto *none = Gtk::make_managed<Gtk::Button>("Select None");
    none->add_css_class("lo-small");
    none->signal_clicked().connect([this] { selectNone(); });
    toolbar->append(*none);
    append(*toolbar);

    for (const auto &group : catalog::choiceGroups())
        addGroup(group);

    m_checker.signalResult().connect([this](const std::string &key, const bool installed) {
        const auto found = m_byCheckKey.find(key);
        if (found != m_byCheckKey.end())
            found->second->setInstalled(installed);
    });

    syncFromSelection();
}

void ChoicePage::addGroup(const catalog::ChoiceGroup &group)
{
    auto *heading = Gtk::make_managed<Gtk::Label>(group.title);
    heading->set_xalign(0.0f);
    heading->add_css_class("section-title");
    heading->set_margin_top(8);
    append(*heading);

    auto *description = Gtk::make_managed<Gtk::Label>(group.description);
    description->set_xalign(0.0f);
    description->set_wrap(true);
    description->add_css_class("muted");
    description->add_css_class("small");
    append(*description);

    auto *grid = Gtk::make_managed<CardGrid>();
    grid->signalLeaveLeft().connect([this] { signalFocusSteps().emit(); });
    registerGrid(*grid);

    std::vector<Entry> entries;
    for (const auto &option : group.options) {
        const std::string syntheticKey = "choice:" + group.id + ":" + option.value;
        const catalog::Item synthetic{syntheticKey, option.label, option.description,
                                      option.diskMb, true, {}};

        auto *card = Gtk::make_managed<ItemCard>();
        card->configure(synthetic, option.source, option.glyph, option.lglChoice);
        grid->addCard(*card);
        entries.push_back({option.value, card});

        const std::string checkKey = "choice/" + group.id + "/" + option.value;
        m_byCheckKey[checkKey] = card;
    }

    const std::string groupId = group.id;
    grid->setActivated([this, groupId, entries](Gtk::FlowBoxChild &child) {
        Gtk::Widget *widget = child.get_child();
        for (const auto &entry : entries)
            if (static_cast<Gtk::Widget *>(entry.card) == widget) {
                choose(groupId, entry.value);
                return;
            }
    });

    if (!m_firstGrid)
        m_firstGrid = grid;

    m_groups[group.id] = std::move(entries);
    append(*grid);
}

void ChoicePage::choose(const std::string &group, const std::string &value)
{
    m_selection.setChoice(group, m_selection.choice(group) == value ? "none" : value);
    syncFromSelection();
    m_selectionChanged();
    stateChanged();
}

void ChoicePage::syncFromSelection()
{
    for (const auto &[groupId, entries] : m_groups) {
        const auto &chosen = m_selection.choice(groupId);
        for (const auto &entry : entries) {
            if (entry.value == chosen)
                entry.card->add_css_class("chosen");
            else
                entry.card->remove_css_class("chosen");
        }
    }
}

void ChoicePage::onShown()
{
    requestMissingChecks();
}

void ChoicePage::focusContent()
{
    if (m_firstGrid) {
        m_firstGrid->focusFirst();
        return;
    }
    Page::focusContent();
}

void ChoicePage::refreshChecks()
{
    // Without clearing this, every key counts as already requested and Refresh
    // would do nothing.
    m_requested.clear();
    for (const auto &[key, card] : m_byCheckKey)
        card->refreshIcon();
    requestMissingChecks();
}

void ChoicePage::selectAll()
{
    // These groups are single-choice, so "all" means one recommended option in
    // each group. An existing choice is kept.
    for (const auto &group : catalog::choiceGroups()) {
        if (group.options.empty() || m_selection.choice(group.id) != "none")
            continue;
        const auto recommended = std::find_if(group.options.begin(), group.options.end(),
                                              [](const catalog::ChoiceOption &option) {
                                                  return option.lglChoice;
                                              });
        const auto &option =
            recommended != group.options.end() ? *recommended : group.options.front();
        m_selection.setChoice(group.id, option.value);
    }
    syncFromSelection();
    m_selectionChanged();
    stateChanged();
}

void ChoicePage::selectNone()
{
    for (const auto &group : catalog::choiceGroups())
        m_selection.setChoice(group.id, "none");
    syncFromSelection();
    m_selectionChanged();
    stateChanged();
}

void ChoicePage::requestMissingChecks()
{
    for (const auto &group : catalog::choiceGroups()) {
        for (const auto &option : group.options) {
            const std::string checkKey = "choice/" + group.id + "/" + option.value;
            if (m_requested.count(checkKey))
                continue;
            m_requested.insert(checkKey);
            m_checker.requestCommand(
                checkKey, {"/usr/bin/rpm", "-q", "--quiet", option.package});
        }
    }
}

} // namespace loadout
