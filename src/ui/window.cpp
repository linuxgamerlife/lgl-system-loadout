#include "ui/window.h"

#include "core/catalog.h"
#include "ui/helpwindow.h"
#include "ui/page.h"
#include "ui/pages/donepage.h"
#include "ui/pages/choicepage.h"
#include "ui/pages/gpupage.h"
#include "ui/pages/installpage.h"
#include "ui/pages/reviewpage.h"
#include "ui/pages/selectpage.h"
#include "ui/pages/updatepage.h"
#include "ui/pages/welcomepage.h"
#include "ui/themewatcher.h"

#include <gdk/gdkkeysyms.h>
#include <gdkmm/texture.h>
#include <giomm/menu.h>
#include <giomm/simpleaction.h>
#include <glibmm/main.h>
#include <gtkmm/application.h>
#include <gtkmm/editable.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/flowboxchild.h>
#include <gtkmm/image.h>
#include <gtkmm/listboxrow.h>
#include <gtkmm/overlay.h>
#include <gtkmm/paned.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>

#include <algorithm>

namespace loadout {
namespace {

bool isAlt(const guint keyval)
{
    return keyval == GDK_KEY_Alt_L || keyval == GDK_KEY_Alt_R;
}

Glib::RefPtr<Gdk::Texture> appIcon()
{
    static const auto icon =
        Gdk::Texture::create_from_resource("/com/linuxgamerlife/loadout/lgl-system-loadout.png");
    return icon;
}

std::string wrapCredit(const std::string &text, const std::size_t width = 62)
{
    std::istringstream words(text);
    std::string word;
    std::string line;
    std::string result;
    while (words >> word) {
        if (line.empty()) {
            line = word;
        } else if (line.size() + 1 + word.size() <= width) {
            line += " " + word;
        } else {
            if (!result.empty())
                result += "\n";
            result += line;
            line = word;
        }
    }
    if (!line.empty()) {
        if (!result.empty())
            result += "\n";
        result += line;
    }
    return result;
}

Gtk::Label *label(const char *text, const char *cssClass)
{
    auto *result = Gtk::make_managed<Gtk::Label>(text);
    result->add_css_class(cssClass);
    result->set_halign(Gtk::Align::START);
    return result;
}

const std::vector<Step> &steps()
{
    static const std::vector<Step> list{
        {"welcome", "Welcome", "What this wizard will do to your system."},
        {"update", "System Update", "A full update is recommended before installing anything."},
        {"repositories", "Repositories", "RPM Fusion provides the packages most of this wizard installs."},
        {"essentials", "Essentials", "A file manager and a terminal for everyday work."},
        {"system-tools", "System Tools", "Essential command-line and system utilities."},
        {"remote-access", "Remote Access", "Stream games, control another machine, or reach your systems from anywhere."},
        {"development", "Development Tools", "Python, package managers, editors, and CLI utilities."},
        {"multimedia", "Multimedia & Codecs", "Video, audio, and codec support."},
        {"content", "Content Creation", "Tools for video, audio, graphics, and 3D creation."},
        {"graphics", "GPU Drivers", "Driver options for your graphics hardware."},
        {"gaming", "Gaming", "Launchers, compatibility tools, overlays, and utilities."},
        {"virtualisation", "Virtualisation", "The KVM/QEMU stack and management tools."},
        {"browsers", "Browsers", "Web browsers and the repositories that provide them."},
        {"communication", "Communication & Productivity", "Chat, email, office, and music applications."},
        {"kernel", "CachyOS Kernel", "A performance-optimised kernel with better scheduling."},
        {"toolkit", "LGL Tool Kit", "Small utilities from LinuxGamerLife."},
        {"review", "Review Selections", "Confirm everything before the installation begins."},
        {"install", "Install Now", "Review the plan, then start the installation."},
        {"done", "Setup Complete", "What was installed, and what to do next."},
    };
    return list;
}

} // namespace

Window::Window(const Glib::RefPtr<Gtk::Application> &application)
    : m_noticeBox(Gtk::Orientation::HORIZONTAL)
{
    set_application(application);
    set_title("LGL System Loadout");
    set_icon_name("lgl-system-loadout");
    set_default_size(1082, 1234);

    // Startup probe: fast local binaries only (rpm, logname), never dnf.
    m_system = detectSystem();

    m_theme = std::make_unique<ThemeWatcher>(*this);
    buildHeader();
    buildBody();
    buildPages();
    buildActions(application);
    applyTitleBar(m_settings.systemTitleBar());

    watchAltTap(*this);
    if (m_menuButton->get_popover())
        watchAltTap(*m_menuButton->get_popover());
    watchStepsExit();

    // Put the keyboard on the current step once the window exists, so Right
    // works immediately instead of only after a click.
    signal_map().connect([this] {
        if (m_initialFocusDone)
            return;
        m_initialFocusDone = true;
        focusPrimaryInput();
    });

    showStep(0);
}

Window::~Window() = default;

void Window::buildHeader()
{
    auto *brand = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
    brand->set_margin_end(12);
    auto *icon = Gtk::make_managed<Gtk::Image>(appIcon());
    icon->set_pixel_size(28);
    icon->add_css_class("brand-icon");
    brand->append(*icon);
    brand->append(*label("LGL SYSTEM LOADOUT", "wordmark"));
    m_header.pack_start(*brand);

    m_header.set_title_widget(*Gtk::make_managed<Gtk::Box>());

    auto menu = Gio::Menu::create();
    auto view = Gio::Menu::create();
    view->append("Use System Title Bar", "win.system-title-bar");
    menu->append_section(view);
    auto help = Gio::Menu::create();
    help->append("Help", "win.help");
    help->append("About LGL System Loadout", "win.about");
    menu->append_section(help);
    auto quit = Gio::Menu::create();
    quit->append("Quit", "app.quit");
    menu->append_section(quit);

    m_menuButton = Gtk::make_managed<Gtk::MenuButton>();
    m_menuButton->set_icon_name("open-menu-symbolic");
    m_menuButton->set_menu_model(menu);
    m_menuButton->set_tooltip_text("Main menu");
    m_menuButton->set_primary(true);
    m_header.pack_end(*m_menuButton);
}

void Window::buildBody()
{
    set_child(m_content);

    auto *paned = Gtk::make_managed<Gtk::Paned>(Gtk::Orientation::HORIZONTAL);
    paned->add_css_class("body");
    paned->set_position(280);
    paned->set_resize_start_child(false);
    paned->set_shrink_start_child(false);
    paned->set_shrink_end_child(true);
    paned->set_vexpand(true);

    auto *sidebar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    sidebar->add_css_class("lo-card");
    sidebar->add_css_class("flush");
    sidebar->set_overflow(Gtk::Overflow::HIDDEN);

    auto *heading = label("STEPS", "section-label");
    heading->add_css_class("card-head");
    sidebar->append(*heading);

    m_steps.set_selection_mode(Gtk::SelectionMode::SINGLE);
    m_steps.add_css_class("step-list");

    auto *stepScroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    stepScroller->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    stepScroller->add_css_class("step-scroller");
    stepScroller->add_css_class("no-scrollbar");
    stepScroller->set_child(m_steps);
    stepScroller->set_vexpand(true);
    sidebar->append(*stepScroller);
    paned->set_start_child(*sidebar);

    m_stack.set_transition_type(Gtk::StackTransitionType::CROSSFADE);
    m_stack.set_vexpand(true);

    auto *contentScroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    contentScroller->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    contentScroller->add_css_class("no-scrollbar");
    contentScroller->set_child(m_stack);
    contentScroller->set_vexpand(true);
    m_contentScroller = contentScroller;

    // The Continue control is a slim footer at the bottom left of the content
    // area. It is outside the scroller, so it stays in place while a page scrolls.
    auto *contentArea = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
    contentArea->append(*contentScroller);

    m_continueBar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 0);
    m_continueBar->set_halign(Gtk::Align::START);
    m_continueBar->set_margin_start(12);
    m_continueBar->set_margin_end(12);
    m_continueBar->set_margin_top(8);
    m_continueBar->set_margin_bottom(10);
    m_continueBar->set_visible(false);
    m_continueButton = Gtk::make_managed<Gtk::Button>("Continue");
    m_continueButton->add_css_class("lo-small");
    m_continueButton->add_css_class("suggested-action");
    m_continueButton->set_tooltip_text("Move to the next step");
    m_continueButton->signal_clicked().connect([this] { showStep(m_current + 1); });
    m_continueBar->append(*m_continueButton);
    contentArea->append(*m_continueBar);
    contentArea->set_vexpand(true);
    paned->set_end_child(*contentArea);

