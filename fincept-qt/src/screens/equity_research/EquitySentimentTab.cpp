#include "screens/equity_research/EquitySentimentTab.h"

#include "services/equity/EquitySentimentService.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>

namespace fincept::screens {

namespace {

// Distinct helper names (anonymous namespace) to avoid unity-build ODR clashes.
QFrame* sentiment_make_panel(const QString& title, QVBoxLayout** body_out, QLabel** title_out = nullptr) {
    auto* frame = new QFrame;
    frame->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    auto* label = new QLabel(title);
    label->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px; background:transparent; border:0;")
            .arg(ui::colors::AMBER()));
    layout->addWidget(label);
    if (title_out)
        *title_out = label;
    if (body_out)
        *body_out = layout;
    return frame;
}

QString sentiment_label_color(const QString& label) {
    if (label == "BULLISH")
        return ui::colors::POSITIVE();
    if (label == "BEARISH")
        return ui::colors::NEGATIVE();
    return ui::colors::TEXT_TERTIARY();
}

QString sentiment_fmt_score(double score) {
    return QString("%1%2").arg(score >= 0.0 ? "+" : "", QString::number(score, 'f', 2));
}

} // namespace

EquitySentimentTab::EquitySentimentTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect(&services::equity::EquitySentimentService::instance(),
            &services::equity::EquitySentimentService::sentiment_loaded, this,
            &EquitySentimentTab::on_sentiment_loaded);
}

void EquitySentimentTab::set_symbol(const QString& symbol) {
    if (symbol.isEmpty())
        return;
    current_symbol_ = symbol;
    snapshot_loaded_ = false;
    company_label_->setText(symbol);
    caption_label_->setText("");
    content_widget_->hide();
    status_label_->setText(tr("Loading sentiment…"));
    status_label_->show();
    loading_overlay_->show_loading(tr("LOADING SENTIMENT…"));
    services::equity::EquitySentimentService::instance().fetch_sentiment(symbol);
}

void EquitySentimentTab::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    loading_overlay_ = new ui::LoadingOverlay(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setStyleSheet(
        QString("background:%1; border-bottom:2px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::AMBER()));
    auto* header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(14, 8, 14, 8);
    header_layout->setSpacing(12);

    title_lbl_ = new QLabel(tr("MARKET SENTIMENT"));
    title_lbl_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:2px; background:transparent; border:0;")
            .arg(ui::colors::AMBER()));
    header_layout->addWidget(title_lbl_);

    caption_label_ = new QLabel;
    caption_label_->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY()));
    header_layout->addWidget(caption_label_);

    company_label_ = new QLabel;
    company_label_->setStyleSheet(QString("color:%1; font-size:10px; background:transparent; border:0;").arg("#22d3ee"));
    header_layout->addWidget(company_label_);

    header_layout->addStretch();

    refresh_btn_ = new QPushButton(tr("REFRESH"));
    refresh_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2; border-radius:3px; "
                "padding:4px 12px; font-size:10px; font-weight:700; }"
                "QPushButton:hover { border-color:%3; color:%3; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() {
        if (!current_symbol_.isEmpty())
            set_symbol(current_symbol_);
    });
    header_layout->addWidget(refresh_btn_);
    root->addWidget(header);

    // ── Idle / status text ──────────────────────────────────────────────────────
    status_label_ = new QLabel(tr("Open a symbol to compute sentiment from news and price momentum."));
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(
        QString("color:%1; font-size:12px; padding:20px; background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    root->addWidget(status_label_);

    // ── Scrollable content ────────────────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent; border:0;");

    content_widget_ = new QWidget;
    auto* content_layout = new QVBoxLayout(content_widget_);
    content_layout->setContentsMargins(12, 12, 12, 12);
    content_layout->setSpacing(12);

    // Net score badge + distribution bar in one summary panel.
    QVBoxLayout* summary_body = nullptr;
    auto* summary_panel = sentiment_make_panel(tr("OVERALL"), &summary_body, &summary_title_lbl_);

    auto* badge_row = new QHBoxLayout;
    badge_row->setContentsMargins(0, 0, 0, 0);
    badge_row->setSpacing(16);

    net_label_ = new QLabel(QStringLiteral("—"));
    net_label_->setStyleSheet(
        QString("color:%1; font-size:26px; font-weight:800; background:transparent; border:0;")
            .arg(ui::colors::TEXT_PRIMARY()));
    badge_row->addWidget(net_label_);

    net_score_label_ = new QLabel;
    net_score_label_->setStyleSheet(
        QString("color:%1; font-size:22px; font-weight:700; background:transparent; border:0;")
            .arg(ui::colors::TEXT_SECONDARY()));
    badge_row->addWidget(net_score_label_);

    badge_row->addStretch();

    net_conf_label_ = new QLabel;
    net_conf_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    net_conf_label_->setStyleSheet(
        QString("color:%1; font-size:11px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY()));
    badge_row->addWidget(net_conf_label_);
    summary_body->addLayout(badge_row);

    // Distribution bar (bull / neutral / bear proportions).
    auto* dist_bar = new QFrame;
    dist_bar->setFixedHeight(10);
    dist_bar->setStyleSheet("background:transparent; border:0;");
    dist_layout_ = new QHBoxLayout(dist_bar);
    dist_layout_->setContentsMargins(0, 0, 0, 0);
    dist_layout_->setSpacing(2);
    summary_body->addWidget(dist_bar);

    dist_counts_label_ = new QLabel;
    dist_counts_label_->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY()));
    summary_body->addWidget(dist_counts_label_);

    content_layout->addWidget(summary_panel);

    // Source breakdown.
    QVBoxLayout* sources_body = nullptr;
    auto* sources_panel = sentiment_make_panel(tr("SIGNAL SOURCES"), &sources_body, &sources_title_lbl_);
    sources_layout_ = new QVBoxLayout;
    sources_layout_->setContentsMargins(0, 0, 0, 0);
    sources_layout_->setSpacing(6);
    sources_body->addLayout(sources_layout_);
    content_layout->addWidget(sources_panel);

    // Per-article list.
    QVBoxLayout* articles_body = nullptr;
    auto* articles_panel = sentiment_make_panel(tr("HEADLINES"), &articles_body, &articles_title_lbl_);
    articles_layout_ = new QVBoxLayout;
    articles_layout_->setContentsMargins(0, 0, 0, 0);
    articles_layout_->setSpacing(6);
    articles_body->addLayout(articles_layout_);
    content_layout->addWidget(articles_panel);

    content_layout->addStretch();
    scroll->setWidget(content_widget_);
    root->addWidget(scroll, 1);
    content_widget_->hide();
}

void EquitySentimentTab::clear_layout(QLayout* layout) {
    if (!layout)
        return;
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        if (item->layout())
            clear_layout(item->layout());
        delete item;
    }
}

