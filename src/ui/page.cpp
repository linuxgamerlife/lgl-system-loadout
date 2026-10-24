#include "ui/page.h"

#include "ui/cardgrid.h"

#include <algorithm>
#include <cstddef>

namespace loadout {

void Page::registerGrid(CardGrid &grid)
{
    m_grids.push_back(&grid);
    grid.signalLeaveUp().connect(
        [this, &grid](const int column) { moveGridFocus(grid, column, -1); });
    grid.signalLeaveDown().connect(
        [this, &grid](const int column) { moveGridFocus(grid, column, 1); });
}

// Up and Down at the edge of a section continue into the neighbouring section.
void Page::moveGridFocus(CardGrid &from, const int column, const int delta)
{
    const auto found = std::find(m_grids.begin(), m_grids.end(), &from);
    if (found == m_grids.end())
        return;

    auto index = static_cast<std::ptrdiff_t>(found - m_grids.begin()) + delta;
    while (index >= 0 && index < static_cast<std::ptrdiff_t>(m_grids.size())) {
        CardGrid *candidate = m_grids[static_cast<std::size_t>(index)];
        if (candidate->cardCount() > 0) {
            candidate->focusColumn(column, delta < 0);
            return;
        }
        index += delta;
    }
}

} // namespace loadout
