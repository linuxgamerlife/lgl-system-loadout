#pragma once

#include "core/catalog.h"
#include "ui/checker.h"
#include "ui/page.h"

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace loadout {

class ItemCard;
class CardGrid;

// Single-choice groups from the catalogue, such as the file manager and the
// terminal. Each group installs at most one package.
class ChoicePage final : public Page {
public:
    ChoicePage(Selection &selection, Checker &checker, const SystemInfo &system,
               std::function<void()> selectionChanged);

    void onShown() override;
    void syncFromSelection() override;
    void focusContent() override;

private:
    void addGroup(const catalog::ChoiceGroup &group);
    void choose(const std::string &group, const std::string &value);
    void requestMissingChecks();
    void refreshChecks();
    void selectAll();
    void selectNone();

    Selection &m_selection;
    Checker &m_checker;
    const SystemInfo &m_system;
    std::function<void()> m_selectionChanged;

    struct Entry final {
        std::string value;
        ItemCard *card{nullptr};
    };

    std::map<std::string, std::vector<Entry>> m_groups;
    std::map<std::string, ItemCard *> m_byCheckKey;
    CardGrid *m_firstGrid{nullptr};
    std::set<std::string> m_requested;
};

} // namespace loadout
