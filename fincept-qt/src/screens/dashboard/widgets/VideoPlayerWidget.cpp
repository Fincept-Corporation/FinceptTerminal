#include "screens/dashboard/widgets/VideoPlayerWidget.h"

#include "ui/theme/Theme.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

#ifdef HAS_QT_MULTIMEDIA
#include <QAudioOutput>
#endif

namespace fincept::screens::widgets {

// ── Preset channels ─────────────────────────────────────────────────────────

struct PresetChannel {
    const char* name;
    const char* stream_url;   // direct HLS / MP4 stream
    const char* fallback_url; // YouTube fallback if stream fails
    const char* description;
    const char* accent;
};

static const PresetChannel kPresets[] = {
    {"Bloomberg TV",
     "https://www.youtube.com/watch?v=dp8PhLsUcFE",
     "https://www.youtube.com/watch?v=dp8PhLsUcFE",
     "Global business & markets", "#9D4EDD"},
    {"CNBC Live",
     "https://www.youtube.com/watch?v=9ApRjK3RVSY",
     "https://www.youtube.com/watch?v=9ApRjK3RVSY",
     "US market coverage", "#2563eb"},
    {"Reuters TV",
     "https://www.youtube.com/watch?v=vAvNMaWlkrc",
     "https://www.youtube.com/watch?v=vAvNMaWlkrc",
     "World news & finance", "#d97706"},
    {"Yahoo Finance",
     "https://www.youtube.com/watch?v=WUm7b7GiTFM",
     "https://www.youtube.com/watch?v=WUm7b7GiTFM",
     "Market analysis", "#16a34a"},
    {"Fox Business",
     "https://www.youtube.com/watch?v=6IlWB5R_Oso",
     "https://www.youtube.com/watch?v=6IlWB5R_Oso",
     "Business & economy", "#dc2626"},
    {"NDTV Profit",
     "https://www.youtube.com/watch?v=QFIn0MQDXuA",
     "https://www.youtube.com/watch?v=QFIn0MQDXuA",
     "India business & markets", "#0891b2"},
};

static constexpr int kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

// ─────────────────────────────────────────────────────────────────────────────

VideoPlayerWidget::VideoPlayerWidget(QWidget* parent)
    : BaseWidget("LIVE TV / STREAMS", parent, "#9D4EDD") {

    stack_ = new QStackedWidget;
    stack_->setStyleSheet("background: transparent;");
    content_layout()->addWidget(stack_, 1);

    build_channel_list();
    build_player_view();

    stack_->setCurrentIndex(0); // show channel list
}

// ── Page 0: Channel list ────────────────────────────────────────────────────

void VideoPlayerWidget::build_channel_list() {
    list_page_ = new QWidget;
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 5px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #222; border-radius: 2px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    auto* container = new QWidget;
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(6, 6, 6, 6);
    vl->setSpacing(3);

    // Section header
    auto* header = new QLabel("FINANCIAL TV");
    header->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.5px; "
                "background: transparent; padding: 2px 0;")
            .arg(ui::colors::TEXT_TERTIARY));
    vl->addWidget(header);

    // Preset rows
    for (int i = 0; i < kPresetCount; ++i) {
        const auto& ch = kPresets[i];

        auto* row = new QPushButton;
        row->setCursor(Qt::PointingHandCursor);
        row->setFixedHeight(32);
        row->setStyleSheet(
            QString("QPushButton { background: %1; border: 1px solid %2; border-radius: 2px; "
                    "text-align: left; padding: 0 8px; }"
                    "QPushButton:hover { background: %3; border-color: %4; }")
                .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM,
                     ui::colors::BG_HOVER, ch.accent));

        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 0, 8, 0);
        rl->setSpacing(8);

        // Color dot
        auto* dot = new QLabel;
        dot->setFixedSize(6, 6);
        dot->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(ch.accent));
        rl->addWidget(dot);

        // Play icon
        auto* play = new QLabel(QChar(0x25B6)); // ▶
        play->setStyleSheet(
            QString("color: %1; font-size: 10px; background: transparent;").arg(ch.accent));
        rl->addWidget(play);

        // Name
        auto* name = new QLabel(ch.name);
        name->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                .arg(ui::colors::TEXT_PRIMARY));
        rl->addWidget(name);

        // Description
        auto* desc = new QLabel(ch.description);
        desc->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;")
                .arg(ui::colors::TEXT_TERTIARY));
        rl->addWidget(desc, 1);

        connect(row, &QPushButton::clicked, this, [this, i]() { play_preset(i); });
        vl->addWidget(row);
    }

    // Separator
    auto* sep = new QLabel;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM));
    vl->addSpacing(4);
    vl->addWidget(sep);
    vl->addSpacing(4);

    // Custom URL
    auto* custom_header = new QLabel("CUSTOM STREAM");
    custom_header->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.5px; "
                "background: transparent;")
            .arg(ui::colors::TEXT_TERTIARY));
    vl->addWidget(custom_header);

    auto* input_row = new QWidget;
    input_row->setStyleSheet("background: transparent;");
    auto* irl = new QHBoxLayout(input_row);
    irl->setContentsMargins(0, 2, 0, 0);
    irl->setSpacing(4);

    url_input_ = new QLineEdit;
    url_input_->setPlaceholderText("Paste HLS (.m3u8) / MP4 / stream URL...");
    url_input_->setStyleSheet(
        QString("QLineEdit { background: %1; color: %2; border: 1px solid %3; "
                "border-radius: 2px; padding: 4px 8px; font-size: 10px; }"
                "QLineEdit:focus { border-color: %4; }")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY,
                 ui::colors::BORDER_DIM, "#9D4EDD"));
    connect(url_input_, &QLineEdit::returnPressed, this, &VideoPlayerWidget::play_custom_url);
    irl->addWidget(url_input_, 1);

    auto* open_btn = new QPushButton("PLAY");
    open_btn->setFixedWidth(50);
    open_btn->setCursor(Qt::PointingHandCursor);
    open_btn->setStyleSheet(
        QString("QPushButton { background: #9D4EDD; color: %1; border: none; border-radius: 2px; "
                "font-size: 9px; font-weight: bold; padding: 5px; }"
                "QPushButton:hover { background: #7C3AED; }")
            .arg(ui::colors::TEXT_PRIMARY));
    connect(open_btn, &QPushButton::clicked, this, &VideoPlayerWidget::play_custom_url);
    irl->addWidget(open_btn);

    vl->addWidget(input_row);

