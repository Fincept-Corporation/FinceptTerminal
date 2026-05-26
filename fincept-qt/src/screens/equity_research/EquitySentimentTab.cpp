#include "screens/equity_research/EquitySentimentTab.h"

#include "services/equity/MarketSentimentService.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>

namespace fincept::screens {

namespace {

// make_panel / make_stat_box optionally capture the title label so the caller
// can re-translate it via retranslateUi. When title_lbl_out is nullptr the
// helper still constructs the title but doesn't expose it (caller is
// responsible for any later retranslation).
QFrame* make_panel(const QString& title, QLabel** title_lbl_out = nullptr) {
    auto* frame = new QFrame;
    frame->setStyleSheet(
        QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);

    auto* label = new QLabel(title);
    label->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px; background:transparent; border:0;")
            .arg(ui::colors::AMBER()));
    layout->addWidget(label);
    if (title_lbl_out)
        *title_lbl_out = label;
    return frame;
}

QWidget* make_stat_box(const QString& title, QLabel** value_label, QLabel** title_label_out = nullptr) {
    auto* box = new QFrame;
    box->setStyleSheet(
        QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(4);

    auto* title_label = new QLabel(title);
    title_label->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px; background:transparent; border:0;")
            .arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(title_label);
    if (title_label_out)
        *title_label_out = title_label;

    auto* value = new QLabel(QStringLiteral("—"));
    value->setStyleSheet(
        QString("color:%1; font-size:18px; font-weight:700; background:transparent; border:0;")
            .arg(ui::colors::TEXT_PRIMARY()));
    layout->addWidget(value);

    *value_label = value;
    return box;
}

QString alignment_label(const QString& alignment) {
    // Caller is a Q_OBJECT instance method; we route translation through
    // QCoreApplication so lupdate associates these strings with the
    // EquitySentimentTab context.
    if (alignment == "aligned")
        return QCoreApplication::translate("EquitySentimentTab", "ALIGNED");
    if (alignment == "mixed")
        return QCoreApplication::translate("EquitySentimentTab", "MIXED");
    if (alignment == "divergent")
        return QCoreApplication::translate("EquitySentimentTab", "DIVERGENT");
    if (alignment == "single_source")
        return QCoreApplication::translate("EquitySentimentTab", "SINGLE SOURCE");
    return QCoreApplication::translate("EquitySentimentTab", "UNAVAILABLE");
}

QString alignment_color(const QString& alignment) {
    if (alignment == "aligned") {
        return ui::colors::POSITIVE();
    }
    if (alignment == "mixed" || alignment == "single_source") {
        return ui::colors::WARNING();
    }
    if (alignment == "divergent") {
        return ui::colors::NEGATIVE();
    }
    return ui::colors::TEXT_TERTIARY();
}

} // namespace

EquitySentimentTab::EquitySentimentTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& service = services::equity::MarketSentimentService::instance();
    connect(&service, &services::equity::MarketSentimentService::snapshot_loaded,
            this, &EquitySentimentTab::on_snapshot_loaded);
}

void EquitySentimentTab::set_symbol(const QString& symbol) {
    if (symbol.isEmpty()) {
        return;
    }
    current_symbol_ = symbol;
    snapshot_loaded_ = false;
    company_label_->setText(symbol);
    coverage_label_->setText("");
    content_widget_->hide();
    clear_sources();
    status_label_->setText(tr("Loading market sentiment…"));
    status_label_->show();
    loading_overlay_->show_loading(tr("LOADING SENTIMENT…"));
    services::equity::MarketSentimentService::instance().fetch_snapshot(symbol);
}

