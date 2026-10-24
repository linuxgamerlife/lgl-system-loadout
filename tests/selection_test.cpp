#include "core/selection.h"

#include <glib.h>

using namespace loadout;

namespace {

void defaultsAreOff()
{
    const Selection selection;
    g_assert_false(selection.get("anything"));
    g_assert_true(selection.empty());
    g_assert_true(selection.choice("gpu") == "none");
}

void setAndClear()
{
    Selection selection;
    selection.set("a", true);
    selection.set("b", true);
    g_assert_cmpuint(selection.selectedKeys().size(), ==, 2);

    selection.set("a", false);
    g_assert_false(selection.get("a"));
    g_assert_cmpuint(selection.selectedKeys().size(), ==, 1);
}

void choiceChanges()
{
    Selection selection;
    selection.setChoice("gpu", "nvidia");
    g_assert_true(selection.choice("gpu") == "nvidia");
    g_assert_true(selection.choice("terminal") == "none");
}

} // namespace

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/selection/defaults", defaultsAreOff);
    g_test_add_func("/selection/set", setAndClear);
    g_test_add_func("/selection/choice", choiceChanges);
    return g_test_run();
}