void EquitySentimentTab::populate(const services::equity::EquitySentimentSnapshot& snapshot) {
    // ── Net badge ──────────────────────────────────────────────────────────────
    net_label_->setText(snapshot.label);
    net_label_->setStyleSheet(QString("color:%1; font-size:26px; font-weight:800; background:transparent; border:0;")
                                  .arg(sentiment_label_color(snapshot.label)));
    net_score_label_->setText(sentiment_fmt_score(snapshot.overall_score));
    net_conf_label_->setText(tr("Confidence %1%").arg(QString::number(snapshot.confidence * 100.0, 'f', 0)));

    // ── Distribution bar ─────────────────────────────────────────────────────
    clear_layout(dist_layout_);
    const int total = snapshot.bullish + snapshot.bearish + snapshot.neutral;
    auto add_segment = [this](int count, const QString& color) {
        if (count <= 0)
            return;
        auto* seg = new QFrame;
        seg->setFixedHeight(10);
        seg->setStyleSheet(QString("background:%1; border:0; border-radius:2px;").arg(color));
        dist_layout_->addWidget(seg, count);
    };
    if (total > 0) {
        add_segment(snapshot.bullish, ui::colors::POSITIVE());
        add_segment(snapshot.neutral, ui::colors::BORDER_DIM());
        add_segment(snapshot.bearish, ui::colors::NEGATIVE());
    } else {
        auto* seg = new QFrame;
        seg->setFixedHeight(10);
        seg->setStyleSheet(QString("background:%1; border:0; border-radius:2px;").arg(ui::colors::BORDER_DIM()));
        dist_layout_->addWidget(seg, 1);
    }
    dist_counts_label_->setText(tr("%1 bullish · %2 neutral · %3 bearish  (%4 headlines)")
                                    .arg(snapshot.bullish)
                                    .arg(snapshot.neutral)
                                    .arg(snapshot.bearish)
                                    .arg(snapshot.article_count));

    // ── Signal sources ─────────────────────────────────────────────────────────
    clear_layout(sources_layout_);
    for (const auto& src : snapshot.sources) {
        auto* row = new QWidget;
        auto* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 0, 0, 0);
        row_layout->setSpacing(8);

        auto* name = new QLabel(src.label);
        name->setStyleSheet(
            QString("color:%1; font-size:11px; font-weight:600; background:transparent; border:0;")
                .arg(ui::colors::TEXT_PRIMARY()));
        row_layout->addWidget(name);
        row_layout->addStretch();

        if (src.available) {
            auto* score = new QLabel(sentiment_fmt_score(src.score));
            score->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; background:transparent; border:0;")
                                     .arg(src.score >= 0.0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
            row_layout->addWidget(score);

            auto* weight = new QLabel(tr("wt %1%").arg(QString::number(src.weight * 100.0, 'f', 0)));
            weight->setStyleSheet(QString("color:%1; font-size:10px; background:transparent; border:0;")
                                      .arg(ui::colors::TEXT_TERTIARY()));
            row_layout->addWidget(weight);
        } else {
            auto* na = new QLabel(tr("n/a"));
            na->setStyleSheet(QString("color:%1; font-size:10px; background:transparent; border:0;")
                                  .arg(ui::colors::TEXT_TERTIARY()));
            row_layout->addWidget(na);
        }
        sources_layout_->addWidget(row);
    }

    // ── Headlines ───────────────────────────────────────────────────────────────
    clear_layout(articles_layout_);
    if (snapshot.articles.isEmpty()) {
        auto* empty = new QLabel(tr("No headlines available for this symbol."));
        empty->setStyleSheet(
            QString("color:%1; font-size:11px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY()));
        articles_layout_->addWidget(empty);
    }
    for (const auto& article : snapshot.articles) {
        auto* row = new QFrame;
        row->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:3px; }")
                               .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(8, 6, 8, 6);
        row_layout->setSpacing(8);

        const QString chip_color = sentiment_label_color(article.label);
        auto* chip = new QLabel(article.label.left(4));
        chip->setAlignment(Qt::AlignCenter);
        chip->setFixedWidth(46);
        chip->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:1px; "
                                    "border:1px solid %1; border-radius:3px; padding:2px 0; background:transparent;")
                                .arg(chip_color));
        row_layout->addWidget(chip, 0, Qt::AlignTop);

        auto* text_col = new QVBoxLayout;
        text_col->setContentsMargins(0, 0, 0, 0);
        text_col->setSpacing(2);

        auto* headline = new QLabel(article.title);
        headline->setWordWrap(true);
        headline->setStyleSheet(
            QString("color:%1; font-size:11px; background:transparent; border:0;").arg(ui::colors::TEXT_PRIMARY()));
        text_col->addWidget(headline);

        QString meta = article.publisher;
        const QString date = article.published_date.left(10);
        if (!date.isEmpty())
            meta = meta.isEmpty() ? date : QString("%1 · %2").arg(meta, date);
        if (!meta.isEmpty()) {
            auto* meta_lbl = new QLabel(meta);
            meta_lbl->setStyleSheet(
                QString("color:%1; font-size:9px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY()));
            text_col->addWidget(meta_lbl);
        }
        row_layout->addLayout(text_col, 1);
        articles_layout_->addWidget(row);
    }
}

void EquitySentimentTab::on_sentiment_loaded(QString symbol,
                                             fincept::services::equity::EquitySentimentSnapshot snapshot) {
    if (symbol != current_symbol_)
        return;

    loading_overlay_->hide_loading();
    cached_snapshot_ = snapshot;
    snapshot_loaded_ = true;

    caption_label_->setText(snapshot.available && !snapshot.engine.isEmpty()
                                ? tr("engine: %1").arg(snapshot.engine)
                                : tr("self-computed"));

    if (!snapshot.available) {
        content_widget_->hide();
        status_label_->setText(snapshot.message.isEmpty()
                                   ? tr("No sentiment is available for this symbol.")
                                   : snapshot.message);
        status_label_->show();
        return;
    }

    populate(snapshot);
    status_label_->hide();
    content_widget_->show();
}

// ── Re-translation ───────────────────────────────────────────────────────────

void EquitySentimentTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquitySentimentTab::retranslateUi() {
    if (title_lbl_)
        title_lbl_->setText(tr("MARKET SENTIMENT"));
    if (refresh_btn_)
        refresh_btn_->setText(tr("REFRESH"));
    if (summary_title_lbl_)
        summary_title_lbl_->setText(tr("OVERALL"));
    if (sources_title_lbl_)
        sources_title_lbl_->setText(tr("SIGNAL SOURCES"));
    if (articles_title_lbl_)
        articles_title_lbl_->setText(tr("HEADLINES"));

    if (status_label_ && current_symbol_.isEmpty())
        status_label_->setText(tr("Open a symbol to compute sentiment from news and price momentum."));

    if (snapshot_loaded_ && cached_snapshot_.available)
        populate(cached_snapshot_);
}

} // namespace fincept::screens
