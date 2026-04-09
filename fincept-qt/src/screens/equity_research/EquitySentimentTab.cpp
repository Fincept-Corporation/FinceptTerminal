#include "screens/equity_research/EquitySentimentTab.h"

#include "services/equity/MarketSentimentService.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>

namespace fincept::screens {

namespace {

QFrame* make_panel(const QString& title) {
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
    return frame;
}

QWidget* make_stat_box(const QString& title, QLabel** value_label) {
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

    auto* value = new QLabel("—");
    value->setStyleSheet(
        QString("color:%1; font-size:18px; font-weight:700; background:transparent; border:0;")
            .arg(ui::colors::TEXT_PRIMARY()));
    layout->addWidget(value);

    *value_label = value;
    return box;
}

QString alignment_label(const QString& alignment) {
    if (alignment == "aligned") {
        return "ALIGNED";
    }
    if (alignment == "mixed") {
        return "MIXED";
    }
    if (alignment == "divergent") {
        return "DIVERGENT";
    }
    if (alignment == "single_source") {
        return "SINGLE SOURCE";
    }
    return "UNAVAILABLE";
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
    company_label_->setText(symbol);
    coverage_label_->setText("");
    content_widget_->hide();
    clear_sources();
    status_label_->setText("Loading market sentiment…");
    status_label_->show();
    loading_overlay_->show_loading("LOADING SENTIMENT…");
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

    auto* title = new QLabel("MARKET SENTIMENT");
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:2px; background:transparent; border:0;")
            .arg(ui::colors::AMBER()));
    header_layout->addWidget(title);

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

    auto* refresh_button = new QPushButton("REFRESH");
    refresh_button->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2; border-radius:3px; "
                "padding:4px 12px; font-size:10px; font-weight:700; }"
                "QPushButton:hover { border-color:%3; color:%3; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    connect(refresh_button, &QPushButton::clicked, this, [this]() {
        if (!current_symbol_.isEmpty()) {
            content_widget_->hide();
            clear_sources();
            status_label_->setText("Refreshing market sentiment…");
            status_label_->show();
            loading_overlay_->show_loading("REFRESHING SENTIMENT…");
            services::equity::MarketSentimentService::instance().fetch_snapshot(current_symbol_, 7, true);
        }
    });
    header_layout->addWidget(refresh_button);
    root->addWidget(header);

    status_label_ = new QLabel("Open a symbol and enable Adanos Market Sentiment in Data Sources to load a snapshot.");
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

    auto* sources_panel = make_panel("SOURCE BREAKDOWN");
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
    auto* panel = make_panel("SUMMARY");
    auto* grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);

    grid->addWidget(make_stat_box("AVERAGE BUZZ", &avg_buzz_value_), 0, 0);
    grid->addWidget(make_stat_box("BULLISH %", &avg_bullish_value_), 0, 1);
    grid->addWidget(make_stat_box("COVERAGE", &coverage_value_), 1, 0);
    grid->addWidget(make_stat_box("ALIGNMENT", &alignment_value_), 1, 1);

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
            add_row("Buzz", QString::number(source.buzz_score, 'f', 1));
            add_row("Bullish", QString("%1%").arg(QString::number(source.bullish_pct, 'f', 1)));
            add_row("Activity", QString::number(source.activity_count, 'f', 0));
            add_row("Sentiment", QString::number(source.sentiment_score, 'f', 2),
                    source.sentiment_score >= 0.0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
        } else {
            auto* empty = new QLabel("No snapshot available.");
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
    coverage_label_->setText(snapshot.coverage > 0
                                  ? QString("%1 sources live").arg(snapshot.coverage)
                                  : QString("Optional alternative data"));

    if (!snapshot.configured || !snapshot.available) {
        content_widget_->hide();
        status_label_->setText(snapshot.message.isEmpty() ? "No market sentiment snapshot is available for this symbol." : snapshot.message);
        status_label_->show();
        return;
    }

    populate(snapshot);
    status_label_->hide();
    content_widget_->show();
}

} // namespace fincept::screens
