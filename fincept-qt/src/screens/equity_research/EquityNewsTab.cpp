// src/screens/equity_research/EquityNewsTab.cpp
#include "screens/equity_research/EquityNewsTab.h"

#include "services/equity/EquityResearchService.h"
#include "ui/theme/Theme.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QUrl>

namespace fincept::screens {

EquityNewsTab::EquityNewsTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::news_loaded, this, &EquityNewsTab::on_news_loaded);
    // Dismiss the "LOADING NEWS…" overlay on failure — it's hidden only on success.
    connect(&svc, &services::equity::EquityResearchService::error_occurred, this,
            [this](const QString& ctx, const QString&) {
                if (ctx == "News" && loading_overlay_)
                    loading_overlay_->hide_loading();
            });
}

void EquityNewsTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    news_loaded_ = false;
    company_label_->setText(symbol);
    refresh_provider_availability();
    start_fetch();
}

// (Re)fetch the current symbol's news using the selected provider. Sole owner of
// the news fetch — the screen no longer triggers it, so the dropdown is authoritative.
void EquityNewsTab::start_fetch() {
    if (current_symbol_.isEmpty())
        return;
    clear_cards();
    count_label_->setText("");
    status_label_->setText(tr("Loading news…"));
    status_label_->show();
    loading_overlay_->show_loading(tr("LOADING NEWS…"));
    using NP = services::equity::NewsProvider;
    services::equity::EquityResearchService::instance().fetch_news(current_symbol_, 20,
                                                                   use_newsapi_ ? NP::NewsApi : NP::Auto);
}

// Enable the "NewsAPI" item only when a key is configured in Data Sources; if it
// was selected but the key went away, fall back to Auto.
void EquityNewsTab::refresh_provider_availability() {
    if (!provider_combo_)
        return;
    const bool avail = !services::equity::EquityResearchService::instance().configured_newsapi_key().isEmpty();
    if (auto* model = qobject_cast<QStandardItemModel*>(provider_combo_->model())) {
        if (auto* item = model->item(1)) {
            item->setEnabled(avail);
            item->setToolTip(avail ? QString() : tr("Add a NewsAPI key in the Data Sources tab"));
        }
    }
    if (!avail && use_newsapi_) {
        use_newsapi_ = false;
        suppress_provider_signal_ = true;
        provider_combo_->setCurrentIndex(0);
        suppress_provider_signal_ = false;
    }
}

QString EquityNewsTab::provider_key() const {
    return use_newsapi_ ? QStringLiteral("newsapi") : QStringLiteral("auto");
}

void EquityNewsTab::set_provider_key(const QString& key) {
    use_newsapi_ = (key == QLatin1String("newsapi"));
    if (provider_combo_) {
        suppress_provider_signal_ = true;
        provider_combo_->setCurrentIndex(use_newsapi_ ? 1 : 0);
        suppress_provider_signal_ = false;
    }
}