#ifdef HAS_QT_MULTIMEDIA
    auto* helper = new QLabel("Plays inline. Supports HLS (.m3u8), MP4, and direct streams.");
#else
    auto* helper = new QLabel("Opens in browser. Build with Qt Multimedia for inline playback.");
#endif
    helper->setStyleSheet(
        QString("color: %1; font-size: 8px; background: transparent;")
            .arg(ui::colors::TEXT_DIM));
    vl->addWidget(helper);

    vl->addStretch();
    scroll->setWidget(container);

    auto* page_layout = new QVBoxLayout(list_page_);
    page_layout->setContentsMargins(0, 0, 0, 0);
    page_layout->addWidget(scroll);

    stack_->addWidget(list_page_);
}

// ── Page 1: Player view ─────────────────────────────────────────────────────

void VideoPlayerWidget::build_player_view() {
    player_page_ = new QWidget;
    auto* vl = new QVBoxLayout(player_page_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

#ifdef HAS_QT_MULTIMEDIA
    // Video surface
    video_widget_ = new QVideoWidget;
    video_widget_->setStyleSheet("background: #000;");
    vl->addWidget(video_widget_, 1);

    // Media player
    player_ = new QMediaPlayer(this);
    auto* audio = new QAudioOutput(this);
    audio->setVolume(0.5f);
    player_->setAudioOutput(audio);
    player_->setVideoOutput(video_widget_);
#else
    // Fallback: show message
    auto* placeholder = new QLabel("Video opened in browser");
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(
        QString("color: %1; font-size: 12px; background: #000;").arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(placeholder, 1);
#endif

    // Bottom control bar
    auto* controls = new QWidget;
    controls->setFixedHeight(28);
    controls->setStyleSheet(
        QString("background: %1; border-top: 1px solid %2;")
            .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    auto* cl = new QHBoxLayout(controls);
    cl->setContentsMargins(8, 0, 8, 0);
    cl->setSpacing(8);

    // Now playing label
    now_playing_ = new QLabel("—");
    now_playing_->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
            .arg("#9D4EDD"));
    cl->addWidget(now_playing_, 1);

    // Stop / back button
    auto* stop_btn = new QPushButton(QString(QChar(0x25A0)) + " STOP"); // ■ STOP
    stop_btn->setCursor(Qt::PointingHandCursor);
    stop_btn->setFixedHeight(20);
    stop_btn->setStyleSheet(
        QString("QPushButton { background: %1; border: 1px solid %2; color: %3; "
                "font-size: 9px; font-weight: bold; padding: 0 10px; border-radius: 2px; }"
                "QPushButton:hover { background: %4; color: %5; }")
            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM,
                 ui::colors::TEXT_SECONDARY, ui::colors::NEGATIVE, ui::colors::TEXT_PRIMARY));
    connect(stop_btn, &QPushButton::clicked, this, &VideoPlayerWidget::stop_playback);
    cl->addWidget(stop_btn);

    vl->addWidget(controls);

    stack_->addWidget(player_page_);
}

// ── Actions ─────────────────────────────────────────────────────────────────

void VideoPlayerWidget::play_preset(int index) {
    if (index < 0 || index >= kPresetCount)
        return;
    const auto& ch = kPresets[index];
    play_url(QString(ch.stream_url), QString(ch.name));
}

void VideoPlayerWidget::play_custom_url() {
    QString url = url_input_->text().trimmed();
    if (url.isEmpty())
        return;
    if (!url.startsWith("http://") && !url.startsWith("https://"))
        url.prepend("https://");
    play_url(url, "Custom Stream");
}

void VideoPlayerWidget::play_url(const QString& url, const QString& title) {
    now_playing_->setText(QString(QChar(0x25B6)) + " " + title); // ▶ title
    set_title("LIVE TV — " + title.toUpper());

#ifdef HAS_QT_MULTIMEDIA
    player_->setSource(QUrl(url));
    player_->play();
    stack_->setCurrentIndex(1); // show player
#else
    // Fallback: open in browser
    QDesktopServices::openUrl(QUrl(url));
    stack_->setCurrentIndex(1);
#endif
}

void VideoPlayerWidget::stop_playback() {
#ifdef HAS_QT_MULTIMEDIA
    player_->stop();
    player_->setSource(QUrl());
#endif
    set_title("LIVE TV / STREAMS");
    stack_->setCurrentIndex(0); // back to channel list
}

} // namespace fincept::screens::widgets
