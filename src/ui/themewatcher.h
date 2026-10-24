#pragma once

#include "core/theme.h"

#include <giomm/filemonitor.h>
#include <giomm/settings.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/window.h>
#include <sigc++/sigc++.h>

#include <map>
#include <string>
#include <vector>

namespace loadout {

// Re-themes the app live when the desktop theme changes.
//
// libadwaita provides the base stylesheet and follows the desktop's dark mode
// and accent colour. This watcher adds what libadwaita does not do: it re-reads
// ~/.config/gtk-4.0/gtk.css when it or one of its @imports changes (GTK loads
// that file only once), applies the desktop monospace font to every mono class,
// and sets the window's .dark class from the effective background. Changes are
// debounced by 200 ms.
class ThemeWatcher final : public sigc::trackable {
public:
    explicit ThemeWatcher(Gtk::Window &window);
    ~ThemeWatcher();
    ThemeWatcher(const ThemeWatcher &) = delete;
    ThemeWatcher &operator=(const ThemeWatcher &) = delete;

    const theme::Theme &current() const { return m_theme; }
    std::string monoFamily() const;

    // Emitted after a change has been applied.
    sigc::signal<void()> &signalChanged() { return m_changed; }

    // Schedules a re-read of the theme (debounced).
    void queueChange() { queue(); }

private:
    void watch();
    void watchFiles();
    void queue();
    bool changed();
    void followDesktopWithoutPortal();
    void apply(bool reloadUserCss);
    theme::Theme read() const;
    std::string interfaceString(const char *key) const;

    Gtk::Window &m_window;
    theme::Theme m_theme;
    Glib::RefPtr<Gtk::CssProvider> m_userProvider;
    Glib::RefPtr<Gtk::CssProvider> m_fontProvider;
    Glib::RefPtr<Gio::Settings> m_interface;
    Glib::RefPtr<Gtk::Settings> m_gtkSettings;
    std::map<std::string, Glib::RefPtr<Gio::FileMonitor>> m_monitors;
    sigc::connection m_timeout;
    std::vector<gulong> m_styleHandlers;
    sigc::signal<void()> m_changed;
};

// Initialises libadwaita and adds data/style.css at APPLICATION priority for the
// default display. Call once from the application's startup handler.
void installApplicationStylesheet();

// Re-parses the application stylesheet so its named colours follow the current
// desktop theme and accent colour.
void reloadApplicationStylesheet();

} // namespace loadout
