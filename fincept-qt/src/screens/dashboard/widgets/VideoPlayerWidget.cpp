#include "screens/dashboard/widgets/VideoPlayerWidget.h"

#include "ui/theme/Theme.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

// ── Preset channels ─────────────────────────────────────────────────────────

struct PresetChannel {
    const char* name;
    const char* stream_url;
    const char* description;
    const char* accent;
};

static const PresetChannel kPresets[] = {
    {"Bloomberg TV",     "https://www.youtube.com/watch?v=iEpJwprxDdk", "Global business & markets", "#9D4EDD"},
    {"CNBC Live",        "https://www.youtube.com/watch?v=rGgBsS3cm7E", "US market coverage",        "#2563eb"},
    {"Yahoo Finance",    "https://www.youtube.com/@YahooFinance/live",   "Market analysis",           "#16a34a"},
    {"NDTV Profit",      "https://www.youtube.com/watch?v=kf8tVmwfUHI", "India business & markets",  "#0891b2"},
    {"Al Jazeera",       "https://www.youtube.com/watch?v=gCNeDWCI0vo", "World news & finance",      "#d97706"},
};

static constexpr int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

static bool is_youtube_url(const QString& url) {
    return url.contains("youtube.com/watch") || url.contains("youtu.be/")
        || url.contains("youtube.com/live")  || url.contains("youtube.com/@");
}

// ─────────────────────────────────────────────────────────────────────────────

VideoPlayerWidget::VideoPlayerWidget(QWidget* parent) : BaseWidget("LIVE TV / STREAMS", parent, "#9D4EDD") {

    stack_ = new QStackedWidget;
    stack_->setStyleSheet("background: transparent;");
    content_layout()->addWidget(stack_, 1);

    build_channel_list();
    build_player_view();

    connect(this, &BaseWidget::refresh_requested, this, &VideoPlayerWidget::refresh_data);

    stack_->setCurrentIndex(0);
}

// ── Page 0: Channel list ─────────────────────────────────────────────────────

