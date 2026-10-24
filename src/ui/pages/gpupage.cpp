#include "ui/pages/gpupage.h"

#include "ui/itemcard.h"

#include <gtkmm/button.h>
#include <gtkmm/flowboxchild.h>

#include <exception>

namespace loadout {
namespace {

void addLabel(Gtk::Box &box, const std::string &text, const char *cssClass)
{
    auto *label = Gtk::make_managed<Gtk::Label>(text);
    label->set_wrap(true);
    label->set_xalign(0.0f);
    if (cssClass)
        label->add_css_class(cssClass);
    box.append(*label);
}

} // namespace

GpuPage::GpuPage(Selection &selection, Checker &checker, const SystemInfo &system,
                 std::function<void()> selectionChanged)
    : m_selection(selection)
    , m_checker(checker)
    , m_system(system)
    , m_selectionChanged(std::move(selectionChanged))
{
    buildManufacturerChoice();
    buildAmd();
    buildNvidia();
    buildSkip();

    m_warning.add_css_class("disk-banner");
    m_warning.add_css_class("neutral");
    m_warning.set_visible(false);

    append(m_amdBox);
    append(m_nvidiaBox);
    append(m_skipBox);
    append(m_warning);

    m_checker.signalResult().connect([this](const std::string &key, const bool installed) {
        const auto found = m_cards.find(key);
        if (found != m_cards.end())
            found->second->setInstalled(installed);
    });

    syncFromSelection();
}

void GpuPage::buildManufacturerChoice()
{
    auto *heading = Gtk::make_managed<Gtk::Label>("GPU MANUFACTURER");
    heading->set_xalign(0.0f);
    heading->add_css_class("section-title");
    append(*heading);

    auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 16);
    m_nvidia.set_group(m_amd);
    m_skip.set_group(m_amd);
    row->append(m_amd);
    row->append(m_nvidia);
    row->append(m_skip);
    append(*row);

    for (Gtk::CheckButton *button : {&m_amd, &m_nvidia, &m_skip}) {
        button->signal_toggled().connect([this, button] {
            if (!button->get_active() || m_syncing)
                return;
            if (button == &m_amd)
                m_selection.setChoice("gpu", "amd");
            else if (button == &m_nvidia)
                m_selection.setChoice("gpu", "nvidia");
            else
                m_selection.setChoice("gpu", "skip");
            updateVisibility();
            updateWarnings();
            m_selectionChanged();
            stateChanged();
        });
    }
}

void GpuPage::buildAmd()
{
    auto *recommendation = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    recommendation->add_css_class("lo-card");
    addLabel(*recommendation,
             "Install all of these for the best AMD experience. The full Mesa and Vulkan "
             "stack is required for hardware-accelerated rendering, DXVK and VKD3D gaming, "
             "and GPU-accelerated video decode.",
             "muted");

    auto *toolbar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    toolbar->set_halign(Gtk::Align::END);
    auto *all = Gtk::make_managed<Gtk::Button>("Select All");
    all->add_css_class("lo-small");
    all->signal_clicked().connect([this] {
        for (const auto &[key, card] : m_cards)
            m_selection.set(key, true);
        syncFromSelection();
        m_selectionChanged();
        stateChanged();
    });
    auto *none = Gtk::make_managed<Gtk::Button>("Select None");
    none->add_css_class("lo-small");
    none->signal_clicked().connect([this] {
        for (const auto &[key, card] : m_cards)
            m_selection.set(key, false);
        syncFromSelection();
        m_selectionChanged();
        stateChanged();
    });
    auto *refresh = Gtk::make_managed<Gtk::Button>("Refresh");
    refresh->add_css_class("lo-small");
    refresh->signal_clicked().connect([this] {
        m_requested.clear();
        for (const auto &[key, card] : m_cards)
            card->refreshIcon();
        requestMissingChecks();
    });
    toolbar->append(*refresh);
    toolbar->append(*all);
    toolbar->append(*none);

    m_grid.setActivated([this](Gtk::FlowBoxChild &child) { activate(child); });
    m_grid.signalLeaveLeft().connect([this] { signalFocusSteps().emit(); });

    if (const auto *page = catalog::page("graphics")) {
        for (const auto &section : page->sections) {
            for (const auto &item : section.items) {
                auto *card = Gtk::make_managed<ItemCard>();
                card->configure(item, catalog::sourceFor(item.key), {},
                                catalog::isLglChoice(item.key));
                m_grid.addCard(*card);
                m_cards[item.key] = card;
            }
        }
    }

    m_amdBox.append(*recommendation);
    m_amdBox.append(*toolbar);
    m_amdBox.append(m_grid);
}

