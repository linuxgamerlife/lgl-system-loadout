#include "core/theme.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace loadout;

namespace {

class TemporaryConfig final {
public:
    TemporaryConfig()
    {
        gchar *created = g_dir_make_tmp("loadout-theme-test-XXXXXX", nullptr);
        g_assert_nonnull(created);
        m_path = created;
        g_free(created);
        std::filesystem::create_directories(m_path + "/gtk-4.0");
    }
    ~TemporaryConfig()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }
    TemporaryConfig(const TemporaryConfig &) = delete;
    TemporaryConfig &operator=(const TemporaryConfig &) = delete;

    void write(const std::string &name, const std::string &text) const
    {
        std::ofstream(m_path + "/gtk-4.0/" + name) << text;
    }

    const std::string &path() const { return m_path; }

private:
    std::string m_path;
};

void followsImports()
{
    TemporaryConfig config;
    config.write("gtk.css", "@import url('palette.css');\n@import url('missing.css');\n");
    config.write("palette.css",
                 "@import 'gtk.css';\n@define-color window_bg_color #000000;\n");
    const auto paths = theme::gtkCssPaths(config.path());
    g_assert_cmpuint(paths.size(), ==, 2);
}

void resolvesNamedColors()
{
    TemporaryConfig config;
    config.write("gtk.css",
                 "@define-color accent_bg_color #3584e4;\n"
                 "@define-color theme_selected_bg_color @accent_bg_color;\n"
                 "@define-color window_bg_color rgb(30, 30, 30);\n");
    const auto colors = theme::gtkColors(theme::gtkCssPaths(config.path()));
    g_assert_true(colors.at("theme_selected_bg_color") == "#3584e4");
    g_assert_true(theme::isDark(colors, false));
}

void breaksAliasCycles()
{
    TemporaryConfig config;
    config.write("gtk.css",
                 "@define-color a @b;\n@define-color b @a;\n@define-color window_bg_color #ffffff;\n");
    const auto colors = theme::gtkColors(theme::gtkCssPaths(config.path()));
    g_assert_cmpuint(colors.count("a"), ==, 0);
    g_assert_false(theme::isDark(colors, false));
}

void extractsFontFamilies()
{
    g_assert_true(theme::fontFamily("Adwaita Sans Bold 11") == "Adwaita Sans");
    g_assert_true(theme::fontFamily("JetBrains Mono 10.5") == "JetBrains Mono");
    g_assert_true(theme::fontFamily("") == "");
}

void buildsFontStylesheetForEveryMonoClass()
{
    theme::Theme current;
    current.monoFont = "Fira Code";
    const auto css = theme::fontStylesheet(current);
    g_assert_nonnull(strstr(css.c_str(), "Fira Code"));
    for (const char *name : {".mono", ".wordmark", ".section-label", ".lo-chip", ".install-log"})
        g_assert_nonnull(strstr(css.c_str(), name));
}

void measuresLuminance()
{
    g_assert_true(theme::luminance("#000000").value() < 0.01);
    g_assert_true(theme::luminance("#ffffff").value() > 0.99);
    g_assert_true(theme::luminance("#fff").has_value());
    g_assert_false(theme::luminance("not-a-colour").has_value());
}

} // namespace

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/theme/follows-imports", followsImports);
    g_test_add_func("/theme/resolves-named-colors", resolvesNamedColors);
    g_test_add_func("/theme/breaks-alias-cycles", breaksAliasCycles);
    g_test_add_func("/theme/extracts-font-families", extractsFontFamilies);
    g_test_add_func("/theme/builds-font-stylesheet", buildsFontStylesheetForEveryMonoClass);
    g_test_add_func("/theme/luminance", measuresLuminance);
    return g_test_run();
}
