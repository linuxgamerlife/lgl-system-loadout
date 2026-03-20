#include <QPalette>
#include "multimediapage.h"
#include "../mainwizard.h"
#include "../pagehelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QPushButton>
#include <QApplication>

MultimediaPage::MultimediaPage(MainWizard *wizard) : QWizardPage(wizard), m_wiz(wizard)
{
    setTitle("Multimedia & Codecs");
    setSubTitle("Video, audio, and codec support. RPM Fusion must be enabled for most of these.");
}

void MultimediaPage::initializePage()
{
    // Rebuild the page each time so Refresh can repopulate the installed-state
    // badges from a clean layout.
    clearWidgetLayout(this);
    m_boxes.clear();

    auto *outer = new QVBoxLayout(this);
    auto toolbarUi = makeSelectionToolbar(this, this,
        [this] { initializePage(); },
        [this] { for (auto *cb : m_boxes) cb->setChecked(true); },
        [this] { for (auto *cb : m_boxes) cb->setChecked(false); });
    m_checkingLabel = toolbarUi.checkingLabel;
    outer->addWidget(toolbarUi.widget);

    auto *scroll = new SmoothScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto *inner = new QWidget; auto *layout = new QVBoxLayout(inner); layout->setSpacing(4);

    const QList<std::tuple<QString,QString,QString,bool>> items = {
        {"ffmpeg",              "ffmpeg (full - swap from ffmpeg-free)",
         "Replaces the restricted ffmpeg-free with the full build including all codecs.",
         false},
        {"gst_bad_free_extras", "GStreamer bad-free-extras",
         "Additional bad-free plugins including extra format support.",
         false},
        {"gst_bad_nonfree",     "GStreamer Ugly Plugins (MP3/DVD)",
         "Non-free GStreamer plugins with additional codec support.",
         false},
        {"vlc",                 "VLC media player",
         "Versatile player that handles virtually any audio or video format.",
         false},
    };

    for (const auto &[key, label, desc, installed] : items) {
        auto *cb = makeItemRow(inner, layout, label, installed);
        layout->addWidget(makeDescLabel(inner, desc)); layout->addSpacing(2);
        m_boxes[key] = cb;
    }
    layout->addStretch();
    scroll->setWidget(inner);
    outer->addWidget(scroll);
    // Run install checks concurrently
    QList<QPair<QString, std::function<bool()>>> _checks;
    _checks.append({"ffmpeg", []{ return isDnfInstalled("ffmpeg"); }});
    _checks.append({"gst_bad_free_extras", []{ return isDnfInstalled("gstreamer1-plugins-bad-free-extras"); }});
    _checks.append({"gst_bad_nonfree", []{ return isDnfInstalled("gstreamer1-plugins-ugly"); }});
    _checks.append({"vlc", []{ return isDnfInstalled("vlc"); }});
    runChecksAsync(this, _checks, [this](QMap<QString,bool> results) {
        applySelectionCheckResults(m_boxes, results, m_checkingLabel);
    });
}

bool MultimediaPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("media/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