void GpuPage::buildNvidia()
{
    addLabel(m_nvidiaBox, "NVIDIA", "section-label");
    addLabel(m_nvidiaBox,
             "This wizard does not automate NVIDIA driver installation, because it needs care "
             "to avoid conflicts with the nouveau driver. After this wizard completes:",
             nullptr);
    addLabel(m_nvidiaBox,
             "1. Enable RPM Fusion NonFree on the Repositories step.\n"
             "2. Run: sudo dnf install akmod-nvidia xorg-x11-drv-nvidia-cuda\n"
             "3. Wait for the kernel module to build; do not reboot immediately.\n"
             "4. Run: modinfo -F version nvidia to confirm the module is built.\n"
             "5. Reboot.",
             "mono");
    addLabel(m_nvidiaBox, "Full instructions: https://rpmfusion.org/Howto/NVIDIA", "muted");
}

void GpuPage::buildSkip()
{
    addLabel(m_skipBox, "No GPU drivers will be installed. You can install drivers manually "
                        "at any time.",
             "muted");
}

void GpuPage::updateVisibility()
{
    const auto &choice = m_selection.choice("gpu");
    m_amdBox.set_visible(choice == "amd");
    m_nvidiaBox.set_visible(choice == "nvidia");
    m_skipBox.set_visible(choice != "amd" && choice != "nvidia");
}

void GpuPage::syncFromSelection()
{
    m_syncing = true;
    if (m_selection.choice("gpu") == "amd")
        m_amd.set_active(true);
    else if (m_selection.choice("gpu") == "nvidia")
        m_nvidia.set_active(true);
    else
        m_skip.set_active(true);
    m_syncing = false;

    for (const auto &[key, card] : m_cards) {
        if (m_selection.get(key))
            card->add_css_class("chosen");
        else
            card->remove_css_class("chosen");
    }

    updateVisibility();
    updateWarnings();
}

void GpuPage::toggle(const std::string &key)
{
    m_selection.set(key, !m_selection.get(key));
    syncFromSelection();
    m_selectionChanged();
    stateChanged();
}

void GpuPage::onShown()
{
    requestMissingChecks();
}

void GpuPage::focusContent()
{
    m_grid.focusFirst();
}

void GpuPage::activate(Gtk::FlowBoxChild &child)
{
    Gtk::Widget *widget = child.get_child();
    for (const auto &[key, card] : m_cards)
        if (static_cast<Gtk::Widget *>(card) == widget) {
            toggle(key);
            return;
        }
}

void GpuPage::requestMissingChecks()
{
    for (const auto &[key, card] : m_cards) {
        if (m_requested.count(key))
            continue;
        const auto check = checkFor(key, m_system);
        m_requested.insert(key);
        m_checker.request(key, check);
    }
}

void GpuPage::updateWarnings()
{
    while (auto *child = m_warning.get_first_child())
        m_warning.remove(*child);

    int version = 44;
    try {
        version = std::stoi(m_system.fedoraVersion);
    } catch (const std::exception &) {
    }

    std::vector<catalog::Unmet> unmet;
    for (const auto &entry : catalog::unmetRequirements(m_selection, version))
        if (m_cards.count(entry.itemKey))
            unmet.push_back(entry);

    if (unmet.empty()) {
        m_warning.set_visible(false);
        return;
    }

    auto *heading = Gtk::make_managed<Gtk::Label>("REQUIREMENTS");
    heading->set_xalign(0.0f);
    heading->add_css_class("section-label");
    m_warning.append(*heading);

    for (const auto &entry : unmet) {
        auto *line = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
        auto *text = Gtk::make_managed<Gtk::Label>(
            catalog::labelFor(entry.itemKey) + " needs " + catalog::labelFor(entry.requiredKey) +
            ", which is not selected.");
        text->set_wrap(true);
        text->set_xalign(0.0f);
        text->set_hexpand(true);
        line->append(*text);

        auto *fix = Gtk::make_managed<Gtk::Button>(
            "Select " + catalog::labelFor(entry.requiredKey));
        fix->add_css_class("lo-small");
        const std::string requiredKey = entry.requiredKey;
        fix->signal_clicked().connect([this, requiredKey] {
            m_selection.set(requiredKey, true);
            m_selectionChanged();
            syncFromSelection();
            stateChanged();
        });
        line->append(*fix);
        m_warning.append(*line);
    }
    m_warning.set_visible(true);
}

} // namespace loadout
