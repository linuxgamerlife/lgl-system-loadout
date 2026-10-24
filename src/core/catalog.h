#pragma once

#include "core/selection.h"
#include "core/system.h"

#include <string>
#include <utility>
#include <vector>

namespace loadout::catalog {

// A selection key must also have another key selected.
struct Requirement final {
    std::string key;
    int minFedora{0};   // only applies from this Fedora release onwards
};

struct Item final {
    std::string key;
    std::string label;
    std::string description;
    int diskMb{0};
    bool showBadge{true};
    std::vector<Requirement> requirements;
};

// An install method that is not automated: shown alongside a section's items,
// with its command and a Copy button.
struct ManualOption final {
    std::string label;
    std::string description;
    std::string command;
    // Any-of detection commands; "{user}" is substituted. Any exit 0 means the
    // tool is installed.
    std::vector<std::vector<std::string>> detect;
    bool lglChoice{false};
    std::string source;
    // When set, a detection command only counts if its output contains this.
    std::string detectContains;

    ManualOption(std::string optionLabel, std::string optionDescription, std::string optionCommand,
                 std::vector<std::vector<std::string>> detectCommands = {},
                 const bool isChoice = false, std::string optionSource = {},
                 std::string contains = {})
        : label(std::move(optionLabel))
        , description(std::move(optionDescription))
        , command(std::move(optionCommand))
        , detect(std::move(detectCommands))
        , lglChoice(isChoice)
        , source(std::move(optionSource))
        , detectContains(std::move(contains))
    {
    }
};

struct Section final {
    std::string title;
    std::vector<Item> items;
    std::vector<ManualOption> manual;

    Section(std::string sectionTitle, std::vector<Item> sectionItems,
            std::vector<ManualOption> manualOptions = {})
        : title(std::move(sectionTitle))
        , items(std::move(sectionItems))
        , manual(std::move(manualOptions))
    {
    }
};

struct Page final {
    std::string id;
    std::string title;
    std::string subtitle;
    std::vector<Section> sections;
};

// An informational or warning box shown above a page's sections.
struct Note final {
    std::string title;
    std::string text;
    bool warning{false};

    Note(std::string noteTitle, std::string noteText, const bool isWarning = false)
        : title(std::move(noteTitle))
        , text(std::move(noteText))
        , warning(isWarning)
    {
    }
};

// Page-level material that is not a selectable item.
struct PageExtras final {
    std::vector<Note> notes;
};

const std::vector<Page> &pages();
const Page *page(const std::string &id);
const Item *item(const std::string &key);
std::vector<const Item *> allItems();

// A page-level single choice, such as the file manager or terminal.
struct ChoiceOption final {
    std::string value;        // stored in Selection::choice()
    std::string label;
    std::string description;
    std::string package;      // dnf package
    std::string glyph;        // symbolic icon for the generated tile
    int diskMb{0};
    bool lglChoice{false};
    std::string source{"Fedora"};
    std::string copr;         // COPR project to enable first, if any

    ChoiceOption(std::string optionValue, std::string optionLabel, std::string optionDescription,
                 std::string optionPackage, std::string optionGlyph, const int megabytes,
                 const bool isChoice = false, std::string optionSource = "Fedora",
                 std::string optionCopr = {})
        : value(std::move(optionValue))
        , label(std::move(optionLabel))
        , description(std::move(optionDescription))
        , package(std::move(optionPackage))
        , glyph(std::move(optionGlyph))
        , diskMb(megabytes)
        , lglChoice(isChoice)
        , source(std::move(optionSource))
        , copr(std::move(optionCopr))
    {
    }
};

struct ChoiceGroup final {
    std::string id;           // "file-manager", "terminal"
    std::string title;
    std::string description;
    std::vector<ChoiceOption> options;
};

const std::vector<ChoiceGroup> &choiceGroups();

PageExtras extrasFor(const std::string &pageId, const SystemInfo &system);

struct Unmet final {
    std::string itemKey;
    std::string requiredKey;
};

// Every requirement of a selected item that is not itself selected.
std::vector<Unmet> unmetRequirements(const Selection &selection, int fedoraVersion);

// Human label for a key, for warning text. Falls back to the key itself.
std::string labelFor(const std::string &key);

// Icon names to try, most specific first. An empty list means the card falls
// back to a monogram.
std::vector<std::string> iconNamesFor(const std::string &key);

// A bundled icon in the GResource for apps whose artwork is not in the icon
// theme, such as AppImages. Empty when there is none.
std::string iconResourceFor(const std::string &key);

// Where the package comes from, shown in brackets after the card's name:
// "Fedora", "RPM Fusion", "COPR", "Flatpak", "pipx", "AppImage".
std::string sourceFor(const std::string &key);

// Items LinuxGamerLife recommends, marked with the LGL'S CHOICE chip.
bool isLglChoice(const std::string &key);

} // namespace loadout::catalog
