#include "screens/news/WireRow.h"
#include "ui/theme/Theme.h"
#include <QMouseEvent>

namespace fincept::screens {

using namespace fincept::services;

WireRow::WireRow(const NewsArticle& article, QWidget* parent) : QWidget(parent) {
    article_id_ = article.id;
    priority_ = article.priority;
    is_hot_ = (article.priority == Priority::FLASH || article.priority == Priority::URGENT);

    setFixedHeight(26);
    setCursor(Qt::PointingHandCursor);

    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(12, 0, 12, 0);
    h->setSpacing(6);

    // Time
    auto* time_lbl = new QLabel(relative_time(article.sort_ts));
    time_lbl->setFixedWidth(28);
    time_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    time_lbl->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_DIM));
    h->addWidget(time_lbl);

    // Priority dot
    auto pc = priority_color(article.priority);
    dot_ = new QWidget;
    dot_->setFixedSize(4, 4);
    dot_->setStyleSheet(QString("background:%1;").arg(pc));
    h->addWidget(dot_);

    // Source
    auto* src = new QLabel(article.source);
    src->setFixedWidth(64);
    src->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::CYAN));
    h->addWidget(src);

    // Headline
    auto* head = new QLabel(article.headline);
    head->setStyleSheet(QString("color:%1;font-size:12px;%2background:transparent;"
        "font-family:'Consolas','Courier New',monospace;")
        .arg(is_hot_ ? ui::colors::TEXT_PRIMARY : ui::colors::TEXT_SECONDARY)
        .arg(is_hot_ ? "font-weight:600;" : ""));
    head->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    h->addWidget(head, 1);

    // Sentiment arrow
    QString arrow;
    auto sc = sentiment_color(article.sentiment);
    if (article.sentiment == Sentiment::BULLISH)      arrow = "\xe2\x96\xb2";  // ▲
    else if (article.sentiment == Sentiment::BEARISH)  arrow = "\xe2\x96\xbc";  // ▼
    else                                               arrow = "\xe2\x80\x93";  // –
    auto* sent = new QLabel(arrow);
    sent->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(sc));
    h->addWidget(sent);

    // Tickers (up to 2)
    for (int i = 0; i < std::min(2, static_cast<int>(article.tickers.size())); i++) {
        auto* tk = new QLabel("$" + article.tickers[i]);
        tk->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;"
            "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::WARNING));
        h->addWidget(tk);
    }

    update_style();
}

void WireRow::set_selected(bool selected) {
    selected_ = selected;
    update_style();
}

void WireRow::mousePressEvent(QMouseEvent*) {
    emit clicked();
}

void WireRow::enterEvent(QEnterEvent*) {
    hovered_ = true;
    update_style();
}

void WireRow::leaveEvent(QEvent*) {
    hovered_ = false;
    update_style();
}

void WireRow::update_style() {
    auto pc = priority_color(priority_);
    QString bg = selected_ ? ui::colors::BG_HOVER
               : hovered_  ? ui::colors::BG_RAISED
                           : ui::colors::BG_SURFACE;
    QString bl = selected_ ? QString("1px solid %1").arg(ui::colors::INFO)
               : is_hot_   ? QString("1px solid %1").arg(pc)
                           : QString("1px solid transparent");

    setStyleSheet(QString(
        "WireRow{background:%1;border-left:%2;border-bottom:1px solid %3;}")
        .arg(bg, bl, ui::colors::BORDER_DIM));
}

} // namespace fincept::screens
