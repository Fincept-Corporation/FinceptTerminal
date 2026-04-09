#include "screens/news/NewsTickerStrip.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>

namespace fincept::screens {

NewsTickerStrip::NewsTickerStrip(QWidget* parent)
    : QWidget(parent), ticker_font_(ui::fonts::DATA_FAMILY, 10), ticker_fm_(ticker_font_) {
    setObjectName("newsTickerStrip");
    setFixedHeight(22);

    connect(&scroll_timer_, &QTimer::timeout, this, [this]() {
        offset_ += 1.0;
        if (total_width_ > 0 && offset_ >= total_width_)
            offset_ = 0;
        update();
    });
    // Timer interval set but NOT started — parent manages via pause/resume (P3)
    scroll_timer_.setInterval(50); // 20fps

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { update(); });
}

void NewsTickerStrip::set_articles(const QVector<services::NewsArticle>& breaking_articles) {
    entries_.clear();
    total_width_ = 0;

    for (const auto& article : breaking_articles) {
        if (article.priority != services::Priority::FLASH && article.priority != services::Priority::URGENT &&
            article.priority != services::Priority::BREAKING)
            continue;

        TickerEntry entry;
        entry.article = article;
        entry.tag_text = services::priority_string(article.priority);
        entry.source_text = article.source.toUpper();
        entry.headline_text = article.headline;

        entry.tag_width = ticker_fm_.horizontalAdvance(entry.tag_text);
        entry.source_width = ticker_fm_.horizontalAdvance(entry.source_text);
        entry.headline_width = ticker_fm_.horizontalAdvance(entry.headline_text);
        entry.total_width =
            entry.tag_width + SEGMENT_GAP + entry.source_width + SEGMENT_GAP + entry.headline_width + ITEM_SPACING;

        total_width_ += entry.total_width;
        entries_.append(entry);
    }

    offset_ = 0;
    update();
}

void NewsTickerStrip::pause() {
    paused_ = true;
    scroll_timer_.stop();
}

void NewsTickerStrip::resume() {
    paused_ = false;
    if (!entries_.isEmpty())
        scroll_timer_.start();
}

void NewsTickerStrip::paintEvent(QPaintEvent*) {
    if (entries_.isEmpty())
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setFont(ticker_font_);

    int text_y = (height() + ticker_fm_.ascent() - ticker_fm_.descent()) / 2;
    double x = -offset_;
    int passes = total_width_ > 0 ? (width() / total_width_) + 2 : 1;

    for (int pass = 0; pass < passes; ++pass) {
        for (const auto& entry : entries_) {
            // Tag in priority color
            QString pcolor = services::priority_color(entry.article.priority);
            p.setPen(QColor(pcolor));
            p.drawText(QPointF(x, text_y), entry.tag_text);
            x += entry.tag_width + SEGMENT_GAP;

            // Source in cyan
            p.setPen(QColor(ui::colors::CYAN()));
            p.drawText(QPointF(x, text_y), entry.source_text);
            x += entry.source_width + SEGMENT_GAP;

            // Headline in secondary text
            p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
            p.drawText(QPointF(x, text_y), entry.headline_text);
            x += entry.headline_width + ITEM_SPACING;
        }
    }
}

void NewsTickerStrip::mousePressEvent(QMouseEvent* event) {
    if (entries_.isEmpty())
        return;

    double click_x = event->position().x() + offset_;
    // Normalize to within one cycle
    if (total_width_ > 0)
        click_x = std::fmod(click_x, static_cast<double>(total_width_));

    double cumulative = 0;
    for (const auto& entry : entries_) {
        cumulative += entry.total_width;
        if (click_x < cumulative) {
            emit article_clicked(entry.article);
            return;
        }
    }
}

void NewsTickerStrip::enterEvent(QEnterEvent*) {
    scroll_timer_.stop();
}

void NewsTickerStrip::leaveEvent(QEvent*) {
    if (!paused_ && !entries_.isEmpty())
        scroll_timer_.start();
}

} // namespace fincept::screens
