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
    g_assert_cmpuint(pages.size(), ==, 14);
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

void essentialsHasChoicesAndWifi()
{
    const auto *essentials = catalog::page("essentials");
    g_assert_nonnull(essentials);

    for (const char *key : {"choice:file-manager:dolphin", "choice:file-manager:nemo",
                            "choice:terminal:konsole", "choice:terminal:kitty",
                            "choice:terminal:foot", "choice:terminal:ghostty",
                            "essentials/wifi_nm", "essentials/wifi_wpa",
                            "essentials/wifi_firmware"})
        g_assert_nonnull(catalog::item(key));

    bool hasWifiNote = false;
    for (const auto &section : essentials->sections)
        hasWifiNote = hasWifiNote || !section.note.empty();
    g_assert_true(hasWifiNote);
}

void everyPageHasItems()
{
    for (const char *id : {"essentials", "repositories", "system-tools", "remote-access", "development", "multimedia", "content",
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
    g_test_add_func("/catalog/essentials", essentialsHasChoicesAndWifi);
    g_test_add_func("/catalog/pages", everyPageHasItems);
    g_test_add_func("/selection/empty", selectionStartsEmpty);
    g_test_add_func("/selection/keeps", selectionRemembersChoices);
    return g_test_run();
}
