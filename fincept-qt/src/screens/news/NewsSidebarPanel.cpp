#include "screens/news/NewsSidebarPanel.h"
#include "ui/theme/Theme.h"
#include <QScrollArea>
#include <QFrame>
#include <QPushButton>
#include <algorithm>

namespace fincept::screens {

using namespace fincept::services;

static QLabel* section_title(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:700;letter-spacing:0.5px;"
        "background:transparent;padding:6px 12px 2px 12px;font-family:'Consolas','Courier New',monospace;")
        .arg(ui::colors::AMBER));
    return l;
}

static QFrame* separator() {
    auto* f = new QFrame;
    f->setFixedHeight(1);
    f->setStyleSheet(QString("background:%1;border:none;").arg(ui::colors::BORDER_DIM));
    return f;
}

NewsSidebarPanel::NewsSidebarPanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background:%1;border-right:1px solid %2;")
        .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        "QScrollArea{border:none;background:transparent;}"
        "QScrollBar:vertical{width:5px;background:transparent;}"
        "QScrollBar::handle:vertical{background:#222222;}"
        "QScrollBar::handle:vertical:hover{background:#333333;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    auto* content = new QWidget;
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_sentiment_gauge());
    vl->addWidget(separator());
    vl->addWidget(build_top_stories());
    vl->addWidget(separator());
    vl->addWidget(build_categories());
    vl->addWidget(separator());
    vl->addWidget(build_feed_stats());
    vl->addStretch();

    scroll->setWidget(content);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll);
}

QWidget* NewsSidebarPanel::build_sentiment_gauge() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(12, 6, 12, 6);
    vl->setSpacing(4);
    vl->addWidget(section_title("MARKET SENTIMENT"));

    // Horizontal bar
    auto* bar = new QWidget;
    bar->setFixedHeight(6);
    auto* bh = new QHBoxLayout(bar);
    bh->setContentsMargins(0, 0, 0, 0);
    bh->setSpacing(1);

    bull_bar_ = new QWidget; bull_bar_->setStyleSheet(QString("background:%1;").arg(ui::colors::POSITIVE));
    neut_bar_ = new QWidget; neut_bar_->setStyleSheet(QString("background:%1;").arg(ui::colors::WARNING));
    bear_bar_ = new QWidget; bear_bar_->setStyleSheet(QString("background:%1;").arg(ui::colors::NEGATIVE));
    bh->addWidget(bull_bar_, 1);
    bh->addWidget(neut_bar_, 1);
    bh->addWidget(bear_bar_, 1);
    vl->addWidget(bar);

    // Labels row
    auto* lr = new QHBoxLayout;
    lr->setSpacing(0);
    auto mk = [](const QString& color) {
        auto* l = new QLabel("0");
        l->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;"
            "font-family:'Consolas','Courier New',monospace;").arg(color));
        l->setAlignment(Qt::AlignCenter);
        return l;
    };
    bull_label_ = mk(ui::colors::POSITIVE);
    neut_label_ = mk(ui::colors::WARNING);
    bear_label_ = mk(ui::colors::NEGATIVE);
    lr->addWidget(bull_label_, 1);
    lr->addWidget(neut_label_, 1);
    lr->addWidget(bear_label_, 1);
    vl->addLayout(lr);

    // Score
    sent_score_label_ = new QLabel("0.00");
    sent_score_label_->setStyleSheet(QString("color:%1;font-size:16px;font-weight:700;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_PRIMARY));
    sent_score_label_->setAlignment(Qt::AlignCenter);
    vl->addWidget(sent_score_label_);

    return w;
}

QWidget* NewsSidebarPanel::build_top_stories() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    vl->addWidget(section_title("TOP STORIES"));

    stories_layout_ = new QVBoxLayout;
    stories_layout_->setContentsMargins(0, 0, 0, 0);
    stories_layout_->setSpacing(0);
    vl->addLayout(stories_layout_);
    return w;
}

QWidget* NewsSidebarPanel::build_categories() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    vl->addWidget(section_title("CATEGORIES"));

    categories_layout_ = new QVBoxLayout;
    categories_layout_->setContentsMargins(0, 0, 0, 0);
    categories_layout_->setSpacing(0);
    vl->addLayout(categories_layout_);
    return w;
}