    m_notice.set_wrap(true);
    m_notice.set_max_width_chars(80);
    m_noticeBox.add_css_class("lo-notice");
    m_noticeBox.append(m_notice);
    m_noticeRevealer.set_child(m_noticeBox);
    m_noticeRevealer.set_transition_type(Gtk::RevealerTransitionType::SLIDE_UP);
    m_noticeRevealer.set_halign(Gtk::Align::CENTER);
    m_noticeRevealer.set_valign(Gtk::Align::END);
    m_noticeRevealer.set_can_target(false);

    auto *overlay = Gtk::make_managed<Gtk::Overlay>();
    overlay->set_child(*paned);
    overlay->add_overlay(m_noticeRevealer);
    overlay->set_vexpand(true);
    m_content.append(*overlay);
}

void Window::buildPages()
{
    m_stepList = steps();

    const auto changed = [this] { refreshSelections(); };
    const auto notifyText = [this](const std::string &text, const bool positive) {
        notify(text, positive ? NoticeKind::Positive : NoticeKind::Negative, 5000);
    };

    m_pages.push_back(new WelcomePage(m_checker, m_system));
    m_pages.push_back(new UpdatePage(m_system, notifyText));

    m_pages.push_back(new SelectPage(*catalog::page("repositories"), m_selection, m_checker,
                                     m_system, changed, notifyText));
    m_pages.push_back(new ChoicePage(m_selection, m_checker, m_system, changed));

    for (const char *id : {"system-tools", "remote-access", "development", "multimedia", "content"})
        m_pages.push_back(new SelectPage(*catalog::page(id), m_selection, m_checker, m_system,
                                         changed, notifyText));

    m_pages.push_back(new GpuPage(m_selection, m_checker, m_system, changed));

    for (const char *id : {"gaming", "virtualisation", "browsers", "communication", "kernel",
                           "toolkit"})
        m_pages.push_back(new SelectPage(*catalog::page(id), m_selection, m_checker, m_system,
                                         changed, notifyText));

    m_pages.push_back(new ReviewPage(m_selection, m_system));

    auto *install = new InstallPage(m_selection, m_system, notifyText);
    m_installPage = install;
    m_pages.push_back(install);

    auto *done = new DonePage(notifyText);
    m_donePage = done;
    m_pages.push_back(done);

    for (std::size_t index = 0; index < m_pages.size(); ++index) {
        Page *page = m_pages[index];
        page->signalStateChanged().connect([this] { updateNavigation(); });
        page->signalFocusSteps().connect([this] { focusPrimaryInput(); });
        m_stack.add(*page, m_stepList[index].id);

        auto *row = Gtk::make_managed<Gtk::ListBoxRow>();
        auto *rowLabel = label(m_stepList[index].title, "small");
        rowLabel->set_xalign(0.0f);
        row->set_child(*rowLabel);
        m_steps.append(*row);
    }

    m_steps.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
        if (m_syncing || !row)
            return;
        showStep(row->get_index());
    });

    refreshStepVisibility();
}

