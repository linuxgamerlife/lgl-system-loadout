#pragma once

#include "core/selection.h"
#include "core/settings.h"
#include "core/system.h"
#include "ui/checker.h"

#include <gtkmm/aboutdialog.h>
#include <gtkmm/applicationwindow.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/revealer.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/stack.h>
#include <sigc++/sigc++.h>

#include <memory>
#include <string>
#include <vector>

namespace loadout {

class Page;
class InstallPage;
class DonePage;
class ThemeWatcher;
class HelpWindow;

enum class NoticeKind { Info, Positive, Negative };

struct Step final {
    const char *id;
    const char *title;
    const char *subtitle;
};

// The main window: the step list beside the current page, a menu in the header
// bar, and both title bar modes.
class Window final : public Gtk::ApplicationWindow {
public:
    explicit Window(const Glib::RefPtr<Gtk::Application> &application);
    ~Window() override;

    void notify(const std::string &text, NoticeKind kind, unsigned int timeoutMs = 5000);

private:
    void buildHeader();
    void buildBody();
    void buildPages();
    void buildActions(const Glib::RefPtr<Gtk::Application> &application);

    void showStep(int index);
    void updateNavigation();
    void refreshStepVisibility();
    void syncStepSelection();
    void focusPrimaryInput();
    void focusContent();
    void refreshSelections();
    void refreshContinueButton();

    void applyTitleBar(bool system);
    void showHelp();
    void showAbout();
    void watchAltTap(Gtk::Widget &widget);
    void watchStepsExit();

    Settings m_settings;
    Selection m_selection;
    SystemInfo m_system;
    Checker m_checker;

    Gtk::HeaderBar m_header;
    Gtk::Box m_content{Gtk::Orientation::VERTICAL};
    Gtk::Stack m_stack;
    Gtk::ScrolledWindow *m_contentScroller{nullptr};
    Gtk::ListBox m_steps;
    Gtk::MenuButton *m_menuButton{nullptr};
    Gtk::Box *m_continueBar{nullptr};
    Gtk::Button *m_continueButton{nullptr};

    Gtk::Revealer m_noticeRevealer;
    Gtk::Box m_noticeBox{Gtk::Orientation::HORIZONTAL};
    Gtk::Label m_notice;
    sigc::connection m_noticeTimer;

    std::vector<Step> m_stepList;
    std::vector<Page *> m_pages;
    InstallPage *m_installPage{nullptr};
    DonePage *m_donePage{nullptr};

    int m_current{-1};
    bool m_syncing{false};
    bool m_altPressed{false};
    bool m_initialFocusDone{false};

    std::unique_ptr<ThemeWatcher> m_theme;
    std::unique_ptr<HelpWindow> m_help;
    std::unique_ptr<Gtk::AboutDialog> m_about;
};

} // namespace loadout
