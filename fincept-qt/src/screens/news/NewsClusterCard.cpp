#include "screens/news/NewsClusterCard.h"
#include "ui/theme/Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace fincept::screens {

using namespace fincept::services;

NewsClusterCard::NewsClusterCard(const NewsCluster& cluster, QWidget* parent)
    : QWidget(parent), cluster_(cluster) {

    setStyleSheet(QString("background:%1;border:1px solid %2;margin:1px 4px;")
        .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(12, 6, 12, 6);
    vl->setSpacing(4);

    // Header row
    auto* hdr = new QHBoxLayout;
    hdr->setSpacing(6);

    if (cluster.is_breaking) {
        auto* badge = new QLabel("BREAKING");
        badge->setStyleSheet(QString("color:#080808;background:%1;font-size:12px;font-weight:700;"
            "padding:1px 6px;font-family:'Consolas','Courier New',monospace;").arg(ui::colors::NEGATIVE));
        hdr->addWidget(badge);
    }

    auto* tier = new QLabel(QString("T%1").arg(cluster.tier));
    tier->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(cluster.tier <= 2 ? ui::colors::AMBER : ui::colors::TEXT_TERTIARY));
    hdr->addWidget(tier);

    auto* headline = new QLabel(cluster.lead_article.headline);
    headline->setStyleSheet(QString("color:%1;font-size:13px;%2background:transparent;"
        "font-family:'Consolas','Courier New',monospace;")
        .arg(cluster.is_breaking ? ui::colors::TEXT_PRIMARY : ui::colors::TEXT_SECONDARY)
        .arg(cluster.is_breaking ? "font-weight:600;" : ""));
    headline->setWordWrap(true);
    headline->setCursor(Qt::PointingHandCursor);
    hdr->addWidget(headline, 1);

    vl->addLayout(hdr);

    // Meta row
    auto* meta = new QHBoxLayout;
    meta->setSpacing(8);

    auto* source = new QLabel(cluster.lead_article.source);
    source->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::CYAN));
    meta->addWidget(source);

    auto* time_lbl = new QLabel(relative_time(cluster.latest_sort_ts));
    time_lbl->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_DIM));
    meta->addWidget(time_lbl);

    auto* src_count = new QLabel(QString("%1 sources").arg(cluster.source_count));
    src_count->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_SECONDARY));
    meta->addWidget(src_count);

    auto* vel = new QLabel(cluster.velocity.toUpper());
    QString vel_color = cluster.velocity == "rising" ? ui::colors::POSITIVE
                      : cluster.velocity == "falling" ? ui::colors::NEGATIVE
                      : ui::colors::TEXT_TERTIARY;
    vel->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(vel_color));
    meta->addWidget(vel);

    auto* sent = new QLabel(sentiment_string(cluster.sentiment));
    sent->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(sentiment_color(cluster.sentiment)));
    meta->addWidget(sent);

    meta->addStretch();
    vl->addLayout(meta);

    // Expand toggle + related articles
    if (cluster.articles.size() > 1) {
        auto* toggle = new QPushButton(QString("+ %1 related").arg(cluster.articles.size() - 1));
        toggle->setFixedHeight(22);
        toggle->setCursor(Qt::PointingHandCursor);
        toggle->setStyleSheet(
            QString("QPushButton{background:transparent;color:%1;border:none;text-align:left;"
            "font-size:12px;font-family:'Consolas','Courier New',monospace;padding:0;}"
            "QPushButton:hover{color:%2;}").arg(ui::colors::AMBER, ui::colors::TEXT_PRIMARY));

        related_container_ = new QWidget;
        related_container_->hide();
        auto* rvl = new QVBoxLayout(related_container_);
        rvl->setContentsMargins(20, 0, 0, 0);
        rvl->setSpacing(0);

        for (int i = 1; i < cluster.articles.size(); i++) {
            const auto& a = cluster.articles[i];
            auto* row = new QPushButton(QString("%1  %2 — %3")
                .arg(relative_time(a.sort_ts), a.source, a.headline));
            row->setFixedHeight(26);
            row->setCursor(Qt::PointingHandCursor);
            row->setStyleSheet(
                QString("QPushButton{background:transparent;color:%1;border:none;text-align:left;"
                "font-size:12px;font-family:'Consolas','Courier New',monospace;padding:0 6px;}"
                "QPushButton:hover{color:%2;background:%3;}")
                .arg(ui::colors::TEXT_SECONDARY, ui::colors::TEXT_PRIMARY, ui::colors::BG_RAISED));
            connect(row, &QPushButton::clicked, this, [this, i]() {
                if (i < cluster_.articles.size()) emit article_clicked(cluster_.articles[i]);
            });
            rvl->addWidget(row);
        }

        connect(toggle, &QPushButton::clicked, this, [this, toggle]() {
            expanded_ = !expanded_;
            related_container_->setVisible(expanded_);
            toggle->setText(expanded_
                ? QString("- hide %1 related").arg(cluster_.articles.size() - 1)
                : QString("+ %1 related").arg(cluster_.articles.size() - 1));
        });

        vl->addWidget(toggle);
        vl->addWidget(related_container_);
    }
}

} // namespace fincept::screens