void EquitySentimentTab::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    loading_overlay_ = new ui::LoadingOverlay(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

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

    coverage_label_ = new QLabel;
    coverage_label_->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent; border:0;")
            .arg(ui::colors::TEXT_TERTIARY()));
    header_layout->addWidget(coverage_label_);

    company_label_ = new QLabel;
    company_label_->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent; border:0;").arg("#22d3ee"));
    header_layout->addWidget(company_label_);

    header_layout->addStretch();

    refresh_btn_ = new QPushButton(tr("REFRESH"));
    refresh_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2; border-radius:3px; "
                "padding:4px 12px; font-size:10px; font-weight:700; }"
                "QPushButton:hover { border-color:%3; color:%3; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() {
        if (!current_symbol_.isEmpty()) {
            content_widget_->hide();
            clear_sources();
            status_label_->setText(tr("Refreshing market sentiment…"));
            status_label_->show();
            loading_overlay_->show_loading(tr("REFRESHING SENTIMENT…"));
            services::equity::MarketSentimentService::instance().fetch_snapshot(current_symbol_, 7, true);
        }
    });
    header_layout->addWidget(refresh_btn_);
    root->addWidget(header);

    status_label_ = new QLabel(tr("Open a symbol and enable Adanos Market Sentiment in Data Sources to load a snapshot."));
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setStyleSheet(
        QString("color:%1; font-size:12px; padding:20px; background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    root->addWidget(status_label_);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent; border:0;");

    content_widget_ = new QWidget;
    auto* content_layout = new QVBoxLayout(content_widget_);
    content_layout->setContentsMargins(12, 12, 12, 12);
    content_layout->setSpacing(12);
    content_layout->addWidget(build_summary_panel());

    auto* sources_panel = make_panel(tr("SOURCE BREAKDOWN"), &sources_title_lbl_);
    sources_widget_ = sources_panel;
    sources_layout_ = new QGridLayout;
    sources_layout_->setContentsMargins(0, 0, 0, 0);
    sources_layout_->setSpacing(10);
    static_cast<QVBoxLayout*>(sources_panel->layout())->addLayout(sources_layout_);
    content_layout->addWidget(sources_panel);
    content_layout->addStretch();

    scroll->setWidget(content_widget_);
    root->addWidget(scroll, 1);
    content_widget_->hide();
}

QWidget* EquitySentimentTab::build_summary_panel() {
    auto* panel = make_panel(tr("SUMMARY"), &summary_title_lbl_);
    auto* grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);

    grid->addWidget(make_stat_box(tr("AVERAGE BUZZ"), &avg_buzz_value_,    &avg_buzz_title_),    0, 0);
    grid->addWidget(make_stat_box(tr("BULLISH %"),    &avg_bullish_value_, &avg_bullish_title_), 0, 1);
    grid->addWidget(make_stat_box(tr("COVERAGE"),     &coverage_value_,    &coverage_title_),    1, 0);
    grid->addWidget(make_stat_box(tr("ALIGNMENT"),    &alignment_value_,   &alignment_title_),   1, 1);

    static_cast<QVBoxLayout*>(panel->layout())->addLayout(grid);
    return panel;
}