void Window::buildActions(const Glib::RefPtr<Gtk::Application> &application)
{
    add_action("back", [this] { showStep(m_current - 1); });
    add_action("next", [this] { showStep(m_current + 1); });
    add_action("help", sigc::mem_fun(*this, &Window::showHelp));
    add_action("about", sigc::mem_fun(*this, &Window::showAbout));

    auto titleBar = add_action_bool("system-title-bar", false);
    titleBar->signal_activate().connect([titleBar](const Glib::VariantBase &) {
        bool system = false;
        titleBar->get_state(system);
        titleBar->change_state(!system);
    });
    titleBar->signal_change_state().connect([this, titleBar](const Glib::VariantBase &value) {
        const bool system = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(value).get();
        titleBar->set_state(value);
        m_settings.setSystemTitleBar(system);
        applyTitleBar(system);
        notify(system ? "Using the system title bar." : "Using the app title bar.",
               NoticeKind::Info, 5000);
    });
    titleBar->set_state(Glib::Variant<bool>::create(m_settings.systemTitleBar()));

    application->set_accel_for_action("win.next", "<Control>Page_Down");
    application->set_accel_for_action("win.back", "<Control>Page_Up");
    application->set_accel_for_action("win.help", "F1");
    application->set_accel_for_action("win.system-title-bar", "<Control><Shift>m");
}

