#include "ui/itemcard.h"

#include <gdkmm/display.h>
#include <gdkmm/pixbuf.h>
#include <gdkmm/texture.h>
#include <giomm/file.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/iconpaintable.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/overlay.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <functional>
#include <map>
#include <string>

namespace loadout {
namespace {

struct Rgb final {
    double red;
    double green;
    double blue;
};

Rgb hslToRgb(const double hue, const double saturation, const double lightness)
{
    const auto channel = [](const double p, const double q, double t) {
        if (t < 0.0)
            t += 1.0;
        if (t > 1.0)
            t -= 1.0;
        if (t < 1.0 / 6.0)
            return p + (q - p) * 6.0 * t;
        if (t < 1.0 / 2.0)
            return q;
        if (t < 2.0 / 3.0)
            return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
        return p;
    };

    if (saturation <= 0.0)
        return {lightness, lightness, lightness};

    const double q = lightness < 0.5 ? lightness * (1.0 + saturation)
                                     : lightness + saturation - lightness * saturation;
    const double p = 2.0 * lightness - q;
    return {channel(p, q, hue + 1.0 / 3.0), channel(p, q, hue), channel(p, q, hue - 1.0 / 3.0)};
}

// A stable hue per package, so a generated tile looks the same between runs.
double hueFor(const std::string &key)
{
    const std::size_t hash = std::hash<std::string>{}(key);
    return static_cast<double>(hash % 360u) / 360.0;
}

// The category glyph drawn on a generated tile.
std::string glyphFor(const std::string &key)
{
    if (key.rfind("manual:", 0) == 0)
        return "utilities-terminal-symbolic";
    if (key == "systools/flatseal" || key == "systools/timeshift")
        return "applications-utilities-symbolic";
    if (key == "comms/office_calc" || key == "comms/office_writer")
        return "x-office-document-symbolic";

    const auto slash = key.find('/');
    const std::string page = slash == std::string::npos ? key : key.substr(0, slash);

    if (page == "repos")
        return "system-software-install-symbolic";
    if (page == "systools")
        return "utilities-terminal-symbolic";
    if (page == "python")
        return "applications-development-symbolic";
    if (page == "media")
        return "applications-multimedia-symbolic";
    if (page == "content")
        return "applications-graphics-symbolic";
    if (page == "gpu")
        return "video-display-symbolic";
    if (page == "gaming")
        return "applications-games-symbolic";
    if (page == "virt")
        return "computer-symbolic";
    if (page == "browsers")
        return "web-browser-symbolic";
    if (page == "comms")
        return "applications-internet-symbolic";
    if (page == "cachyos")
        return "application-x-executable-symbolic";
    if (page == "toolkit")
        return "applications-utilities-symbolic";
    return "package-x-generic-symbolic";
}

void roundedSquare(cairo_t *cr, const double size, const double radius)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, size - radius, radius, radius, -M_PI / 2.0, 0.0);
    cairo_arc(cr, size - radius, size - radius, radius, 0.0, M_PI / 2.0);
    cairo_arc(cr, radius, size - radius, radius, M_PI / 2.0, M_PI);
    cairo_arc(cr, radius, radius, radius, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

// A generated app tile: diagonal gradient, soft highlight, inner edge. The
// category glyph is a themed symbolic icon drawn on top by the caller.
void drawGeneratedTile(cairo_t *cr, const int width, const int height, const std::string &key,
                       const std::string &letter = {})
{
    const double size = std::min(width, height);
    const double hue = hueFor(key);
    const Rgb start = hslToRgb(hue, 0.62, 0.56);
    const Rgb end = hslToRgb(std::fmod(hue + 0.09, 1.0), 0.68, 0.36);

    cairo_pattern_t *gradient = cairo_pattern_create_linear(0.0, 0.0, size, size);
    cairo_pattern_add_color_stop_rgb(gradient, 0.0, start.red, start.green, start.blue);
    cairo_pattern_add_color_stop_rgb(gradient, 1.0, end.red, end.green, end.blue);

    cairo_save(cr);
    roundedSquare(cr, size, size * 0.24);
    cairo_set_source(cr, gradient);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(gradient);

    cairo_pattern_t *highlight = cairo_pattern_create_radial(
        size * 0.32, size * 0.24, 0.0, size * 0.32, size * 0.24, size * 0.8);
    cairo_pattern_add_color_stop_rgba(highlight, 0.0, 1.0, 1.0, 1.0, 0.20);
    cairo_pattern_add_color_stop_rgba(highlight, 1.0, 1.0, 1.0, 1.0, 0.0);
    cairo_set_source(cr, highlight);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(highlight);

    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.16);
    cairo_stroke(cr);
    cairo_restore(cr);

    if (letter.empty())
        return;

    // A monogram for apps whose own mark cannot be bundled.
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, size * 0.46);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.95);
    cairo_text_extents_t extents{};
    cairo_text_extents(cr, letter.c_str(), &extents);
    cairo_move_to(cr, (size - extents.width) / 2.0 - extents.x_bearing,
                  (size - extents.height) / 2.0 - extents.y_bearing);
    cairo_show_text(cr, letter.c_str());
}

