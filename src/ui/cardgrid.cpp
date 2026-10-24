#include "ui/cardgrid.h"

#include <gdk/gdkkeysyms.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/flowboxchild.h>
#include <gtkmm/root.h>
#include <gtkmm/window.h>

#include <algorithm>

namespace loadout {

CardGrid::CardGrid()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0)
{
    m_flow.set_selection_mode(Gtk::SelectionMode::NONE);
    m_flow.set_activate_on_single_click(true);
    m_flow.set_homogeneous(true);
    m_flow.set_max_children_per_line(4);
    m_flow.set_min_children_per_line(2);
    m_flow.set_column_spacing(12);
    m_flow.set_row_spacing(12);
    m_flow.set_valign(Gtk::Align::START);
    m_flow.set_hexpand(true);
    m_flow.add_css_class("item-grid");
    // Qualified: addCard() below hides Gtk::Box::append().
    Gtk::Box::append(m_flow);

    m_flow.signal_child_activated().connect([this](Gtk::FlowBoxChild *child) {
        if (child && m_activated)
            m_activated(*child);
    });

    auto controller = Gtk::EventControllerKey::create();
    controller->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    controller->signal_key_pressed().connect(
        [this](guint keyval, guint, Gdk::ModifierType state) {
            return handleKey(keyval, state);
        },
        false);
    // On the CardGrid, not the FlowBox: a capture controller on the flowbox runs
    // after its own arrow bindings, which swallow the key first.
    add_controller(controller);
}

namespace {

// Focus transitions between the two panes are explicit rather than inferred.
void focusWidget(Gtk::Widget &widget)
{
    if (auto *window = dynamic_cast<Gtk::Window *>(widget.get_root())) {
        window->set_focus(widget);
        return;
    }
    widget.grab_focus();
}

} // namespace

void CardGrid::addCard(Gtk::Widget &card)
{
    m_cards.push_back(&card);
    m_flow.append(card);
}

void CardGrid::setActivated(std::function<void(Gtk::FlowBoxChild &)> callback)
{
    m_activated = std::move(callback);
}

void CardGrid::focusFirst()
{
    if (!get_mapped())
        return;
    if (auto *first = m_flow.get_child_at_index(0)) {
        focusWidget(*first);
        return;
    }
    m_flow.child_focus(Gtk::DirectionType::TAB_FORWARD);
}

int CardGrid::cardCount() const
{
    return static_cast<int>(m_cards.size());
}

int CardGrid::columnCount() const
{
    return columns();
}

void CardGrid::focusColumn(const int column, const bool fromBottom)
{
    const int total = cardCount();
    if (total == 0)
        return;

    const int cols = columns();
    const int col = std::clamp(column, 0, cols - 1);
    int index = fromBottom ? total - 1 : std::min(col, total - 1);

    // On an incomplete last row, walk back to the last row that has this
    // column, so moving up from a short row still lands somewhere sensible.
    while (fromBottom && index > col && index % cols != col)
        --index;

    if (auto *child = m_flow.get_child_at_index(index))
        focusWidget(*child);
}

// How many cards share the first row. Read from the allocated positions, so it
// follows the grid as it reflows.
int CardGrid::columns() const
{
    auto *first = m_flow.get_child_at_index(0);
    if (!first)
        return 1;

    const int firstRow = first->get_allocation().get_y();
    int count = 0;
    while (auto *candidate = m_flow.get_child_at_index(count)) {
        if (candidate->get_allocation().get_y() != firstRow)
            break;
        ++count;
    }
    return std::max(1, count);
}

bool CardGrid::handleKey(const guint keyval, const Gdk::ModifierType state)
{
    if (keyval != GDK_KEY_Left && keyval != GDK_KEY_Right && keyval != GDK_KEY_Up &&
        keyval != GDK_KEY_Down)
        return false;

    const auto modifiers = Gdk::ModifierType::CONTROL_MASK | Gdk::ModifierType::ALT_MASK |
                           Gdk::ModifierType::SUPER_MASK;
    if ((state & modifiers) != Gdk::ModifierType(0))
        return false;

    // The focused widget, via the window: Gtk::Widget::get_focus_child() is
    // protected in gtkmm.
    Gtk::Widget *focus = nullptr;
    if (auto *window = dynamic_cast<Gtk::Window *>(get_root()))
        focus = window->get_focus();

    auto *child = dynamic_cast<Gtk::FlowBoxChild *>(focus);
    if (!child || child->get_parent() != &m_flow)
        return false;

    const int index = child->get_index();
    const int count = columns();
    const int column = index % count;

    int target = -1;
    if (keyval == GDK_KEY_Left) {
        if (column == 0) {
            m_leaveLeft.emit();
            return true;
        }
        target = index - 1;
    } else if (keyval == GDK_KEY_Right) {
        if (column == count - 1)
            return false;
        target = index + 1;
    } else if (keyval == GDK_KEY_Up) {
        // Above the top row: let the page hand focus to the section above.
        if (index < count) {
            m_leaveUp.emit(column);
            return true;
        }
        target = index - count;
    } else {
        // Below the bottom row: let the page hand focus to the section below.
        if (index + count >= cardCount()) {
            m_leaveDown.emit(column);
            return true;
        }
        target = index + count;
    }

    if (target < 0)
        return false;

    auto *next = m_flow.get_child_at_index(target);
    if (!next)
        return false;

    focusWidget(*next);
    return true;
}

} // namespace loadout
