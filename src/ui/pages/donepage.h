#pragma once

#include "ui/helpersession.h"
#include "ui/page.h"
#include "ui/pages/installpage.h"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/textview.h>

namespace loadout {

class DonePage final : public Page {
public:
    explicit DonePage(Notify notify);

    void setSummary(const InstallSummary &summary);
    void onShown() override;

private:
    void reboot();
    void copy(const std::string &text, const std::string &what);

    Notify m_notify;
    HelperSession m_session;
    InstallSummary m_summary;

    Gtk::Label m_summaryLabel;
    Gtk::Box m_detailCard{Gtk::Orientation::VERTICAL, 6};
    Gtk::TextView m_detail;
    Gtk::Button m_copyErrors{"Copy Failed Steps"};
    Gtk::Button m_copyLog{"Copy Full Log"};
    Gtk::Button m_rebootButton{"Reboot Now"};
};

} // namespace loadout