// Trims the transparent margin baked into an icon's artwork, then scales it down
// so the longest edge is 128 px (enough for a 64 px cell at 2x).
Glib::RefPtr<Gdk::Pixbuf> croppedIcon(const Glib::RefPtr<Gdk::Pixbuf> &pixbuf)
{
    if (!pixbuf || !pixbuf->get_has_alpha())
        return pixbuf;

    const int width = pixbuf->get_width();
    const int height = pixbuf->get_height();
    const int channels = pixbuf->get_n_channels();
    const int stride = pixbuf->get_rowstride();
    const guint8 *pixels = pixbuf->get_pixels();

    // Faint drop shadows are common; ignore them so the visible artwork decides
    // the bounds rather than the shadow around it.
    constexpr guint8 alphaThreshold = 24;
    int minX = width;
    int minY = height;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < height; ++y) {
        const guint8 *row = pixels + static_cast<gsize>(y) * static_cast<gsize>(stride);
        for (int x = 0; x < width; ++x) {
            if (row[x * channels + 3] <= alphaThreshold)
                continue;
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
    }

    if (maxX < minX || maxY < minY)
        return pixbuf;   // Completely transparent: leave it alone.

    const int croppedWidth = maxX - minX + 1;
    const int croppedHeight = maxY - minY + 1;
    if (croppedWidth == width && croppedHeight == height)
        return pixbuf;

    auto cropped =
        Gdk::Pixbuf::create_subpixbuf(pixbuf, minX, minY, croppedWidth, croppedHeight);
    if (!cropped)
        return pixbuf;

    constexpr int target = 128;
    const int longest = std::max(croppedWidth, croppedHeight);
    if (longest <= target)
        return cropped;

    const double factor = static_cast<double>(target) / longest;
    const int scaledWidth = std::max(1, static_cast<int>(std::round(croppedWidth * factor)));
    const int scaledHeight = std::max(1, static_cast<int>(std::round(croppedHeight * factor)));
    return cropped->scale_simple(scaledWidth, scaledHeight, Gdk::InterpType::BILINEAR);
}

// Returns the cropped texture for an icon name, or an empty pointer when the
// icon has no file on disk (built-in theme icons) so the caller can fall back.
void ensureBundledIcons(const Glib::RefPtr<Gtk::IconTheme> &theme)
{
    static Glib::RefPtr<Gtk::IconTheme> registered;
    if (theme && theme != registered) {
        theme->add_resource_path("/com/linuxgamerlife/loadout/icons");
        registered = theme;
    }
}

Glib::RefPtr<Gdk::Texture> iconTexture(const std::string &name)
{
    static std::map<std::string, Glib::RefPtr<Gdk::Texture>> cache;
    static std::map<std::string, bool> unavailable;

    if (const auto found = cache.find(name); found != cache.end())
        return found->second;
    if (unavailable.count(name))
        return {};

    Glib::RefPtr<Gdk::Texture> texture;
    try {
        const auto theme = Gtk::IconTheme::get_for_display(Gdk::Display::get_default());
        if (!theme)
            return {};
        const auto paintable = theme->lookup_icon(name, 128);
        if (!paintable)
            return {};
        const auto file = paintable->get_file();
        if (!file)
            return {};
        const auto path = file->get_path();
        if (path.empty())
            return {};

        const auto pixbuf = Gdk::Pixbuf::create_from_file(path, 128, 128, true);
        if (!pixbuf)
            return {};
        texture = Gdk::Texture::create_for_pixbuf(croppedIcon(pixbuf));
    } catch (const Glib::Error &) {
        unavailable[name] = true;
        return {};
    }

    if (!texture) {
        unavailable[name] = true;
        return {};
    }
    cache[name] = texture;
    return texture;
}

} // namespace

