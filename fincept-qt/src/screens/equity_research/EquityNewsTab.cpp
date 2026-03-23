// src/screens/equity_research/EquityNewsTab.cpp
#include "screens/equity_research/EquityNewsTab.h"

#include "services/equity/EquityResearchService.h"
#include "ui/theme/Theme.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>

namespace fincept::screens {

EquityNewsTab::EquityNewsTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::news_loaded,
            this, &EquityNewsTab::on_news_loaded);
}

void EquityNewsTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_) return;
    current_symbol_ = symbol;
    clear_cards();
    count_label_->setText("");
    company_label_->setText(symbol);
    status_label_->setText("Loading news…");
    status_label_->show();
    loading_overlay_->show_loading("LOADING NEWS…");
    services::equity::EquityResearchService::instance().fetch_news(symbol, 20);
}

void EquityNewsTab::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    loading_overlay_ = new ui::LoadingOverlay(this);
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Header bar ────────────────────────────────────────────────────────────
    auto* hdr = new QWidget;
    hdr->setStyleSheet(
        QString("background:%1; border-bottom:2px solid %2;")
        .arg(ui::colors::BG_SURFACE, ui::colors::AMBER));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(14, 8, 14, 8);
    hl->setSpacing(12);

    auto* title = new QLabel("LATEST NEWS");
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:2px; "
                "background:transparent; border:0;").arg(ui::colors::AMBER));
    hl->addWidget(title);

    count_label_ = new QLabel("");
    count_label_->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent; border:0;")
        .arg(ui::colors::TEXT_TERTIARY));
    hl->addWidget(count_label_);

    company_label_ = new QLabel("");
    company_label_->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent; border:0;")
        .arg("#22d3ee"));
    hl->addWidget(company_label_);

    hl->addStretch();

    auto* refresh_btn = new QPushButton("REFRESH");
    refresh_btn->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2; "
                "border-radius:3px; padding:4px 12px; font-size:10px; font-weight:700; }"
                "QPushButton:hover { border-color:%3; color:%3; }")
        .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_DIM, ui::colors::AMBER));
    connect(refresh_btn, &QPushButton::clicked, this, [this]() {
        if (!current_symbol_.isEmpty()) {
            clear_cards();
            status_label_->show();
            loading_overlay_->show_loading("LOADING NEWS…");
            services::equity::EquityResearchService::instance().fetch_news(current_symbol_, 20);
        }
    });
    hl->addWidget(refresh_btn);
    vl->addWidget(hdr);

    // ── Status label ──────────────────────────────────────────────────────────
    status_label_ = new QLabel("Search for a symbol to load news.");
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setStyleSheet(
        QString("color:%1; font-size:12px; padding:20px; background:transparent;")
        .arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(status_label_);

    // ── Scrollable card list ──────────────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent; border:0;");

    cards_container_ = new QWidget;
    cards_layout_ = new QVBoxLayout(cards_container_);
    cards_layout_->setContentsMargins(12, 12, 12, 12);
    cards_layout_->setSpacing(10);
    cards_layout_->addStretch();

    scroll->setWidget(cards_container_);
    vl->addWidget(scroll, 1);
}

