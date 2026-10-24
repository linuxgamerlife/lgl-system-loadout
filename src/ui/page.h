#pragma once

#include <gtkmm/box.h>
#include <gtkmm/root.h>
#include <gtkmm/window.h>
#include <sigc++/sigc++.h>

#include <functional>
#include <string>
#include <vector>

namespace loadout {

class CardGrid;

// Reports a non-blocking result to the window.
using Notify = std::function<void(const std::string &, bool positive)>;

// One wizard step. Pages never rebuild themselves when shown: they read the
// shared Selection and keep their widgets in sync, so choices survive moving
// between steps.
class Page : public Gtk::Box {
public:
    Page()
        : Gtk::Box(Gtk::Orientation::VERTICAL, 12)
    {
        set_valign(Gtk::Align::START);
    }

    virtual void onShown() {}
    virtual void syncFromSelection() {}
    // Where focus should land when the user moves right out of the step list.
    virtual void focusContent() { child_focus(Gtk::DirectionType::TAB_FORWARD); }

    // Emitted when a card grid asks for focus to return to the step list.
    sigc::signal<void()> &signalFocusSteps() { return m_focusSteps; }
    virtual bool canAdvance() const { return true; }
    virtual bool navigationLocked() const { return false; }

    sigc::signal<void()> &signalStateChanged() { return m_stateChanged; }

protected:
    void stateChanged() { m_stateChanged.emit(); }

    // Registers a grid for vertical navigation. Grids are visited in the order
    // they are registered, which is the order they appear on the page.
    void registerGrid(CardGrid &grid);

    // Dialogs are transient for the window that owns this page.
    Gtk::Window *hostWindow() { return dynamic_cast<Gtk::Window *>(get_root()); }

private:
    void moveGridFocus(CardGrid &from, int column, int delta);

    std::vector<CardGrid *> m_grids;
    sigc::signal<void()> m_stateChanged;
    sigc::signal<void()> m_focusSteps;
};

} // namespace loadout