ItemCard::ItemCard()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0)
{
    add_css_class("item-card");
    set_size_request(250, -1);

    auto *top = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 14);
    top->set_valign(Gtk::Align::START);

    // An Overlay, not a Box: every child gets the full 64x64 cell, so halign and
    // valign actually centre the icon. A Box would allocate only the image's
    // natural width and leave it pinned to the left.
    auto *tile = Gtk::make_managed<Gtk::Overlay>();
    tile->add_css_class("item-icon");
    tile->set_size_request(64, 64);
    tile->set_valign(Gtk::Align::START);
    tile->set_halign(Gtk::Align::START);

    m_backdrop.set_content_width(64);
    m_backdrop.set_content_height(64);
    m_backdrop.set_size_request(64, 64);
    m_backdrop.set_halign(Gtk::Align::CENTER);
    m_backdrop.set_valign(Gtk::Align::CENTER);
    m_backdrop.set_visible(false);
    m_backdrop.set_draw_func([](const Cairo::RefPtr<Cairo::Context> &, int, int) {});
    tile->set_child(m_backdrop);

    m_glyph.set_pixel_size(32);
    m_glyph.set_halign(Gtk::Align::CENTER);
    m_glyph.set_valign(Gtk::Align::CENTER);
    m_glyph.add_css_class("item-glyph");
    m_glyph.set_visible(false);
    tile->add_overlay(m_glyph);

    m_icon.set_pixel_size(64);
    m_icon.set_halign(Gtk::Align::CENTER);
    m_icon.set_valign(Gtk::Align::CENTER);
    m_icon.set_visible(false);
    tile->add_overlay(m_icon);

    top->append(*tile);

    auto *text = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 3);
    text->set_hexpand(true);
    text->set_valign(Gtk::Align::START);

    m_title.add_css_class("item-title");
    m_title.set_xalign(0.0f);
    m_title.set_ellipsize(Pango::EllipsizeMode::END);
    text->append(m_title);

    m_description.add_css_class("muted");
    m_description.add_css_class("small");
    m_description.set_xalign(0.0f);
    m_description.set_wrap(true);
    m_description.set_wrap_mode(Pango::WrapMode::WORD_CHAR);
    m_description.set_lines(3);
    m_description.set_ellipsize(Pango::EllipsizeMode::END);
    text->append(m_description);

    m_chip.add_css_class("lo-chip");
    m_chip.set_halign(Gtk::Align::START);
    m_chip.set_visible(false);

    m_choiceChip.set_text("LGL'S CHOICE");
    m_choiceChip.add_css_class("lo-chip");
    m_choiceChip.add_css_class("accent");
    m_choiceChip.set_halign(Gtk::Align::START);
    m_choiceChip.set_visible(false);

    m_sourceChip.add_css_class("lo-chip");
    m_sourceChip.add_css_class("source");
    m_sourceChip.set_halign(Gtk::Align::START);
    m_sourceChip.set_visible(false);

    // A FlowBox, so three pills wrap instead of widening the card.
    auto *chips = Gtk::make_managed<Gtk::FlowBox>();
    chips->set_selection_mode(Gtk::SelectionMode::NONE);
    chips->set_min_children_per_line(1);
    chips->set_max_children_per_line(3);
    chips->set_column_spacing(6);
    chips->set_row_spacing(4);
    chips->set_halign(Gtk::Align::START);
    chips->set_valign(Gtk::Align::START);
    chips->set_can_focus(false);
    chips->set_can_target(false);
    chips->add_css_class("chip-row");
    chips->set_margin_top(4);
    chips->append(m_sourceChip);
    chips->append(m_choiceChip);
    chips->append(m_chip);
    text->append(*chips);

    top->append(*text);
    append(*top);
}

void ItemCard::setCommand(const std::string &command,
                          const std::function<void(const std::string &, bool)> &notify)
{
    // Full card width, below the icon and text, so the command does not wrap
    // inside a narrow column.
    auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    row->set_margin_top(10);
    row->add_css_class("item-command-box");

    auto *label = Gtk::make_managed<Gtk::Label>(command);
    label->set_xalign(0.0f);
    label->set_hexpand(true);
    label->set_selectable(true);
    label->set_wrap(true);
    label->set_wrap_mode(Pango::WrapMode::CHAR);
    label->add_css_class("mono");
    label->add_css_class("small");
    label->add_css_class("item-command");
    row->append(*label);

    auto *copy = Gtk::make_managed<Gtk::Button>("Copy");
    copy->add_css_class("lo-small");
    copy->set_valign(Gtk::Align::CENTER);
    copy->signal_clicked().connect([label, notify] {
        auto *widget = label;
        widget->get_clipboard()->set_text(label->get_text());
        if (notify)
            notify("Command copied to the clipboard.", true);
    });
    row->append(*copy);

    append(*row);
}