void EquityNewsTab::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    loading_overlay_ = new ui::LoadingOverlay(this);
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Header bar ────────────────────────────────────────────────────────────
    auto* hdr = new QWidget(this);
    hdr->setStyleSheet(
        QString("background:%1; border-bottom:2px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::AMBER()));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(14, 8, 14, 8);
    hl->setSpacing(12);

    title_lbl_ = new QLabel(tr("LATEST NEWS"));
    title_lbl_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; letter-spacing:2px; "
                                       "background:transparent; border:0;")
                                  .arg(ui::colors::AMBER()));
    hl->addWidget(title_lbl_);

    count_label_ = new QLabel("");
    count_label_->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY()));
    hl->addWidget(count_label_);

    company_label_ = new QLabel("");
    company_label_->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent; border:0;").arg("#22d3ee"));
    hl->addWidget(company_label_);

    hl->addStretch();

    // ── Provider switch ─────────────────────────────────────────────────────────
    // "NewsAPI" stays disabled until a key is configured in the Data Sources tab.
    provider_combo_ = new QComboBox;
    provider_combo_->addItem(tr("Auto (GNews → Yahoo)"));
    provider_combo_->addItem(QStringLiteral("NewsAPI"));
    provider_combo_->setToolTip(tr("News source"));
    provider_combo_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3; border-radius:3px; "
                "padding:3px 8px; font-size:10px; font-weight:700; }"
                "QComboBox::drop-down { border:0; width:14px; }"
                "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3; "
                "selection-background-color:%4; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(),
                 ui::colors::BG_RAISED()));
    connect(provider_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (suppress_provider_signal_)
            return;
        use_newsapi_ = (idx == 1);
        start_fetch();
    });
    hl->addWidget(provider_combo_);

    refresh_btn_ = new QPushButton(tr("REFRESH"));
    refresh_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2; "
                                        "border-radius:3px; padding:4px 12px; font-size:10px; font-weight:700; }"
                                        "QPushButton:hover { border-color:%3; color:%3; }")
                                    .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() { start_fetch(); });
    hl->addWidget(refresh_btn_);
    vl->addWidget(hdr);

    refresh_provider_availability(); // set the initial enabled state of the NewsAPI item

    // ── Status label ──────────────────────────────────────────────────────────
    status_label_ = new QLabel(tr("Search for a symbol to load news."));
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setStyleSheet(
        QString("color:%1; font-size:12px; padding:20px; background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(status_label_);

    // ── Scrollable card list ──────────────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent; border:0;");

    cards_container_ = new QWidget(this);
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
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void EquityNewsTab::populate(const QVector<services::equity::NewsArticle>& articles) {
    clear_cards();
    status_label_->hide();
    count_label_->setText(tr("%1 articles").arg(articles.size()));

    if (articles.isEmpty()) {
        status_label_->setText(tr("No news found for %1").arg(current_symbol_));
        status_label_->show();
        return;
    }

    for (const auto& art : articles) {
        auto* card = new QFrame;
        card->setStyleSheet(
            QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }"
                    "QFrame:hover { border-color:%3; background:%4; }")
                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::AMBER(), ui::colors::BG_RAISED()));
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
        title_lbl->setStyleSheet(QString("color:%1; font-size:12px; font-weight:700; line-height:1.4; "
                                         "background:transparent; border:0;")
                                     .arg(ui::colors::TEXT_PRIMARY()));
        text_col->addWidget(title_lbl);

        // Publisher + date
        auto* meta_hl = new QHBoxLayout;
        meta_hl->setSpacing(10);
        auto* pub_lbl = new QLabel(art.publisher.isEmpty() ? tr("Unknown Source") : art.publisher);
        pub_lbl->setStyleSheet(
            QString("color:%1; font-size:10px; font-weight:700; background:transparent; border:0;").arg("#22d3ee"));
        auto* date_lbl = new QLabel(art.published_date.isEmpty() ? "" : art.published_date.left(10));
        date_lbl->setStyleSheet(
            QString("color:%1; font-size:10px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY()));
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
            desc_lbl->setStyleSheet(QString("color:%1; font-size:10px; line-height:1.5; "
                                            "background:transparent; border:0;")
                                        .arg(ui::colors::TEXT_SECONDARY()));
            cl->addWidget(desc_lbl);
        }

        // ── Read full article ──────────────────────────────────────────────────
        if (!art.url.isEmpty()) {
            auto* read_lbl = new QLabel(tr("READ FULL ARTICLE →"));
            read_lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px; "
                                            "background:transparent; border:0;")
                                        .arg(ui::colors::AMBER()));
            cl->addWidget(read_lbl);

            // Whole card opens URL
            QString url = art.url;
            auto* click_filter = new QObject(card);
            card->installEventFilter(click_filter);
            connect(click_filter, &QObject::destroyed, this, [] {});
            // Use QPushButton invisible overlay approach via event filter on card
            QObject::connect(card, &QFrame::destroyed, click_filter, &QObject::deleteLater);

            // Simpler: intercept mouse release via child button spanning the card
            auto* link_btn = new QPushButton;
            link_btn->setFlat(true);
            link_btn->setCursor(Qt::PointingHandCursor);
            link_btn->setStyleSheet("QPushButton { background:transparent; border:0; }");
            link_btn->setFixedSize(0, 0); // invisible, just for signal
            connect(link_btn, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(QUrl(url)); });

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

void EquityNewsTab::on_news_loaded(QString symbol, QVector<services::equity::NewsArticle> articles) {
    if (symbol != current_symbol_)
        return;
    loading_overlay_->hide_loading();
    cached_articles_ = articles;
    news_loaded_ = true;
    populate(articles);
}

// ── Re-translation ───────────────────────────────────────────────────────────
// Static chrome (title, refresh button, idle status text) re-set explicitly.
// Card content is rebuilt by populate() from cached articles so each card's
// "Unknown Source" fallback and "READ FULL ARTICLE →" link pick up new locale.

void EquityNewsTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void EquityNewsTab::retranslateUi() {
    if (title_lbl_)   title_lbl_->setText(tr("LATEST NEWS"));
    if (refresh_btn_) refresh_btn_->setText(tr("REFRESH"));
    if (provider_combo_) {
        provider_combo_->setItemText(0, tr("Auto (GNews → Yahoo)"));
        provider_combo_->setToolTip(tr("News source"));
    }

    if (status_label_) {
        // Pick the appropriate idle text — never overwrite a live "Loading…"
        // string mid-fetch (the next populate() call will replace it).
        if (current_symbol_.isEmpty())
            status_label_->setText(tr("Search for a symbol to load news."));
        else if (!news_loaded_)
            status_label_->setText(tr("Loading news…"));
    }

    if (news_loaded_)
        populate(cached_articles_);
}

} // namespace fincept::screens
