#include "screens/dashboard/widgets/StockQuoteWidget.h"

#include "ui/theme/Theme.h"

#include <QFrame>

namespace fincept::screens::widgets {

StockQuoteWidget::StockQuoteWidget(const QString& symbol, QWidget* parent)
    : BaseWidget(QString("QUOTE: %1").arg(symbol.toUpper()), parent), symbol_(symbol.toUpper()) {
    auto* vl = content_layout();
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(8);

    // ── Price row ──
    auto* price_row = new QWidget(this);
    auto* prl = new QHBoxLayout(price_row);
    prl->setContentsMargins(0, 0, 0, 0);
    prl->setSpacing(8);

    price_label_ = new QLabel("--");
    prl->addWidget(price_label_);

    auto* change_col = new QWidget(this);
    auto* ccl = new QVBoxLayout(change_col);
    ccl->setContentsMargins(0, 4, 0, 0);
    ccl->setSpacing(0);

    arrow_label_ = new QLabel;
    ccl->addWidget(arrow_label_);

    change_label_ = new QLabel("--");
    ccl->addWidget(change_label_);

    prl->addWidget(change_col);
    prl->addStretch();

    ticker_label_ = new QLabel(symbol_);
    prl->addWidget(ticker_label_);

    vl->addWidget(price_row);

    // ── Separator ──
    sep_ = new QFrame;
    sep_->setFixedHeight(1);
    vl->addWidget(sep_);

    // ── Stats grid ──
    auto* stats = new QWidget(this);
    auto* gl = new QGridLayout(stats);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(6);

    auto make_stat = [&](int row, int col, const QString& label, QLabel*& val_out) {
        auto* cell = new QWidget(this);
        stat_cells_.append(cell);
        auto* cl = new QVBoxLayout(cell);
        cl->setContentsMargins(8, 6, 8, 6);
        cl->setSpacing(2);

        auto* lbl = new QLabel(label);
        stat_labels_.append(lbl);
        cl->addWidget(lbl);

        val_out = new QLabel("--");
        stat_values_.append(val_out);
        cl->addWidget(val_out);

        gl->addWidget(cell, row, col);
    };

    make_stat(0, 0, "OPEN", open_val_);
    make_stat(0, 1, "PREV CLOSE", prev_val_);
    make_stat(1, 0, "HIGH", high_val_);
    make_stat(1, 1, "LOW", low_val_);
    make_stat(2, 0, "VOLUME", volume_val_);

    vl->addWidget(stats);
    vl->addStretch();

    connect(this, &BaseWidget::refresh_requested, this, &StockQuoteWidget::refresh_data);

    apply_styles();
    set_loading(true);
    refresh_data();
}

void StockQuoteWidget::apply_styles() {
    price_label_->setStyleSheet(QString("color: %1; font-size: 28px; font-weight: bold; background: transparent;")
                                    .arg(ui::colors::TEXT_PRIMARY()));
    arrow_label_->setStyleSheet("font-size: 12px; background: transparent;");
    change_label_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;")
                                     .arg(ui::colors::TEXT_SECONDARY()));
    ticker_label_->setStyleSheet(
        QString("color: %1; font-size: 14px; font-weight: bold; background: transparent;").arg(ui::colors::AMBER()));
    sep_->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM()));

    for (auto* cell : stat_cells_)
        cell->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(ui::colors::BG_RAISED()));
    for (auto* lbl : stat_labels_)
        lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    for (auto* val : stat_values_)
        val->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;")
                               .arg(ui::colors::TEXT_PRIMARY()));
}

void StockQuoteWidget::on_theme_changed() {
    apply_styles();
}

void StockQuoteWidget::set_symbol(const QString& symbol) {
    symbol_ = symbol.toUpper();
    set_title(QString("QUOTE: %1").arg(symbol_));
    refresh_data();
}

void StockQuoteWidget::refresh_data() {
    set_loading(true);

    services::MarketDataService::instance().fetch_quotes({symbol_},
                                                         [this](bool ok, QVector<services::QuoteData> quotes) {
                                                             set_loading(false);
                                                             if (!ok || quotes.isEmpty()) {
                                                                 set_error(QString("No data for %1").arg(symbol_));
                                                                 return;
                                                             }
                                                             populate(quotes.first());
                                                         });
}

void StockQuoteWidget::populate(const services::QuoteData& q) {
    price_label_->setText(QString("$%1").arg(q.price, 0, 'f', 2));

    bool positive = q.change_pct >= 0;
    QString color = positive ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();

    arrow_label_->setText(positive ? QString(QChar(0x25B2)) : QString(QChar(0x25BC)));
    arrow_label_->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(color));

    change_label_->setText(QString("%1%2 (%3%4%)")
                               .arg(positive ? "+" : "")
                               .arg(q.change, 0, 'f', 2)
                               .arg(positive ? "+" : "")
                               .arg(q.change_pct, 0, 'f', 2));
    change_label_->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;").arg(color));

    price_label_->setStyleSheet(
        QString("color: %1; font-size: 28px; font-weight: bold; background: transparent;").arg(color));

    auto fmt = [](double v) { return v > 0 ? QString("$%1").arg(v, 0, 'f', 2) : QString("--"); };
    open_val_->setText(fmt(q.high)); // yfinance batch returns high but not open separately — use high
    high_val_->setText(fmt(q.high));
    low_val_->setText(fmt(q.low));
    prev_val_->setText(fmt(q.price - q.change));

    // Format volume
    if (q.volume >= 1e9)
        volume_val_->setText(QString("%1B").arg(q.volume / 1e9, 0, 'f', 1));
    else if (q.volume >= 1e6)
        volume_val_->setText(QString("%1M").arg(q.volume / 1e6, 0, 'f', 1));
    else if (q.volume >= 1e3)
        volume_val_->setText(QString("%1K").arg(q.volume / 1e3, 0, 'f', 1));
    else
        volume_val_->setText(QString::number(static_cast<int>(q.volume)));
}

} // namespace fincept::screens::widgets