void Window::showStep(const int index)
{
    if (index < 0 || index >= static_cast<int>(m_pages.size()) || index == m_current)
        return;

    // Never walk away from a running installation or update.
    if (m_current >= 0 && m_pages[static_cast<std::size_t>(m_current)]->navigationLocked()) {
        syncStepSelection();
        return;
    }

    // Progressive disclosure is not just visual: the keyboard shortcuts must
    // not jump to a step that is still hidden.
    const bool unlocked = m_pages.size() > 1 && m_pages[1]->canAdvance();
    if (!unlocked && index > 1) {
        syncStepSelection();
        return;
    }

    if (std::string(m_stepList[static_cast<std::size_t>(index)].id) == "done" && m_installPage &&
        m_donePage)
        m_donePage->setSummary(m_installPage->summary());

    m_current = index;
    m_stack.set_visible_child(m_stepList[static_cast<std::size_t>(index)].id);
    if (m_contentScroller)
        if (const auto adjustment = m_contentScroller->get_vadjustment())
            adjustment->set_value(0);
    syncStepSelection();
    updateNavigation();
    focusPrimaryInput();
    m_pages[static_cast<std::size_t>(index)]->onShown();
}

void Window::updateNavigation()
{
    if (m_current < 0 || m_current >= static_cast<int>(m_pages.size()))
        return;

    // The sidebar is the only navigation, so it only has to be locked while an
    // installation is running.
    m_steps.set_sensitive(!m_pages[static_cast<std::size_t>(m_current)]->navigationLocked());
    refreshStepVisibility();
    refreshContinueButton();
}

void Window::refreshContinueButton()
{
    if (!m_continueBar || !m_continueButton)
        return;

    const bool first = m_current == 0;
    const bool last = m_current >= 0 && m_current == static_cast<int>(m_pages.size()) - 1;
    const bool ready = m_current >= 0 && m_pages[static_cast<std::size_t>(m_current)]->canAdvance() &&
                       !m_pages[static_cast<std::size_t>(m_current)]->navigationLocked();
    const bool show = !first && !last && ready;
    m_continueBar->set_visible(show);
    m_continueButton->set_visible(show);
    if (show && m_current + 1 < static_cast<int>(m_stepList.size()))
        m_continueButton->set_tooltip_text(
            "Go to " + std::string(m_stepList[static_cast<std::size_t>(m_current + 1)].title));
    else
        m_continueButton->set_tooltip_text("Move to the next step");
}

// Until the system update has run or been skipped, only the first two steps are
// offered: the rest of the wizard depends on an up-to-date system.
void Window::refreshStepVisibility()
{
    const bool unlocked = m_pages.size() > 1 && m_pages[1]->canAdvance();
    for (int index = 0; index < static_cast<int>(m_pages.size()); ++index) {
        if (auto *row = m_steps.get_row_at_index(index))
            row->set_visible(unlocked || index <= 1);
    }
}

void Window::syncStepSelection()
{
    m_syncing = true;
    if (auto *row = m_steps.get_row_at_index(m_current))
        m_steps.select_row(*row);
    m_syncing = false;
}

void Window::focusPrimaryInput()
{
    // set_focus() before the window is realized can be a no-op or a warning;
    // during construction there is nothing to focus yet anyway.
    if (!get_realized()) {
        if (auto *row = m_steps.get_row_at_index(m_current))
            row->grab_focus();
        return;
    }

    if (auto *row = m_steps.get_row_at_index(m_current))
        set_focus(*row);
    else
        set_focus(m_steps);
}

void Window::focusContent()
{
    if (m_current >= 0 && m_current < static_cast<int>(m_pages.size()))
        m_pages[static_cast<std::size_t>(m_current)]->focusContent();
}

void Window::refreshSelections()
{
    for (Page *page : m_pages)
        page->syncFromSelection();
    updateNavigation();
}

void Window::applyTitleBar(const bool system)
{
    const bool realized = get_realized();
    const auto application = get_application();

    if (realized) {
        application->hold();
        set_visible(false);
        Gtk::Widget::unrealize();
    }

    if (system) {
        unset_titlebar();
        m_header.set_show_title_buttons(false);
        m_content.prepend(m_header);
    } else {
        if (m_header.get_parent())
            m_content.remove(m_header);
        m_header.set_show_title_buttons(true);
        set_titlebar(m_header);
    }

    if (realized) {
        set_application(application);
        present();
        focusPrimaryInput();
        application->release();
    }
}

