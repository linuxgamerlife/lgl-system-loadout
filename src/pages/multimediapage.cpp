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
    if (layout()) {
        QLayoutItem *i; while ((i = layout()->takeAt(0))) { if (i->widget()) i->widget()->deleteLater(); delete i; }
        delete layout();
    }
    m_boxes.clear();

    auto *outer = new QVBoxLayout(this);
    auto *toolbarWidget = new QWidget;
    auto *toolbar = new QHBoxLayout(toolbarWidget);
    toolbar->setContentsMargins(0,0,0,0);
    toolbar->addStretch();
    auto *allBtn = makeToolbarBtn("Select All");
    auto *noneBtn = makeToolbarBtn("Select None");
    connect(allBtn,  &QPushButton::clicked, this, [this]{ for (auto *cb : m_boxes) cb->setChecked(true); });
    connect(noneBtn, &QPushButton::clicked, this, [this]{ for (auto *cb : m_boxes) cb->setChecked(false); });
    // Create checking label first so the refresh lambda can reference it safely
    m_checkingLabel = new QLabel("  Checking...");
    m_checkingLabel->setStyleSheet("color: palette(highlight); font-style: italic;");
    m_checkingLabel->setVisible(true);
    auto *refreshBtn = makeToolbarBtn("Refresh");
    refreshBtn->setToolTip("Re-check installed status of all items");
    connect(refreshBtn, &QPushButton::clicked, this, [this] {
        initializePage();
    });
    toolbar->addSpacing(8);
    toolbar->addWidget(refreshBtn);
    toolbar->addSpacing(4);
    toolbar->addWidget(m_checkingLabel);
    toolbar->addWidget(allBtn); toolbar->addWidget(noneBtn);
    outer->addWidget(toolbarWidget);

    auto *scroll = new SmoothScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto *inner = new QWidget; auto *layout = new QVBoxLayout(inner); layout->setSpacing(4);

    const QList<std::tuple<QString,QString,QString,bool>> items = {
        {"ffmpeg",              "ffmpeg (full - swap from ffmpeg-free)",
         "Replaces the restricted ffmpeg-free with the full build including all codecs.",
         false},
        {"gst_bad_free",        "GStreamer bad-free",
         "GStreamer plugins not considered good quality but are free software.",
         false},
        {"gst_bad_free_extras", "GStreamer bad-free-extras",
         "Additional bad-free plugins including extra format support.",
         false},
        {"gst_bad_nonfree",     "GStreamer Ugly Plugins (MP3/DVD)",
         "Non-free GStreamer plugins with additional codec support.",
         false},
        {"gst_good",            "GStreamer good",
         "Well-maintained stable GStreamer plugins.",
         false},
        {"gst_good_extras",     "GStreamer good-extras",
         "Extra good plugins with additional format support.",
         false},
        {"gst_base",            "GStreamer base",
         "Core GStreamer plugin set - required by most media applications.",
         false},
        {"gst_libav",           "GStreamer libav",
         "GStreamer bridge to libav/ffmpeg for ffmpeg-based decoding.",
         false},
        {"lame",                "lame + lame-libs",
         "MP3 audio encoder library.",
         false},
        {"vaapi",               "VA-API libraries (ffmpeg-libs, libva, libva-utils)",
         "GPU-accelerated video decode/encode support.",
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
    _checks.append({"gst_bad_free", []{ return isDnfInstalled("gstreamer1-plugins-bad-free"); }});
    _checks.append({"gst_bad_free_extras", []{ return isDnfInstalled("gstreamer1-plugins-bad-free-extras"); }});
    _checks.append({"gst_bad_nonfree", []{ return isDnfInstalled("gstreamer1-plugins-ugly"); }});
    _checks.append({"gst_good", []{ return isDnfInstalled("gstreamer1-plugins-good"); }});
    _checks.append({"gst_good_extras", []{ return isDnfInstalled("gstreamer1-plugins-good-extras"); }});
    _checks.append({"gst_base", []{ return isDnfInstalled("gstreamer1-plugins-base"); }});
    _checks.append({"gst_libav", []{ return (isDnfInstalled("gstreamer1-libav") || isDnfInstalled("gstreamer1-plugin-libav")); }});
    _checks.append({"lame", []{ return isDnfInstalled("lame"); }});
    _checks.append({"vaapi", []{ return isDnfInstalled("libva"); }});
    _checks.append({"vlc", []{ return isDnfInstalled("vlc"); }});
    runChecksAsync(this, _checks, [this](QMap<QString,bool> results) {
        for (auto it = results.constBegin(); it != results.constEnd(); ++it) {
            if (!m_boxes.contains(it.key())) continue;
            auto *row = m_boxes[it.key()]->parentWidget();
            if (!row) continue;
            auto *lbl = row->findChild<QLabel*>();
            if (!lbl) continue;
            lbl->setText(it.value() ? "[Installed]" : "[Not Installed]");
            lbl->setStyleSheet(it.value()
                ? "color: #3db03d; font-weight: bold; font-size: 8pt;"
                : "color: #cc7700; font-weight: bold; font-size: 8pt;");
        }
        if (m_checkingLabel) m_checkingLabel->setVisible(false);
    });
}

bool MultimediaPage::validatePage()
{
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        m_wiz->setOpt(QString("media/%1").arg(it.key()), it.value()->isChecked());
    return true;
}
