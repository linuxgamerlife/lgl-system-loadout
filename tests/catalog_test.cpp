#include "core/catalog.h"
#include "core/selection.h"

#include <glib.h>

#include <set>
#include <string>

using namespace loadout;

namespace {

void hasUniqueKeysAndPages()
{
    std::set<std::string> keys;
    std::set<std::string> pages;
    for (const auto &page : catalog::pages()) {
        g_assert_true(pages.insert(page.id).second);
        g_assert_false(page.title.empty());
        g_assert_false(page.sections.empty());
        for (const auto &section : page.sections) {
            g_assert_false(section.title.empty());
            g_assert_true(!section.items.empty() || !section.manual.empty());
            for (const auto &item : section.items) {
                g_assert_true(keys.insert(item.key).second);
                g_assert_false(item.label.empty());
                g_assert_false(item.description.empty());
                g_assert_cmpint(item.diskMb, >=, 0);
            }
        }
    }
    g_assert_cmpuint(pages.size(), ==, 13);
}

void requirementsReferenceRealItems()
{
    for (const auto *item : catalog::allItems())
        for (const auto &requirement : item->requirements)
            g_assert_nonnull(catalog::item(requirement.key));
}

void pageLookupWorks()
{
    g_assert_nonnull(catalog::page("gaming"));
    g_assert_null(catalog::page("nope"));
    g_assert_nonnull(catalog::item("gaming/steam"));
    g_assert_null(catalog::item("gaming/nope"));
    g_assert_true(catalog::labelFor("gaming/steam") == "Steam");
    g_assert_true(catalog::labelFor("unknown/key") == "unknown/key");
}

void reportsChainedRequirements()
{
    Selection selection;
    selection.set("gaming/protontricks", true);

    const auto unmet = catalog::unmetRequirements(selection, 44);
    g_assert_cmpuint(unmet.size(), ==, 1);
    g_assert_true(unmet.front().requiredKey == "gaming/steam");
}

void choiceGroupsAreComplete()
{
    const auto &groups = catalog::choiceGroups();
    g_assert_cmpuint(groups.size(), ==, 2);

    for (const auto &group : groups) {
        g_assert_false(group.id.empty());
        g_assert_false(group.title.empty());
        g_assert_cmpuint(group.options.size(), >=, 2);

        std::set<std::string> values;
        for (const auto &option : group.options) {
            g_assert_true(values.insert(option.value).second);
            g_assert_false(option.label.empty());
            g_assert_false(option.package.empty());
            g_assert_false(option.glyph.empty());
        }
    }
}

void everyPageHasItems()
{
    for (const char *id : {"repositories", "system-tools", "remote-access", "development", "multimedia", "content",
                           "graphics", "gaming", "virtualisation", "browsers", "communication",
                           "kernel", "toolkit"}) {
        const auto *page = catalog::page(id);
        g_assert_nonnull(page);
        bool hasItems = false;
        for (const auto &section : page->sections)
            hasItems = hasItems || !section.items.empty();
        g_assert_true(hasItems);
    }
}

void selectionStartsEmpty()
{
    Selection selection;
    g_assert_true(selection.empty());
    g_assert_false(selection.get("gaming/steam"));
    g_assert_true(selection.choice("gpu") == "none");
    g_assert_cmpuint(selection.selectedKeys().size(), ==, 0);
}

void selectionRemembersChoices()
{
    Selection selection;
    selection.set("gaming/steam", true);
    selection.set("browsers/firefox", true);
    selection.set("gaming/steam", false);

    g_assert_false(selection.empty());
    g_assert_false(selection.get("gaming/steam"));
    g_assert_true(selection.get("browsers/firefox"));
    g_assert_cmpuint(selection.selectedKeys().size(), ==, 1);
    g_assert_true(selection.selectedKeys().front() == "browsers/firefox");

    selection.setChoice("gpu", "amd");
    g_assert_true(selection.choice("gpu") == "amd");
}

} // namespace

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/catalog/unique", hasUniqueKeysAndPages);
    g_test_add_func("/catalog/requirements", requirementsReferenceRealItems);
    g_test_add_func("/catalog/lookup", pageLookupWorks);
    g_test_add_func("/catalog/chained", reportsChainedRequirements);
    g_test_add_func("/catalog/choices", choiceGroupsAreComplete);
    g_test_add_func("/catalog/pages", everyPageHasItems);
    g_test_add_func("/selection/empty", selectionStartsEmpty);
    g_test_add_func("/selection/keeps", selectionRemembersChoices);
    return g_test_run();
}
