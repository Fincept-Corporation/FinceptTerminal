#include "screens/dashboard/widgets/NewsWidget.h"

#include "services/markets/MarketDataService.h"
#include "ui/theme/Theme.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens::widgets {

NewsWidget::NewsWidget(QWidget* parent) : BaseWidget("MARKET NEWS", parent, ui::colors::CYAN) {
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);

    auto* container = new QWidget(this);
    news_layout_ = new QVBoxLayout(container);
    news_layout_->setContentsMargins(4, 4, 4, 4);
    news_layout_->setSpacing(0);
    news_layout_->addStretch();

    scroll_area_->setWidget(container);
    content_layout()->addWidget(scroll_area_);

    connect(this, &BaseWidget::refresh_requested, this, &NewsWidget::refresh_data);

    apply_styles();
    set_loading(true);
    refresh_data();
}

void NewsWidget::apply_styles() {
    scroll_area_->setStyleSheet(QString("QScrollArea { border: none; background: transparent; }"
                                        "QScrollBar:vertical { width: 6px; background: transparent; }"
                                        "QScrollBar::handle:vertical { background: %1; border-radius: 3px; }"
                                        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                                    .arg(ui::colors::BORDER_MED()));
}

void NewsWidget::on_theme_changed() {
    apply_styles();
    if (!last_articles_.isEmpty())
        populate(last_articles_);
}

void NewsWidget::refresh_data() {
    set_loading(true);

    // Fetch news for SPY (broad market news)
    services::MarketDataService::instance().fetch_news("SPY", 10, [this](bool ok, QJsonArray articles) {
        set_loading(false);
        if (!ok || articles.isEmpty()) {
            if (news_layout_->count() <= 1) {
                set_error("No news available. Check Python/yfinance.");
            }
            return;
        }
        populate(articles);
    });
}

void NewsWidget::populate(const QJsonArray& articles) {
    last_articles_ = articles;

    // Clear old items (keep the stretch)
    while (news_layout_->count() > 1) {
        auto* item = news_layout_->takeAt(0);
        delete item->widget();
        delete item;
    }

    for (const auto& val : articles) {
        auto obj = val.toObject();
        QString title = obj["title"].toString();
        QString publisher = obj["publisher"].toString();
        QString pub_date = obj["published_date"].toString();

        if (title.isEmpty())
            continue;

        // Extract time portion from date
        QString time_str;
        if (pub_date.length() >= 16) {
            time_str = pub_date.mid(11, 5); // "HH:MM"
        }

        auto* row = new QWidget(this);
        row->setStyleSheet(QString("border-bottom: 1px solid %1;").arg(ui::colors::BORDER_DIM()));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(4, 4, 4, 4);
        rl->setSpacing(8);

        if (!time_str.isEmpty()) {
            auto* time_lbl = new QLabel(time_str);
            time_lbl->setFixedWidth(36);
            time_lbl->setStyleSheet(
                QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::CYAN()));
            rl->addWidget(time_lbl);
        }

        auto* headline = new QLabel(title);
        headline->setWordWrap(true);
        headline->setStyleSheet(
            QString("color: %1; font-size: 11px; background: transparent;").arg(ui::colors::TEXT_PRIMARY()));
        rl->addWidget(headline, 1);

        if (!publisher.isEmpty()) {
            auto* src = new QLabel(publisher);
            src->setStyleSheet(
                QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
            rl->addWidget(src);
        }

        // Insert before the stretch
        news_layout_->insertWidget(news_layout_->count() - 1, row);
    }
}

} // namespace fincept::screens::widgets