void EquitySentimentTab::clear_sources() {
    while (sources_layout_->count() > 0) {
        auto* item = sources_layout_->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void EquitySentimentTab::populate(const services::equity::MarketSentimentSnapshot& snapshot) {
    avg_buzz_value_->setText(QString::number(snapshot.average_buzz, 'f', 1));
    avg_bullish_value_->setText(QString("%1%").arg(QString::number(snapshot.average_bullish_pct, 'f', 1)));
    coverage_value_->setText(QString("%1 / %2").arg(snapshot.coverage).arg(snapshot.sources.size()));
    alignment_value_->setText(alignment_label(snapshot.source_alignment));
    alignment_value_->setStyleSheet(
        QString("color:%1; font-size:18px; font-weight:700; background:transparent; border:0;")
            .arg(alignment_color(snapshot.source_alignment)));

    clear_sources();
    for (int i = 0; i < snapshot.sources.size(); ++i) {
        const auto& source = snapshot.sources.at(i);
        auto* card = make_panel(source.label);
        auto* layout = static_cast<QVBoxLayout*>(card->layout());

        auto add_row = [layout](const QString& key, const QString& value, const QString& color = QString()) {
            auto* row = new QWidget;
            auto* row_layout = new QHBoxLayout(row);
            row_layout->setContentsMargins(0, 0, 0, 0);
            row_layout->setSpacing(6);

            auto* key_label = new QLabel(key);
            key_label->setStyleSheet(
                QString("color:%1; font-size:10px; background:transparent; border:0;").arg(ui::colors::TEXT_SECONDARY()));
            row_layout->addWidget(key_label);

            auto* value_label = new QLabel(value);
            value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            value_label->setStyleSheet(
                QString("color:%1; font-size:11px; font-weight:600; background:transparent; border:0;")
                    .arg(color.isEmpty() ? ui::colors::TEXT_PRIMARY() : color));
            row_layout->addWidget(value_label, 1);
            layout->addWidget(row);
        };

        if (source.available) {
            add_row(tr("Buzz"),      QString::number(source.buzz_score, 'f', 1));
            add_row(tr("Bullish"),   QString("%1%").arg(QString::number(source.bullish_pct, 'f', 1)));
            add_row(tr("Activity"),  QString::number(source.activity_count, 'f', 0));
            add_row(tr("Sentiment"), QString::number(source.sentiment_score, 'f', 2),
                    source.sentiment_score >= 0.0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
        } else {
            auto* empty = new QLabel(tr("No snapshot available."));
            empty->setWordWrap(true);
            empty->setStyleSheet(
                QString("color:%1; font-size:11px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY()));
            layout->addWidget(empty);
        }

        sources_layout_->addWidget(card, i / 2, i % 2);
    }
}

void EquitySentimentTab::on_snapshot_loaded(QString symbol, fincept::services::equity::MarketSentimentSnapshot snapshot) {
    if (symbol != current_symbol_) {
        return;
    }

    loading_overlay_->hide_loading();
    cached_snapshot_ = snapshot;
    snapshot_loaded_ = true;

    coverage_label_->setText(snapshot.coverage > 0
                                  ? tr("%1 sources live").arg(snapshot.coverage)
                                  : tr("Optional alternative data"));

    if (!snapshot.configured || !snapshot.available) {
        content_widget_->hide();
        status_label_->setText(snapshot.message.isEmpty()
                                   ? tr("No market sentiment snapshot is available for this symbol.")
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
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void EquitySentimentTab::retranslateUi() {
    if (title_lbl_)         title_lbl_->setText(tr("MARKET SENTIMENT"));
    if (refresh_btn_)       refresh_btn_->setText(tr("REFRESH"));
    if (sources_title_lbl_) sources_title_lbl_->setText(tr("SOURCE BREAKDOWN"));
    if (summary_title_lbl_) summary_title_lbl_->setText(tr("SUMMARY"));
    if (avg_buzz_title_)    avg_buzz_title_->setText(tr("AVERAGE BUZZ"));
    if (avg_bullish_title_) avg_bullish_title_->setText(tr("BULLISH %"));
    if (coverage_title_)    coverage_title_->setText(tr("COVERAGE"));
    if (alignment_title_)   alignment_title_->setText(tr("ALIGNMENT"));

    // Idle status text — only overwrite when nothing's loaded yet.
    if (status_label_ && current_symbol_.isEmpty())
        status_label_->setText(tr("Open a symbol and enable Adanos Market Sentiment in Data Sources to load a snapshot."));

    // Re-render from cached snapshot so coverage caption, alignment badge,
    // and per-source rows pick up the new locale.
    if (snapshot_loaded_) {
        coverage_label_->setText(cached_snapshot_.coverage > 0
                                      ? tr("%1 sources live").arg(cached_snapshot_.coverage)
                                      : tr("Optional alternative data"));
        if (cached_snapshot_.configured && cached_snapshot_.available)
            populate(cached_snapshot_);
    }
}

} // namespace fincept::screens
