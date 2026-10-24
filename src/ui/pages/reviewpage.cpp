#include "ui/pages/reviewpage.h"

#include "core/catalog.h"
#include "core/plan.h"

#include <gtkmm/label.h>

#include <iomanip>
#include <sstream>

namespace loadout {
namespace {

void clear(Gtk::Box &box)
{
    while (auto *child = box.get_first_child())
        box.remove(*child);
}

void addLabel(Gtk::Box &box, const std::string &text, const char *cssClass)
{
    auto *label = Gtk::make_managed<Gtk::Label>(text);
    label->set_wrap(true);
    label->set_xalign(0.0f);
    if (cssClass)
        label->add_css_class(cssClass);
    box.append(*label);
}

std::string gigabytes(const int megabytes)
{
    std::ostringstream text;
    text << std::fixed << std::setprecision(1) << (static_cast<double>(megabytes) / 1024.0);
    return text.str();
}

} // namespace

ReviewPage::ReviewPage(Selection &selection, const SystemInfo &system)
    : m_selection(selection)
    , m_system(system)
{
    m_diskBanner.add_css_class("disk-banner");
    append(m_diskBanner);

    m_proceed.add_css_class("lo-small");
    m_proceed.set_halign(Gtk::Align::START);
    m_proceed.set_visible(false);
    m_proceed.signal_clicked().connect([this] {
        m_proceeded = true;
        m_proceed.set_visible(false);
        stateChanged();
        rebuild();
    });
    append(m_proceed);

    m_requirements.add_css_class("disk-banner");
    m_requirements.add_css_class("neutral");
    m_requirements.set_visible(false);
    append(m_requirements);

    auto *card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    card->add_css_class("lo-card");
    card->add_css_class("flush");
    auto *heading = Gtk::make_managed<Gtk::Label>("STEPS THAT WILL RUN");
    heading->set_xalign(0.0f);
    heading->add_css_class("section-label");
    heading->add_css_class("card-head");
    card->append(*heading);
    m_steps.set_selection_mode(Gtk::SelectionMode::NONE);
    m_steps.add_css_class("step-list");
    card->append(m_steps);
    append(*card);
}

void ReviewPage::onShown()
{
    rebuild();
}

void ReviewPage::rebuild()
{
    const auto plan = buildPlan(m_selection, m_system, "");
    const int needed = estimateDiskMb(m_selection);
    const int neededWithBuffer = needed + (needed * 15 / 100) + 500;
    const int available = availableDiskMb();

    clear(m_diskBanner);
    for (const char *name : {"neutral", "negative"})
        m_diskBanner.remove_css_class(name);

    if (available < 0) {
        addLabel(m_diskBanner,
                 "Estimated disk space required: about " + gigabytes(needed) +
                     " GB. Available space could not be determined, so make sure you have "
                     "enough free space before continuing.",
                 nullptr);
        m_diskBanner.add_css_class("neutral");
        m_diskOk = true;
    } else if (!m_proceeded && available < neededWithBuffer) {
        addLabel(m_diskBanner,
                 "Not enough disk space. This selection needs about " +
                     gigabytes(neededWithBuffer) + " GB including package-manager overhead, and " +
                     gigabytes(available) + " GB is available.",
                 nullptr);
        m_diskBanner.add_css_class("negative");
        m_diskOk = false;
        m_proceed.set_visible(true);
    } else {
        addLabel(m_diskBanner,
                 "Disk space: about " + gigabytes(needed) + " GB required, " +
                     gigabytes(available) + " GB available.",
                 nullptr);
        m_diskOk = true;
    }

    buildRequirements();

    while (auto *child = m_steps.get_first_child())
        m_steps.remove(*child);

    if (plan.size() <= 1) {
        auto *row = Gtk::make_managed<Gtk::ListBoxRow>();
        row->set_selectable(false);
        auto *label = Gtk::make_managed<Gtk::Label>(
            "Nothing selected beyond the bootstrap tools. Go back and select items to install.");
        label->set_xalign(0.0f);
        label->set_wrap(true);
        label->add_css_class("muted");
        row->set_child(*label);
        m_steps.append(*row);
    }

    int number = 1;
    for (const auto &step : plan) {
        auto *row = Gtk::make_managed<Gtk::ListBoxRow>();
        row->set_selectable(false);
        auto *line = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
        auto *index = Gtk::make_managed<Gtk::Label>(std::to_string(number) + ".");
        index->set_size_request(28, -1);
        index->set_xalign(1.0f);
        index->add_css_class("mono");
        index->add_css_class("muted");
        line->append(*index);
        auto *description = Gtk::make_managed<Gtk::Label>(step.description);
        description->set_xalign(0.0f);
        description->set_wrap(true);
        description->set_hexpand(true);
        line->append(*description);
        row->set_child(*line);
        m_steps.append(*row);
        ++number;
    }

    stateChanged();
}

void ReviewPage::buildRequirements()
{
    clear(m_requirements);

    int version = 44;
    try {
        version = std::stoi(m_system.fedoraVersion);
    } catch (const std::exception &) {
    }

    const auto unmet = catalog::unmetRequirements(m_selection, version);
    if (unmet.empty()) {
        m_requirements.set_visible(false);
        return;
    }

    auto *heading = Gtk::make_managed<Gtk::Label>("UNMET REQUIREMENTS");
    heading->set_xalign(0.0f);
    heading->add_css_class("section-label");
    m_requirements.append(*heading);

    for (const auto &entry : unmet)
        addLabel(m_requirements,
                 catalog::labelFor(entry.itemKey) + " needs " +
                     catalog::labelFor(entry.requiredKey) + ", which is not selected.",
                 nullptr);
    m_requirements.set_visible(true);
}

} // namespace loadout