void EquityNewsTab::clear_cards() {
    while (cards_layout_->count() > 1) {
        auto* item = cards_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

void EquityNewsTab::populate(const QVector<services::equity::NewsArticle>& articles) {
    clear_cards();
    status_label_->hide();
    count_label_->setText(QString("%1 articles").arg(articles.size()));

    if (articles.isEmpty()) {
        status_label_->setText("No news found for " + current_symbol_);
        status_label_->show();
        return;
    }

    for (const auto& art : articles) {
        auto* card = new QFrame;
        card->setStyleSheet(
            QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }"
                    "QFrame:hover { border-color:%3; background:%4; }")
            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM,
                 ui::colors::AMBER, ui::colors::BG_RAISED));
        card->setCursor(Qt::PointingHandCursor);

        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(14, 12, 14, 12);
        cl->setSpacing(6);

        // ── Title + meta row ──────────────────────────────────────────────────
        auto* top_hl = new QHBoxLayout;
        top_hl->setSpacing(8);

        auto* text_col = new QVBoxLayout;
        text_col->setSpacing(4);

        auto* title_lbl = new QLabel(art.title);
        title_lbl->setWordWrap(true);
        title_lbl->setStyleSheet(
            QString("color:%1; font-size:12px; font-weight:700; line-height:1.4; "
                    "background:transparent; border:0;")
            .arg(ui::colors::TEXT_PRIMARY));
        text_col->addWidget(title_lbl);

        // Publisher + date
        auto* meta_hl = new QHBoxLayout;
        meta_hl->setSpacing(10);
        auto* pub_lbl = new QLabel(art.publisher.isEmpty() ? "Unknown Source" : art.publisher);
        pub_lbl->setStyleSheet(
            QString("color:%1; font-size:10px; font-weight:700; background:transparent; border:0;")
            .arg("#22d3ee"));
        auto* date_lbl = new QLabel(art.published_date.isEmpty() ? "" : art.published_date.left(10));
        date_lbl->setStyleSheet(
            QString("color:%1; font-size:10px; background:transparent; border:0;")
            .arg(ui::colors::TEXT_TERTIARY));
        meta_hl->addWidget(pub_lbl);
        meta_hl->addWidget(date_lbl);
        meta_hl->addStretch();
        text_col->addLayout(meta_hl);

        top_hl->addLayout(text_col, 1);
        cl->addLayout(top_hl);

        // ── Description ───────────────────────────────────────────────────────
        if (!art.description.isEmpty() && art.description != "N/A") {
            auto* desc_lbl = new QLabel(art.description);
            desc_lbl->setWordWrap(true);
            desc_lbl->setStyleSheet(
                QString("color:%1; font-size:10px; line-height:1.5; "
                        "background:transparent; border:0;")
                .arg(ui::colors::TEXT_SECONDARY));
            cl->addWidget(desc_lbl);
        }

        // ── Read full article ──────────────────────────────────────────────────
        if (!art.url.isEmpty()) {
            auto* read_lbl = new QLabel("READ FULL ARTICLE →");
            read_lbl->setStyleSheet(
                QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px; "
                        "background:transparent; border:0;").arg(ui::colors::AMBER));
            cl->addWidget(read_lbl);

            // Whole card opens URL
            QString url = art.url;
            auto* click_filter = new QObject(card);
            card->installEventFilter(click_filter);
            connect(click_filter, &QObject::destroyed, this, []{});
            // Use QPushButton invisible overlay approach via event filter on card
            QObject::connect(card, &QFrame::destroyed, click_filter, &QObject::deleteLater);

            // Simpler: intercept mouse release via child button spanning the card
            auto* link_btn = new QPushButton;
            link_btn->setFlat(true);
            link_btn->setCursor(Qt::PointingHandCursor);
            link_btn->setStyleSheet("QPushButton { background:transparent; border:0; }");
            link_btn->setFixedSize(0, 0); // invisible, just for signal
            connect(link_btn, &QPushButton::clicked, this, [url]() {
                QDesktopServices::openUrl(QUrl(url));
            });

            // Make the read_lbl clickable
            read_lbl->setCursor(Qt::PointingHandCursor);
            read_lbl->installEventFilter(this);
            read_lbl->setProperty("url", url);
            cl->addWidget(link_btn);
        }

        cards_layout_->insertWidget(cards_layout_->count() - 1, card);
    }
}

bool EquityNewsTab::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        QString url = obj->property("url").toString();
        if (!url.isEmpty()) {
            QDesktopServices::openUrl(QUrl(url));
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void EquityNewsTab::on_news_loaded(QString symbol,
                                    QVector<services::equity::NewsArticle> articles) {
    if (symbol != current_symbol_) return;
    loading_overlay_->hide_loading();
    populate(articles);
}

} // namespace fincept::screens
