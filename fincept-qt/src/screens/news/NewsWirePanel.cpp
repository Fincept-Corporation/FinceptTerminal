#include "screens/news/NewsWirePanel.h"
#include "screens/news/WireRow.h"
#include "screens/news/NewsClusterCard.h"
#include "screens/news/BreakingBanner.h"
#include "ui/theme/Theme.h"
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>

namespace fincept::screens {

using namespace fincept::services;

NewsWirePanel::NewsWirePanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Breaking banner (hidden initially)
    banner_ = new BreakingBanner;
    banner_->hide();
    root->addWidget(banner_);

    // Scrollable feed area
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        "QScrollArea{border:none;background:transparent;}"
        "QScrollBar:vertical{width:5px;background:transparent;}"
        "QScrollBar::handle:vertical{background:#222222;}"
        "QScrollBar::handle:vertical:hover{background:#333333;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    feed_container_ = new QWidget;
    feed_layout_ = new QVBoxLayout(feed_container_);
    feed_layout_->setContentsMargins(0, 0, 0, 0);
    feed_layout_->setSpacing(0);
    feed_layout_->addStretch();

    scroll->setWidget(feed_container_);
    root->addWidget(scroll, 1);
}

void NewsWirePanel::set_articles(const QVector<NewsArticle>& articles,
                                  const QVector<NewsCluster>& clusters) {
    articles_ = articles;
    clusters_ = clusters;
    visible_count_ = page_size_;

    // Show breaking banner if any breaking clusters
    auto breaking = get_breaking_clusters(clusters_);
    if (!breaking.isEmpty()) {
        banner_->show_cluster(breaking.first());
        banner_->show();
    } else {
        banner_->hide();
    }

    if (view_mode_ == "WIRE")           rebuild_wire();
    else if (view_mode_ == "CLUSTERED") rebuild_clustered();
    else                                rebuild_by_source();
}

void NewsWirePanel::set_view_mode(const QString& mode) {
    view_mode_ = mode;
    visible_count_ = page_size_;
    if (!articles_.isEmpty()) {
        if (mode == "WIRE")           rebuild_wire();
        else if (mode == "CLUSTERED") rebuild_clustered();
        else                          rebuild_by_source();
    }
}

void NewsWirePanel::set_selected_article(const QString& article_id) {
    selected_id_ = article_id;
    for (int i = 0; i < feed_layout_->count(); i++) {
        auto* item = feed_layout_->itemAt(i);
        if (!item || !item->widget()) continue;
        auto* row = qobject_cast<WireRow*>(item->widget());
        if (row) row->set_selected(row->article_id() == article_id);
    }
}

void NewsWirePanel::clear_feed() {
    while (feed_layout_->count() > 0) {
        auto* item = feed_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

void NewsWirePanel::rebuild_wire() {
    clear_feed();

    if (articles_.isEmpty()) {
        auto* empty = new QLabel("NO ARTICLES FOUND");
        empty->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;"
            "font-family:'Consolas','Courier New',monospace;background:transparent;padding:40px;")
            .arg(ui::colors::TEXT_TERTIARY));
        empty->setAlignment(Qt::AlignCenter);
        feed_layout_->addWidget(empty);
        feed_layout_->addStretch();
        return;
    }

    int limit = std::min(visible_count_, static_cast<int>(articles_.size()));
    for (int i = 0; i < limit; i++) {
        auto* row = new WireRow(articles_[i]);
        row->set_selected(articles_[i].id == selected_id_);
        connect(row, &WireRow::clicked, this, [this, i]() {
            if (i < articles_.size()) emit article_selected(articles_[i]);
        });
        feed_layout_->addWidget(row);
    }

    // Load more button
    if (limit < articles_.size()) {
        auto* more = new QPushButton(QString("LOAD MORE  (%1 remaining)").arg(articles_.size() - limit));
        more->setFixedHeight(26);
        more->setCursor(Qt::PointingHandCursor);
        more->setStyleSheet(
            QString("QPushButton{background:%1;color:%2;border:1px solid %3;"
            "font-size:12px;font-weight:700;font-family:'Consolas','Courier New',monospace;}"
            "QPushButton:hover{background:%4;border-color:%5;}")
            .arg(ui::colors::BG_SURFACE, ui::colors::AMBER, ui::colors::BORDER_DIM,
                 ui::colors::BG_RAISED, ui::colors::AMBER));
        connect(more, &QPushButton::clicked, this, [this]() {
            visible_count_ += page_size_;
            rebuild_wire();
        });
        feed_layout_->addWidget(more);
    }

    feed_layout_->addStretch();
}

void NewsWirePanel::rebuild_clustered() {
    clear_feed();

    if (clusters_.isEmpty()) {
        auto* empty = new QLabel("NO CLUSTERS FOUND");
        empty->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;"
            "font-family:'Consolas','Courier New',monospace;background:transparent;padding:40px;")
            .arg(ui::colors::TEXT_TERTIARY));
        empty->setAlignment(Qt::AlignCenter);
        feed_layout_->addWidget(empty);
        feed_layout_->addStretch();
        return;
    }

    for (const auto& cluster : clusters_) {
        auto* card = new NewsClusterCard(cluster);
        connect(card, &NewsClusterCard::article_clicked, this,
            [this](const NewsArticle& a) { emit article_selected(a); });
        feed_layout_->addWidget(card);
    }
    feed_layout_->addStretch();
}

void NewsWirePanel::rebuild_by_source() {
    clear_feed();

    QMap<QString, QVector<NewsArticle>> by_source;
    for (const auto& a : articles_) {
        by_source[a.source].append(a);
    }

    if (by_source.isEmpty()) {
        auto* empty = new QLabel("NO ARTICLES FOUND");
        empty->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;"
            "font-family:'Consolas','Courier New',monospace;background:transparent;padding:40px;")
            .arg(ui::colors::TEXT_TERTIARY));
        empty->setAlignment(Qt::AlignCenter);
        feed_layout_->addWidget(empty);
        feed_layout_->addStretch();
        return;
    }

    for (auto it = by_source.begin(); it != by_source.end(); ++it) {
        auto* hdr = new QLabel(QString("%1  (%2)").arg(it.key()).arg(it.value().size()));
        hdr->setStyleSheet(
            QString("color:%1;font-size:12px;font-weight:700;letter-spacing:0.5px;"
            "background:%2;padding:6px 12px;border-bottom:1px solid %3;"
            "font-family:'Consolas','Courier New',monospace;")
            .arg(ui::colors::CYAN, ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
        feed_layout_->addWidget(hdr);

        int limit = std::min(10, static_cast<int>(it.value().size()));
        for (int i = 0; i < limit; i++) {
            auto* row = new WireRow(it.value()[i]);
            row->set_selected(it.value()[i].id == selected_id_);
            connect(row, &WireRow::clicked, this, [this, art=it.value()[i]]() {
                emit article_selected(art);
            });
            feed_layout_->addWidget(row);
        }
    }
    feed_layout_->addStretch();
}

} // namespace fincept::screens