void VideoPlayerWidget::build_channel_list() {
    list_page_ = new QWidget;
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }"
                          "QScrollBar:vertical { width: 5px; background: transparent; }"
                          "QScrollBar::handle:vertical { background: #222; border-radius: 2px; }"
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    auto* container = new QWidget;
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(6, 6, 6, 6);
    vl->setSpacing(3);

    auto* header = new QLabel("FINANCIAL TV");
    header->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.5px; "
                                  "background: transparent; padding: 2px 0;")
                              .arg(ui::colors::TEXT_TERTIARY));
    vl->addWidget(header);

    for (int i = 0; i < kPresetCount; ++i) {
        const auto& ch = kPresets[i];

        auto* row = new QPushButton;
        row->setCursor(Qt::PointingHandCursor);
        row->setFixedHeight(32);
        row->setStyleSheet(QString("QPushButton { background: %1; border: 1px solid %2; border-radius: 2px; "
                                   "text-align: left; padding: 0 8px; }"
                                   "QPushButton:hover { background: %3; border-color: %4; }")
                               .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM, ui::colors::BG_HOVER, ch.accent));

        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 0, 8, 0);
        rl->setSpacing(8);

        auto* dot = new QLabel;
        dot->setFixedSize(6, 6);
        dot->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(ch.accent));
        rl->addWidget(dot);

        auto* play = new QLabel(QChar(0x25B6));
        play->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(ch.accent));
        rl->addWidget(play);

        auto* name = new QLabel(ch.name);
        name->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                .arg(ui::colors::TEXT_PRIMARY));
        rl->addWidget(name);

        auto* desc = new QLabel(ch.description);
        desc->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY));
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
    custom_header->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.5px; "
                                         "background: transparent;")
                                     .arg(ui::colors::TEXT_TERTIARY));
    vl->addWidget(custom_header);

    auto* input_row = new QWidget;
    input_row->setStyleSheet("background: transparent;");
    auto* irl = new QHBoxLayout(input_row);
    irl->setContentsMargins(0, 2, 0, 0);
    irl->setSpacing(4);

    url_input_ = new QLineEdit;
    url_input_->setPlaceholderText("YouTube URL, HLS (.m3u8), MP4, or direct stream...");
    url_input_->setStyleSheet(
        QString("QLineEdit { background: %1; color: %2; border: 1px solid %3; "
                "border-radius: 2px; padding: 4px 8px; font-size: 10px; }"
                "QLineEdit:focus { border-color: %4; }")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM, "#9D4EDD"));
    connect(url_input_, &QLineEdit::returnPressed, this, &VideoPlayerWidget::play_custom_url);
    irl->addWidget(url_input_, 1);

    auto* open_btn = new QPushButton("PLAY");
    open_btn->setFixedWidth(50);
    open_btn->setCursor(Qt::PointingHandCursor);
    open_btn->setStyleSheet(QString("QPushButton { background: #9D4EDD; color: %1; border: none; border-radius: 2px; "
                                    "font-size: 9px; font-weight: bold; padding: 5px; }"
                                    "QPushButton:hover { background: #7C3AED; }")
                                .arg(ui::colors::TEXT_PRIMARY));
    connect(open_btn, &QPushButton::clicked, this, &VideoPlayerWidget::play_custom_url);
    irl->addWidget(open_btn);

    vl->addWidget(input_row);

    auto* helper = new QLabel("YouTube streams resolved via yt-dlp and played inline.");
    helper->setStyleSheet(QString("color: %1; font-size: 8px; background: transparent;").arg(ui::colors::TEXT_DIM));
    vl->addWidget(helper);

    vl->addStretch();
    scroll->setWidget(container);

    auto* page_layout = new QVBoxLayout(list_page_);
    page_layout->setContentsMargins(0, 0, 0, 0);
    page_layout->addWidget(scroll);

    stack_->addWidget(list_page_);
}

// ── Page 1: Player view ──────────────────────────────────────────────────────

