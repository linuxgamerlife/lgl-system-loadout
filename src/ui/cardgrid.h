#pragma once

#include <gtkmm/box.h>
#include <gtkmm/flowbox.h>
#include <sigc++/sigc++.h>

#include <functional>
#include <vector>

namespace loadout {

// A responsive grid of cards that owns its keyboard navigation.
//
// GTK's own flowbox arrow handling depends on an internal cursor that only a
// mouse click sets, so programmatic focus leaves it dead. This widget handles
// the arrows itself, in the capture phase, and computes the grid geometry from
// the laid-out children. Callers only deal with cards, activation and the
// signals sent when the user moves out of the grid.
class CardGrid final : public Gtk::Box {
public:
    CardGrid();

    void addCard(Gtk::Widget &card);
    void setActivated(std::function<void(Gtk::FlowBoxChild &)> callback);
    void focusFirst();

    int cardCount() const;
    int columnCount() const;
    // Focus the top or bottom row in the given column, clamped to this grid.
    void focusColumn(int column, bool fromBottom);

    // The user pressed Left from the first column.
    sigc::signal<void()> &signalLeaveLeft() { return m_leaveLeft; }
    // The user pressed Up from the top row, or Down from the bottom row. The
    // argument is the column they were in, so a sibling grid can line up.
    sigc::signal<void(int)> &signalLeaveUp() { return m_leaveUp; }
    sigc::signal<void(int)> &signalLeaveDown() { return m_leaveDown; }

private:
    bool handleKey(guint keyval, Gdk::ModifierType state);
    int columns() const;

    Gtk::FlowBox m_flow;
    std::vector<Gtk::Widget *> m_cards;
    std::function<void(Gtk::FlowBoxChild &)> m_activated;
    sigc::signal<void()> m_leaveLeft;
    sigc::signal<void(int)> m_leaveUp;
    sigc::signal<void(int)> m_leaveDown;
};

} // namespace loadout
