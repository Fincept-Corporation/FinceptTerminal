#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"

#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>

#ifdef HAS_QT_MULTIMEDIA
#include <QMediaPlayer>
#include <QVideoWidget>
#endif

namespace fincept::screens::widgets {

/// Inline video/stream player widget.
/// When Qt Multimedia is available, plays HLS/MP4 streams directly inside the widget.
/// Falls back to system browser launch when Multimedia is absent.
class VideoPlayerWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit VideoPlayerWidget(QWidget* parent = nullptr);

  private slots:
    void play_preset(int index);
    void play_custom_url();
    void stop_playback();

  private:
    void build_channel_list();
    void build_player_view();
    void play_url(const QString& url, const QString& title);

    QStackedWidget* stack_ = nullptr;   // page 0 = channel list, page 1 = player
    QWidget* list_page_ = nullptr;
    QWidget* player_page_ = nullptr;
    QLineEdit* url_input_ = nullptr;
    QLabel* now_playing_ = nullptr;

#ifdef HAS_QT_MULTIMEDIA
    QMediaPlayer* player_ = nullptr;
    QVideoWidget* video_widget_ = nullptr;
#endif
};

inline BaseWidget* create_video_player_widget() { return new VideoPlayerWidget; }

} // namespace fincept::screens::widgets