void VideoPlayerWidget::build_player_view() {
    player_page_ = new QWidget;
    auto* vl = new QVBoxLayout(player_page_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

#ifdef HAS_QT_MULTIMEDIA
    video_widget_ = new QVideoWidget;
    video_widget_->setStyleSheet("background: #000;");
    vl->addWidget(video_widget_, 1);

    player_ = new QMediaPlayer(this);
    audio_output_ = new QAudioOutput(this);
    audio_output_->setVolume(0.5f);
    player_->setAudioOutput(audio_output_);
    player_->setVideoOutput(video_widget_);
#else
    auto* placeholder = new QLabel("Qt Multimedia not available.\nBuild with Qt6 Multimedia for inline playback.");
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(
        QString("color: %1; font-size: 11px; background: #000;").arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(placeholder, 1);
#endif

    // Status overlay (loading / error)
    status_label_ = new QLabel;
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setFixedHeight(20);
    status_label_->setStyleSheet(QString("color: %1; font-size: 9px; background: %2; padding: 0 8px;")
                                     .arg(ui::colors::TEXT_SECONDARY, ui::colors::BG_RAISED));
    status_label_->hide();
    vl->addWidget(status_label_);

    // Control bar
    auto* controls = new QWidget;
    controls->setFixedHeight(28);
    controls->setStyleSheet(
        QString("background: %1; border-top: 1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    auto* cl = new QHBoxLayout(controls);
    cl->setContentsMargins(8, 0, 8, 0);
    cl->setSpacing(8);

    now_playing_ = new QLabel("—");
    now_playing_->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;").arg("#9D4EDD"));
    cl->addWidget(now_playing_, 1);

    auto* stop_btn = new QPushButton(QString(QChar(0x25A0)) + " STOP");
    stop_btn->setCursor(Qt::PointingHandCursor);
    stop_btn->setFixedHeight(20);
    stop_btn->setStyleSheet(QString("QPushButton { background: %1; border: 1px solid %2; color: %3; "
                                    "font-size: 9px; font-weight: bold; padding: 0 10px; border-radius: 2px; }"
                                    "QPushButton:hover { background: %4; color: %5; }")
                                .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM, ui::colors::TEXT_SECONDARY,
                                     ui::colors::NEGATIVE, ui::colors::TEXT_PRIMARY));
    connect(stop_btn, &QPushButton::clicked, this, &VideoPlayerWidget::stop_playback);
    cl->addWidget(stop_btn);

    vl->addWidget(controls);

    stack_->addWidget(player_page_);
}

// ── Actions ──────────────────────────────────────────────────────────────────

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
    current_url_ = url;
    current_title_ = title;
    pending_title_ = title;
    now_playing_->setText(QString(QChar(0x25B6)) + " " + title);
    set_title("LIVE TV — " + title.toUpper());

#ifdef HAS_QT_MULTIMEDIA
    if (is_youtube_url(url)) {
        resolve_youtube_and_play(url, title);
    } else {
        play_direct(url);
    }
#else
    QDesktopServices::openUrl(QUrl(url));
#endif
}

void VideoPlayerWidget::refresh_data() {
    if (current_url_.isEmpty())
        return;

    play_url(current_url_, current_title_.isEmpty() ? "Custom Stream" : current_title_);
}

void VideoPlayerWidget::resolve_youtube_and_play(const QString& youtube_url, const QString& title) {
    set_loading(true);
    stack_->setCurrentIndex(1);

    // Use yt-dlp to get the best direct stream URL (no downloading, just URL extraction)
    auto* proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, &VideoPlayerWidget::on_ytdlp_finished);
    connect(proc, &QProcess::finished, proc, &QProcess::deleteLater);

    // -f bestvideo+bestaudio/best: best quality with audio
    // --no-playlist: single video only
    // -g: print URL only, don't download
    proc->start("yt-dlp",
                {"-f", "bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best", "--no-playlist", "-g", youtube_url});
}

void VideoPlayerWidget::on_ytdlp_finished(int exit_code, QProcess::ExitStatus /*status*/) {
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc)
        return;

    if (exit_code != 0) {
        const QString err = proc->readAllStandardError().trimmed();
        set_loading(false);
        status_label_->setText("yt-dlp error: " + err.left(120));
        status_label_->show();
        return;
    }

    // yt-dlp may return two lines when bestvideo+bestaudio are separate tracks
    // Qt Multimedia can only handle one URL, so take the first (video) line
    const QString output = proc->readAllStandardOutput().trimmed();
    const QString stream_url = output.split('\n').first().trimmed();

    if (stream_url.isEmpty()) {
        set_loading(false);
        status_label_->setText("Could not extract stream URL.");
        status_label_->show();
        return;
    }

    play_direct(stream_url);
}

void VideoPlayerWidget::play_direct(const QString& stream_url) {
#ifdef HAS_QT_MULTIMEDIA
    set_loading(false);
    status_label_->hide();
    player_->setSource(QUrl(stream_url));
    player_->play();
    stack_->setCurrentIndex(1);
#else
    Q_UNUSED(stream_url)
#endif
}

void VideoPlayerWidget::stop_playback() {
#ifdef HAS_QT_MULTIMEDIA
    player_->stop();
    player_->setSource(QUrl());
#endif
    current_url_.clear();
    current_title_.clear();
    set_loading(false);
    status_label_->hide();
    set_title("LIVE TV / STREAMS");
    stack_->setCurrentIndex(0);
}

void VideoPlayerWidget::set_loading(bool loading) {
    if (loading) {
        status_label_->setText("Resolving stream via yt-dlp...");
        status_label_->show();
    } else {
        status_label_->hide();
    }
}

} // namespace fincept::screens::widgets

