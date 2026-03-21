#include "screens/news/BreakingBanner.h"
#include "ui/theme/Theme.h"
#include <QHBoxLayout>
#include <QPushButton>

namespace fincept::screens {

using namespace fincept::services;

BreakingBanner::BreakingBanner(QWidget* parent) : QWidget(parent) {
    setFixedHeight(26);
    setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
        .arg(ui::colors::BG_RAISED, ui::colors::NEGATIVE));

    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(12, 0, 12, 0);
    h->setSpacing(8);

    tag_label_ = new QLabel("BREAKING");
    tag_label_->setStyleSheet(QString("color:#080808;background:%1;font-size:12px;font-weight:700;"
        "padding:1px 6px;font-family:'Consolas','Courier New',monospace;").arg(ui::colors::NEGATIVE));
    h->addWidget(tag_label_);

    headline_label_ = new QLabel;
    headline_label_->setStyleSheet(QString("color:%1;font-size:13px;font-weight:600;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_PRIMARY));
    headline_label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    h->addWidget(headline_label_, 1);

    source_label_ = new QLabel;
    source_label_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::CYAN));
    h->addWidget(source_label_);

    auto* dismiss = new QPushButton("X");
    dismiss->setFixedSize(22, 22);
    dismiss->setCursor(Qt::PointingHandCursor);
    dismiss->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;font-size:12px;font-weight:700;"
        "border:none;font-family:'Consolas','Courier New',monospace;}"
        "QPushButton:hover{color:%2;}").arg(ui::colors::NEGATIVE, ui::colors::TEXT_PRIMARY));
    connect(dismiss, &QPushButton::clicked, this, &QWidget::hide);
    h->addWidget(dismiss);

    // Auto-dismiss after 60s
    dismiss_timer_ = new QTimer(this);
    dismiss_timer_->setSingleShot(true);
    dismiss_timer_->setInterval(60000);
    connect(dismiss_timer_, &QTimer::timeout, this, &QWidget::hide);

    setCursor(Qt::PointingHandCursor);
}

void BreakingBanner::show_cluster(const NewsCluster& cluster) {
    headline_label_->setText(cluster.lead_article.headline);
    source_label_->setText(cluster.lead_article.source);

    bool is_flash = cluster.lead_article.priority == Priority::FLASH;
    tag_label_->setText(is_flash ? "FLASH" : "BREAKING");
    tag_label_->setStyleSheet(QString("color:#080808;background:%1;font-size:12px;font-weight:700;"
        "padding:1px 6px;font-family:'Consolas','Courier New',monospace;")
        .arg(is_flash ? ui::colors::NEGATIVE : ui::colors::AMBER));

    dismiss_timer_->start();
    show();
}

} // namespace fincept::screens
