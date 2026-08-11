#pragma once
#include "services/news/NewsService.h"

#include <QFont>
#include <QFontMetrics>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

/// 22px scrolling ticker at the bottom showing FLASH/URGENT/BREAKING headlines.
class NewsTickerStrip : public QWidget {
    Q_OBJECT
  public:
    explicit NewsTickerStrip(QWidget* parent = nullptr);

    void set_articles(const QVector<services::NewsArticle>& breaking_articles);
    void pause();
    void resume();

  signals:
    void article_clicked(const services::NewsArticle& article);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    /// P3: the scroll timer must not run while this strip is invisible. It was
    /// previously driven only by the owning screen calling pause()/resume(), so
    /// a refactor that dropped one of those calls left a 20fps antialiased paint
    /// running behind a hidden tab with nothing to notice it. These overrides
    /// make the invariant self-enforcing; pause()/resume() remain for the
    /// explicit hover-to-read and edit cases, which are not visibility events.
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    struct TickerEntry {
        services::NewsArticle article;
        QString tag_text;
        QString source_text;
        QString headline_text;
        int tag_width = 0;
        int source_width = 0;
        int headline_width = 0;
        int total_width = 0;
    };

    QVector<TickerEntry> entries_;
    QTimer scroll_timer_;
    double offset_ = 0;
    int total_width_ = 0;
    bool paused_ = false;

    QFont ticker_font_;
    QFontMetrics ticker_fm_;
    static constexpr int ITEM_SPACING = 50;
    static constexpr int SEGMENT_GAP = 8;
};

} // namespace fincept::screens