void ItemCard::configure(const catalog::Item &item, const std::string &source,
                         const std::string &glyphOverride, const bool lglChoice)
{
    m_key = item.key;
    m_label = item.label;
    m_source = source;
    m_glyphOverride = glyphOverride;
    m_lglChoice = lglChoice;

    const std::string title = item.label;
    m_title.set_text(title);
    m_title.set_tooltip_text(source.empty() ? title : title + " - " + source);
    m_choiceChip.set_visible(lglChoice);

    std::string sourceText;
    for (const char character : source)
        sourceText += static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    m_sourceChip.set_text(sourceText);
    m_sourceChip.set_visible(!sourceText.empty());
    m_sourceChip.set_tooltip_text("Source: " + source);
    m_description.set_text(item.description);
    m_description.set_tooltip_text(item.description);
    m_description.set_visible(!item.description.empty());
    if (!item.showBadge)
        m_chip.set_visible(false);
    applyIcon(item.key, item.label, glyphOverride);
}

void ItemCard::refreshIcon()
{
    applyIcon(m_key, m_label, m_glyphOverride);
}

void ItemCard::applyIcon(const std::string &key, const std::string &,
                         const std::string &glyphOverride)
{
    // Bundled artwork first: AppImages are not in the icon theme.
    if (const auto resource = catalog::iconResourceFor(key); !resource.empty()) {
        try {
            m_icon.set(Gdk::Texture::create_from_resource(resource));
            m_icon.set_pixel_size(64);
            m_icon.set_visible(true);
            m_backdrop.set_visible(false);
            m_glyph.set_visible(false);
            return;
        } catch (const Glib::Error &) {
            // Fall through to the theme and the generated tile.
        }
    }

    const auto theme = Gtk::IconTheme::get_for_display(Gdk::Display::get_default());
    if (theme) {
        ensureBundledIcons(theme);
        for (const auto &name : catalog::iconNamesFor(key)) {
            if (!theme->has_icon(name))
                continue;

            if (auto texture = iconTexture(name)) {
                m_icon.set(texture);
            } else {
                // Built-in theme icons have no file to crop; use them as they are.
                m_icon.set_from_icon_name(name);
            }
            m_icon.set_pixel_size(64);
            m_icon.set_visible(true);
            m_backdrop.set_visible(false);
            m_glyph.set_visible(false);
            return;
        }
    }

    // No artwork installed: generate a tile in the package's colour with a
    // category glyph on top.
    m_icon.set_visible(false);

    // ArtCraft's own mark may not be redistributed, so its card carries a
    // monogram instead of the category glyph.
    const std::string letter = key == "comms/artcraft" ? "A" : std::string{};
    m_backdrop.set_draw_func(
        [key, letter](const Cairo::RefPtr<Cairo::Context> &context, const int width,
                      const int height) {
            drawGeneratedTile(context->cobj(), width, height, key, letter);
        });
    m_backdrop.queue_draw();
    m_backdrop.set_visible(true);

    if (!letter.empty()) {
        m_glyph.set_visible(false);
        return;
    }

    std::string glyph = glyphOverride.empty() ? glyphFor(key) : glyphOverride;
    if (theme && !theme->has_icon(glyph))
        glyph = "package-x-generic-symbolic";
    if (theme && theme->has_icon(glyph)) {
        m_glyph.set_from_icon_name(glyph);
        m_glyph.set_pixel_size(32);
        m_glyph.set_visible(true);
    } else {
        m_glyph.set_visible(false);
    }
}

void ItemCard::setInstalled(const std::optional<bool> installed)
{
    for (const char *name : {"positive", "neutral"})
        m_chip.remove_css_class(name);

    if (!installed) {
        m_chip.set_visible(false);
        return;
    }

    m_chip.set_text(*installed ? "INSTALLED" : "NOT INSTALLED");
    m_chip.add_css_class(*installed ? "positive" : "neutral");
    m_chip.set_visible(true);
}

} // namespace loadout
