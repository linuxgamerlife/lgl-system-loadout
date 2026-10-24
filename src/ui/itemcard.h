#pragma once

#include "core/catalog.h"

#include <gtkmm/box.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

#include <functional>
#include <optional>
#include <string>

namespace loadout {

// A package tile: icon, name, short description, and installed-state badge.
//
// Real icons come from the desktop icon theme, cropped to their visible bounds.
// Packages without artwork get a generated tile: a gradient in a colour derived
// from the package key, with a category glyph from the icon theme on top.
class ItemCard final : public Gtk::Box {
public:
    ItemCard();

    void configure(const catalog::Item &item, const std::string &source = {},
                   const std::string &glyphOverride = {}, bool lglChoice = false);
    void setInstalled(std::optional<bool> installed);

    // Re-resolves the icon, for the page's Refresh button.
    void refreshIcon();

    // A manual install method: the command to run, with a Copy button.
    void setCommand(const std::string &command,
                    const std::function<void(const std::string &, bool)> &notify);

private:
    void applyIcon(const std::string &key, const std::string &label,
                   const std::string &glyphOverride);

    Gtk::DrawingArea m_backdrop;
    Gtk::Image m_glyph;
    Gtk::Image m_icon;
    Gtk::Label m_title;
    Gtk::Label m_description;
    Gtk::Label m_chip;
    Gtk::Label m_choiceChip;
    Gtk::Label m_sourceChip;
    std::string m_key;
    std::string m_label;
    std::string m_source;
    std::string m_glyphOverride;
    bool m_lglChoice{false};
};

} // namespace loadout