QWidget* NewsSidebarPanel::build_feed_stats() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(12, 6, 12, 6);
    vl->setSpacing(2);
    vl->addWidget(section_title("FEED STATS"));

    auto mk = [&](const QString& label) {
        auto* row = new QHBoxLayout;
        row->setSpacing(0);
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;letter-spacing:0.5px;"
            "background:transparent;font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_SECONDARY));
        auto* val = new QLabel("0");
        val->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;background:transparent;"
            "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_PRIMARY));
        val->setAlignment(Qt::AlignRight);
        row->addWidget(lbl);
        row->addStretch();
        row->addWidget(val);
        vl->addLayout(row);
        return val;
    };

    feeds_label_    = mk("FEEDS");
    articles_label_ = mk("ARTICLES");
    clusters_label_ = mk("CLUSTERS");
    sources_label_  = mk("SOURCES");
    return w;
}

void NewsSidebarPanel::update_data(
    const QVector<NewsArticle>& articles,
    const QMap<QString, int>& category_counts,
    int bullish, int bearish, int neutral,
    int feed_count, int source_count, int cluster_count)
{
    current_articles_ = articles;
    int total = bullish + bearish + neutral;

    // Sentiment gauge
    bull_bar_->setFixedWidth(total > 0 ? std::max(2, bullish * 220 / total) : 73);
    neut_bar_->setFixedWidth(total > 0 ? std::max(2, neutral * 220 / total) : 73);
    bear_bar_->setFixedWidth(total > 0 ? std::max(2, bearish * 220 / total) : 73);

    bull_label_->setText(QString("BUL %1").arg(bullish));
    neut_label_->setText(QString("NEU %1").arg(neutral));
    bear_label_->setText(QString("BEA %1").arg(bearish));

    double score = total > 0 ? static_cast<double>(bullish - bearish) / total : 0;
    sent_score_label_->setText(QString::number(score, 'f', 2));
    sent_score_label_->setStyleSheet(QString("color:%1;font-size:16px;font-weight:700;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;")
        .arg(score > 0 ? ui::colors::POSITIVE : score < 0 ? ui::colors::NEGATIVE : ui::colors::WARNING));

    // Top stories (top 10 by priority then recency)
    while (stories_layout_->count() > 0) {
        auto* item = stories_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    QVector<NewsArticle> ranked = articles;
    std::sort(ranked.begin(), ranked.end(), [](const NewsArticle& a, const NewsArticle& b) {
        if (a.priority != b.priority) return static_cast<int>(a.priority) < static_cast<int>(b.priority);
        return a.sort_ts > b.sort_ts;
    });

    int count = std::min(10, static_cast<int>(ranked.size()));
    for (int i = 0; i < count; i++) {
        const auto& art = ranked[i];
        auto* row = new QWidget;
        row->setCursor(Qt::PointingHandCursor);
        row->setStyleSheet(QString("QWidget{background:transparent;padding:3px 12px;}"
            "QWidget:hover{background:%1;}").arg(ui::colors::BG_RAISED));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(6);

        auto* num = new QLabel(QString::number(i + 1));
        num->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;"
            "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::AMBER));
        num->setFixedWidth(14);
        num->setAlignment(Qt::AlignRight | Qt::AlignTop);
        rl->addWidget(num);

        auto* text = new QLabel(art.headline);
        text->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;"
            "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_SECONDARY));
        text->setWordWrap(true);
        text->setMaximumHeight(32);
        rl->addWidget(text, 1);

        stories_layout_->addWidget(row);
    }

    // Categories
    while (categories_layout_->count() > 0) {
        auto* item = categories_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    for (auto it = category_counts.begin(); it != category_counts.end(); ++it) {
        auto* btn = new QPushButton(QString("%1  %2").arg(it.key()).arg(it.value()));
        btn->setFixedHeight(26);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            QString("QPushButton{background:transparent;color:%1;border:none;text-align:left;"
            "padding:0 12px;font-size:12px;font-family:'Consolas','Courier New',monospace;}"
            "QPushButton:hover{color:%2;background:%3;}")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::TEXT_PRIMARY, ui::colors::BG_RAISED));
        connect(btn, &QPushButton::clicked, this, [this, cat=it.key()]() {
            emit category_clicked(cat);
        });
        categories_layout_->addWidget(btn);
    }

    // Feed stats
    feeds_label_->setText(QString::number(feed_count));
    articles_label_->setText(QString::number(articles.size()));
    clusters_label_->setText(QString::number(cluster_count));
    sources_label_->setText(QString::number(source_count));
}

} // namespace fincept::screens
