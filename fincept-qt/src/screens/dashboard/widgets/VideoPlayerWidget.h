#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"

#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>

#ifdef HAS_QT_MULTIMEDIA
#    include <QAudioOutput>
#    include <QMediaPlayer>
#    include <QVideoWidget>
#endif

namespace fincept::screens::widgets {

/// Inline video/stream player widget.
/// Plays HLS/MP4 direct streams via Qt Multimedia.
/// For YouTube URLs, uses yt-dlp to extract the direct stream URL first.
class VideoPlayerWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit VideoPlayerWidget(QWidget* parent = nullptr);

  private slots:
    void play_preset(int index);
    void play_custom_url();
    void refresh_data();
    void stop_playback();
    void on_ytdlp_finished(int exit_code, QProcess::ExitStatus status);
    void on_ytdlp_error(QProcess::ProcessError error);

  protected:
    void on_theme_changed() override;

  private:
    void apply_styles();
    void build_channel_list();
    void build_player_view();
    void play_url(const QString& url, const QString& title);
    void resolve_youtube_and_play(const QString& youtube_url, const QString& title);
    QString resolve_ytdlp_program() const;
    void play_direct(const QString& stream_url);
    void set_loading(bool loading);

    QStackedWidget* stack_ = nullptr; // page 0 = channel list, page 1 = player
    QWidget* list_page_ = nullptr;
    QWidget* player_page_ = nullptr;
    QLineEdit* url_input_ = nullptr;
    QLabel* now_playing_ = nullptr;
    QLabel* status_label_ = nullptr; // loading / error indicator

    QString pending_title_; // title while yt-dlp is resolving
    QString current_url_;   // currently selected stream URL (original, may be YouTube)
    QString current_title_; // title of current stream for refresh

    // Widgets needing theme-aware restyling
    QScrollArea* scroll_ = nullptr;
    QLabel* channel_header_ = nullptr;
    QVector<QPushButton*> channel_rows_;
    QVector<QLabel*> channel_name_labels_;
    QVector<QLabel*> channel_desc_labels_;
    QLabel* channel_sep_ = nullptr;
    QLabel* custom_header_ = nullptr;
    QPushButton* play_btn_ = nullptr;
    QLabel* helper_label_ = nullptr;
    QLabel* status_label_placeholder_ = nullptr; // #ifndef HAS_QT_MULTIMEDIA placeholder
    QWidget* controls_ = nullptr;
    QPushButton* stop_btn_ = nullptr;

#ifdef HAS_QT_MULTIMEDIA
    QMediaPlayer* player_ = nullptr;
    QVideoWidget* video_widget_ = nullptr;
    QAudioOutput* audio_output_ = nullptr;
#endif
};

inline BaseWidget* create_video_player_widget() {
    return new VideoPlayerWidget;
}

} // namespace fincept::screens::widgets