void Window::showHelp()
{
    if (!m_help)
        m_help = std::make_unique<HelpWindow>(*this);
    m_help->present();
}

void Window::showAbout()
{
    if (!m_about) {
        m_about = std::make_unique<Gtk::AboutDialog>();
        m_about->set_transient_for(*this);
        m_about->set_modal(true);
        m_about->set_hide_on_close(true);
        m_about->set_default_size(640, 520);
        m_about->set_resizable(false);
        m_about->set_wrap_license(true);
        m_about->set_program_name("LGL System Loadout");
        m_about->set_version(LOADOUT_VERSION);
        m_about->set_comments(
            "Get a fresh Fedora install ready for gaming, content creation, and development "
            "without the terminal. Pick what you want, and one password prompt covers the "
            "package installation.\n\nBuilt with GTK 4.");
        m_about->set_website("https://github.com/linuxgamerlife/lgl-system-loadout");
        m_about->set_website_label("Project page and issue tracker");
        m_about->set_authors(
            {wrapCredit("LinuxGamerLife https://www.youtube.com/@linuxgamerlife")});
        m_about->add_credit_section(
            "Support",
            {wrapCredit("Support development on Ko-fi https://ko-fi.com/G2G3V70LW")});
        m_about->add_credit_section(
            "Legal",
            {wrapCredit("All product names, logos and brands are the property of their respective "
                        "owners. They are used here only to identify the applications this wizard "
                        "installs, and that use does not imply any affiliation with or endorsement "
                        "by them."),
             wrapCredit("FilmCraft and SoundCraft icons © ArtCraft Team and contributors, MIT OR "
                        "Apache-2.0. The ArtCraft launcher mark is a trademark of the ArtCraft Team, "
                        "used only to identify ArtCraft Launcher.")});
        m_about->set_license_type(Gtk::License::MIT_X11);
        m_about->set_logo(appIcon());
    }
    m_about->present();
}

void Window::notify(const std::string &text, const NoticeKind kind, const unsigned int timeoutMs)
{
    for (const char *name : {"positive", "negative"})
        m_noticeBox.remove_css_class(name);

    if (kind == NoticeKind::Positive)
        m_noticeBox.add_css_class("positive");
    else if (kind == NoticeKind::Negative)
        m_noticeBox.add_css_class("negative");

    m_notice.set_text(text);
    m_noticeRevealer.set_reveal_child(true);
    m_noticeTimer.disconnect();
    if (timeoutMs > 0)
        m_noticeTimer = Glib::signal_timeout().connect(
            [this] {
                m_noticeRevealer.set_reveal_child(false);
                return false;
            },
            timeoutMs);
}

// Right in the step list moves focus into the current page. This lives on the
// window because a controller attached to the listbox sees no key events.
void Window::watchStepsExit()
{
    auto controller = Gtk::EventControllerKey::create();
    controller->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    controller->signal_key_pressed().connect(
        [this](guint keyval, guint, Gdk::ModifierType state) {
            if (keyval != GDK_KEY_Right)
                return false;
            const auto modifiers = Gdk::ModifierType::CONTROL_MASK |
                                   Gdk::ModifierType::ALT_MASK |
                                   Gdk::ModifierType::SUPER_MASK;
            if ((state & modifiers) != Gdk::ModifierType(0))
                return false;

            Gtk::Widget *focus = get_focus();
            if (!focus)
                return false;
            for (Gtk::Widget *widget = focus; widget; widget = widget->get_parent())
                if (widget == &m_steps) {
                    focusContent();
                    return true;
                }
            return false;
        },
        false);
    add_controller(controller);
}

void Window::watchAltTap(Gtk::Widget &widget)
{
    auto controller = Gtk::EventControllerKey::create();
    controller->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);

    controller->signal_key_pressed().connect(
        [this](guint keyval, guint, Gdk::ModifierType) {
            m_altPressed = isAlt(keyval);
            return false;
        },
        false);

    controller->signal_key_released().connect(
        [this](guint keyval, guint, Gdk::ModifierType) {
            const bool tapped = isAlt(keyval) && m_altPressed;
            m_altPressed = false;
            if (!tapped)
                return;

            auto *popover = m_menuButton->get_popover();
            if (popover && popover->get_visible())
                popover->popdown();
            else
                m_menuButton->popup();

            Glib::signal_idle().connect_once([this] { focusPrimaryInput(); });
        },
        false);

    widget.add_controller(controller);
}

} // namespace loadout
