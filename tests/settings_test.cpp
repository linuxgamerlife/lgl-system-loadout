#include "core/settings.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <filesystem>
#include <string>
#include <system_error>

namespace {

class TemporaryFolder final {
public:
    TemporaryFolder()
    {
        GError *error = nullptr;
        gchar *created = g_dir_make_tmp("loadout-settings-test-XXXXXX", &error);
        g_assert_no_error(error);
        g_assert_nonnull(created);
        m_path = created;
        g_free(created);
    }
    ~TemporaryFolder()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }
    TemporaryFolder(const TemporaryFolder &) = delete;
    TemporaryFolder &operator=(const TemporaryFolder &) = delete;

    std::string file(const std::string &name) const { return m_path + "/" + name; }
    const std::string &path() const { return m_path; }

private:
    std::string m_path;
};

void defaultsToAppTitleBar()
{
    TemporaryFolder folder;
    const loadout::Settings settings(folder.file("settings.ini"));
    g_assert_false(settings.systemTitleBar());
}

void persistsSystemTitleBar()
{
    TemporaryFolder folder;
    const std::string path = folder.file("settings.ini");
    {
        loadout::Settings settings(path);
        settings.setSystemTitleBar(true);
    }
    g_assert_true(loadout::Settings(path).systemTitleBar());
}

void ignoresMalformedFile()
{
    TemporaryFolder folder;
    const std::string path = folder.file("settings.ini");
    g_assert_true(g_file_set_contents(path.c_str(), "[window\nsystemTitleBar=nope\n", -1, nullptr));
    const loadout::Settings settings(path);
    g_assert_false(settings.systemTitleBar());
}

void createsConfigFolderWithOwnerOnlyMode()
{
    TemporaryFolder folder;
    const std::string nested = folder.path() + "/nested";
    loadout::Settings settings(nested + "/settings.ini");
    settings.setSystemTitleBar(true);

    GStatBuf status{};
    g_assert_cmpint(g_stat(nested.c_str(), &status), ==, 0);
    g_assert_cmpuint(status.st_mode & 0777, ==, 0700);
}

} // namespace

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/settings/defaults", defaultsToAppTitleBar);
    g_test_add_func("/settings/system-title-bar", persistsSystemTitleBar);
    g_test_add_func("/settings/malformed", ignoresMalformedFile);
    g_test_add_func("/settings/folder-mode", createsConfigFolderWithOwnerOnlyMode);
    return g_test_run();
}
