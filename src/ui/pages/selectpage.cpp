#include "ui/pages/selectpage.h"

#include "ui/cardgrid.h"
#include "ui/itemcard.h"

#include <gtkmm/button.h>
#include <gtkmm/flowboxchild.h>

#include <exception>

namespace loadout {
namespace {

std::string replaceAll(std::string value, const std::string &token, const std::string &with)
{
    std::size_t position = 0;
    while ((position = value.find(token, position)) != std::string::npos) {
        value.replace(position, token.size(), with);
        position += with.size();
    }
    return value;
}

std::string withTokens(std::string value, const std::string &user, const std::string &home)
{
    value = replaceAll(std::move(value), "{user}", user);
    return replaceAll(std::move(value), "{home}", home);
}

} // namespace

SelectPage::SelectPage(const catalog::Page &definition, Selection &selection, Checker &checker,
                       const SystemInfo &system, std::function<void()> selectionChanged,
                       Notify notify)
    : m_definition(definition)
    , m_selection(selection)
    , m_checker(checker)
    , m_system(system)
    , m_selectionChanged(std::move(selectionChanged))
    , m_notify(std::move(notify))
{
    auto *toolbar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    toolbar->set_halign(Gtk::Align::END);

    m_spinner.set_visible(false);
    toolbar->append(m_spinner);
    m_checking.set_text("Checking…");
    m_checking.add_css_class("muted");
    m_checking.add_css_class("small");
    m_checking.set_visible(false);
    toolbar->append(m_checking);

    auto *refresh = Gtk::make_managed<Gtk::Button>("Refresh");
    refresh->add_css_class("lo-small");
    refresh->set_tooltip_text("Re-check installed state and reload icons");
    refresh->signal_clicked().connect([this] {
        // Without clearing these, every key is already "requested" and Refresh
        // would do nothing.
        m_requested.clear();
        m_manualRequested.clear();
        for (const auto &[key, card] : m_cards)
            card->refreshIcon();
        requestMissingChecks();
    });
    toolbar->append(*refresh);

    auto *all = Gtk::make_managed<Gtk::Button>("Select All");
    all->add_css_class("lo-small");
    all->signal_clicked().connect([this] { setAll(true); });
    toolbar->append(*all);

    auto *none = Gtk::make_managed<Gtk::Button>("Select None");
    none->add_css_class("lo-small");
    none->signal_clicked().connect([this] { setAll(false); });
    toolbar->append(*none);
    append(*toolbar);

    addExtra(catalog::extrasFor(definition.id, system));

    m_warning.add_css_class("disk-banner");
    m_warning.add_css_class("neutral");
    m_warning.set_visible(false);
    append(m_warning);

    for (const auto &section : m_definition.sections)
        addSection(section);

    m_checker.signalResult().connect([this](const std::string &key, const bool installed) {
        if (const auto manual = m_manualCards.find(key); manual != m_manualCards.end()) {
            manual->second->setInstalled(installed);
            return;
        }
        const auto found = m_cards.find(key);
        if (found != m_cards.end())
            found->second->setInstalled(installed);
    });
    m_checker.signalIdle().connect([this] {
        m_spinner.set_visible(false);
        m_checking.set_visible(false);
    });

    syncFromSelection();
}

void SelectPage::addExtra(const catalog::PageExtras &extras)
{
    for (const auto &note : extras.notes)
        addNote(note);
}

void SelectPage::addNote(const catalog::Note &note)
{
    auto *card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    if (note.warning) {
        card->add_css_class("disk-banner");
        card->add_css_class("negative");
    } else {
        card->add_css_class("lo-card");
    }

    auto *title = Gtk::make_managed<Gtk::Label>(note.title);
    title->set_xalign(0.0f);
    title->add_css_class("section-label");
    card->append(*title);

    auto *text = Gtk::make_managed<Gtk::Label>(note.text);
    text->set_wrap(true);
    text->set_xalign(0.0f);
    card->append(*text);

    append(*card);
}

void SelectPage::addSection(const catalog::Section &section)
{
    if (!section.title.empty()) {
        auto *heading = Gtk::make_managed<Gtk::Label>(section.title);
        heading->set_xalign(0.0f);
        heading->add_css_class("section-title");
        heading->set_margin_top(8);
        append(*heading);
    }

    auto *grid = Gtk::make_managed<CardGrid>();
    grid->setActivated([this](Gtk::FlowBoxChild &child) { activate(child); });
    grid->signalLeaveLeft().connect([this] { signalFocusSteps().emit(); });
    registerGrid(*grid);

    for (const auto &item : section.items)
        addCard(item, *grid);
    for (const auto &option : section.manual)
        addManual(option, *grid);

    if (!m_firstGrid)
        m_firstGrid = grid;

    append(*grid);
}

void SelectPage::addCard(const catalog::Item &item, CardGrid &grid)
{
    if (item.key == "gaming/epic" && !m_system.architecture.empty() &&
        m_system.architecture != "x86_64")
        return;

    auto *card = Gtk::make_managed<ItemCard>();
    card->configure(item, catalog::sourceFor(item.key), {}, catalog::isLglChoice(item.key));
    grid.addCard(*card);
    m_cards[item.key] = card;
}

void SelectPage::activate(Gtk::FlowBoxChild &child)
{
    Gtk::Widget *widget = child.get_child();
    for (const auto &[key, card] : m_cards)
        if (static_cast<Gtk::Widget *>(card) == widget) {
            toggle(key);
            return;
        }
}

void SelectPage::focusContent()
{
    if (m_firstGrid) {
        m_firstGrid->focusFirst();
        return;
    }
    Page::focusContent();
}

void SelectPage::addManual(const catalog::ManualOption &option, CardGrid &grid)
{
    const std::string key = "manual:" + option.label;
    catalog::Item synthetic{key, option.label, option.description, 0, false, {}};
    // Manual installs with a detection command get the usual installed badge.
    synthetic.showBadge = !option.detect.empty();

    auto *card = Gtk::make_managed<ItemCard>();
    card->configure(synthetic, option.source, {}, option.lglChoice);
    if (!option.command.empty())
        card->setCommand(option.command, m_notify);
    grid.addCard(*card);

    if (option.detect.empty())
        return;

    std::vector<std::vector<std::string>> detect;
    detect.reserve(option.detect.size());
    for (const auto &command : option.detect) {
        std::vector<std::string> arguments;
        arguments.reserve(command.size());
        for (const auto &argument : command)
            arguments.push_back(withTokens(
                argument, m_system.targetUser,
                m_system.targetHome.empty() ? "/home/" + m_system.targetUser
                                            : m_system.targetHome));
        detect.push_back(std::move(arguments));
    }

    const std::string detectKey = "extra/" + m_definition.id + "/" + key;
    m_manualCards[detectKey] = card;
    m_manualDetect[detectKey] = std::move(detect);
    m_manualContains[detectKey] = option.detectContains;
}

void SelectPage::toggle(const std::string &key)
{
    m_selection.set(key, !m_selection.get(key));
    syncFromSelection();
    m_selectionChanged();
    stateChanged();
}

void SelectPage::setAll(const bool selected)
{
    for (const auto &[key, card] : m_cards)
        m_selection.set(key, selected);
    syncFromSelection();
    m_selectionChanged();
    stateChanged();
}

void SelectPage::syncFromSelection()
{
    for (const auto &[key, card] : m_cards) {
        if (m_selection.get(key))
            card->add_css_class("chosen");
        else
            card->remove_css_class("chosen");
    }
    updateWarnings();
}

void SelectPage::onShown()
{
    requestMissingChecks();
}

void SelectPage::requestMissingChecks()
{
    bool requested = false;
    for (const auto &section : m_definition.sections) {
        for (const auto &entry : section.items) {
            if (!entry.showBadge || m_requested.count(entry.key))
                continue;
            const auto check = checkFor(entry.key, m_system);
            if (check.any.empty())
                continue;
            m_requested.insert(entry.key);
            m_checker.request(entry.key, check);
            requested = true;
        }
    }

    for (const auto &[key, commands] : m_manualDetect) {
        if (m_manualRequested.count(key))
            continue;
        m_manualRequested.insert(key);
        Check check;
        check.any = commands;
        check.outputContains = m_manualContains[key];
        m_checker.request(key, check);
    }

    if (requested) {
        m_spinner.set_visible(true);
        m_spinner.start();
        m_checking.set_visible(true);
    }
}

void SelectPage::updateWarnings()
{
    while (auto *child = m_warning.get_first_child())
        m_warning.remove(*child);

    int version = 44;
    try {
        version = std::stoi(m_system.fedoraVersion);
    } catch (const std::exception &) {
    }

    std::vector<catalog::Unmet> unmet;
    for (const auto &entry : catalog::unmetRequirements(m_selection, version))
        if (m_cards.count(entry.itemKey))
            unmet.push_back(entry);

    if (unmet.empty()) {
        m_warning.set_visible(false);
        return;
    }

    auto *heading = Gtk::make_managed<Gtk::Label>("REQUIREMENTS");
    heading->set_xalign(0.0f);
    heading->add_css_class("section-label");
    m_warning.append(*heading);

    for (const auto &entry : unmet) {
        auto *line = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
        auto *text = Gtk::make_managed<Gtk::Label>(
            catalog::labelFor(entry.itemKey) + " needs " + catalog::labelFor(entry.requiredKey) +
            ", which is not selected.");
        text->set_wrap(true);
        text->set_xalign(0.0f);
        text->set_hexpand(true);
        line->append(*text);

        auto *fix = Gtk::make_managed<Gtk::Button>(
            "Select " + catalog::labelFor(entry.requiredKey));
        fix->add_css_class("lo-small");
        const std::string requiredKey = entry.requiredKey;
        fix->signal_clicked().connect([this, requiredKey] {
            m_selection.set(requiredKey, true);
            m_selectionChanged();
            syncFromSelection();
            stateChanged();
        });
        line->append(*fix);
        m_warning.append(*line);
    }

    m_warning.set_visible(true);
}

} // namespace loadout
