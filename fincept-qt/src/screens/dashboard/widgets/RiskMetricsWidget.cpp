#include "screens/dashboard/widgets/RiskMetricsWidget.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>

#include <cmath>

namespace fincept::screens::widgets {

// Symbols: VIX + SPY/QQQ/IWM/TLT for correlation proxies + 6 volatile stocks
static const QStringList kRiskSymbols = {"^VIX", "SPY", "QQQ",  "IWM",  "TLT", "NVDA",
                                         "TSLA", "AMD", "META", "PLTR", "COIN"};

RiskMetricsWidget::RiskMetricsWidget(QWidget* parent) : BaseWidget("RISK METRICS", parent, ui::colors::NEGATIVE) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    // ── VIX section ──
    auto* vix_card = new QWidget;
    vix_card->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(ui::colors::BG_RAISED));
    auto* vcl = new QVBoxLayout(vix_card);
    vcl->setContentsMargins(10, 8, 10, 8);
    vcl->setSpacing(4);

    auto* vix_header = new QHBoxLayout;
    auto* vix_lbl = new QLabel("VIX FEAR GAUGE");
    vix_lbl->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                               .arg(ui::colors::TEXT_TERTIARY));
    vix_header->addWidget(vix_lbl);
    vix_header->addStretch();

    vix_regime_ = new QLabel("--");
    vix_regime_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                   .arg(ui::colors::TEXT_TERTIARY));
    vix_header->addWidget(vix_regime_);
    vcl->addLayout(vix_header);

    vix_value_ = new QLabel("--");
    vix_value_->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold; background: transparent;")
                                  .arg(ui::colors::TEXT_TERTIARY));
    vcl->addWidget(vix_value_);

    // VIX gradient bar (0–40 range, 5 segments)
    auto* bar_row = new QWidget;
    bar_row->setFixedHeight(6);
    auto* brl = new QHBoxLayout(bar_row);
    brl->setContentsMargins(0, 0, 0, 0);
    brl->setSpacing(1);
    static const char* bar_colors[] = {"#16a34a", "#65a30d", "#ca8a04", "#ea580c", "#dc2626"};
    for (auto* c : bar_colors) {
        auto* seg = new QFrame;
        seg->setStyleSheet(QString("background: %1; border-radius: 0;").arg(c));
        brl->addWidget(seg, 1);
    }
    vcl->addWidget(bar_row);

    // Bar position indicator
    vix_bar_fill_ = new QLabel("▲");
    vix_bar_fill_->setStyleSheet(
        QString("color: %1; font-size: 8px; background: transparent;").arg(ui::colors::TEXT_TERTIARY));
    vix_bar_fill_->setAlignment(Qt::AlignLeft);
    vcl->addWidget(vix_bar_fill_);

    vl->addWidget(vix_card);

    // ── Section separator ──
    auto* sep1 = new QFrame;
    sep1->setFixedHeight(1);
    sep1->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM));
    vl->addWidget(sep1);

    // ── Volatile stocks section ──
    auto* stocks_hdr = new QLabel("HIGH-BETA STOCKS");
    stocks_hdr->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                  .arg(ui::colors::TEXT_TERTIARY));
    vl->addWidget(stocks_hdr);

    static const QStringList kVolatile = {"NVDA", "TSLA", "AMD", "META", "PLTR", "COIN"};
    for (const auto& sym : kVolatile) {
        auto* row = new QWidget;
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 2, 0, 2);

        StockRisk sr;
        sr.symbol = new QLabel(sym);
        sr.symbol->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                     .arg(ui::colors::TEXT_PRIMARY));
        sr.symbol->setFixedWidth(48);
        rl->addWidget(sr.symbol);

        sr.chg_pct = new QLabel("--");
        sr.chg_pct->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                      .arg(ui::colors::TEXT_TERTIARY));
        sr.chg_pct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rl->addWidget(sr.chg_pct, 1);

        sr.hi_lo = new QLabel("--");
        sr.hi_lo->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY));
        sr.hi_lo->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rl->addWidget(sr.hi_lo, 1);

        stock_rows_.append(sr);
        vl->addWidget(row);
    }

    // ── Correlation proxies ──
    auto* sep2 = new QFrame;
    sep2->setFixedHeight(1);
    sep2->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM));
    vl->addWidget(sep2);

    auto* corr_hdr = new QLabel("SPREAD PROXIES");
    corr_hdr->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                .arg(ui::colors::TEXT_TERTIARY));
    vl->addWidget(corr_hdr);

    auto make_spread_row = [&](const QString& label, QLabel*& out) {
        auto* row = new QWidget;
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 2, 0, 2);
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_SECONDARY));
        rl->addWidget(lbl);
        rl->addStretch();
        out = new QLabel("--");
        out->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                               .arg(ui::colors::TEXT_PRIMARY));
        rl->addWidget(out);
        vl->addWidget(row);
    };

    make_spread_row("SPY vs QQQ spread", spy_qqq_spread_);
    make_spread_row("SPY vs IWM spread", spy_iwm_spread_);
    make_spread_row("Equity/Bond (SPY-TLT)", equity_bond_lbl_);

    vl->addStretch();

    connect(this, &BaseWidget::refresh_requested, this, &RiskMetricsWidget::refresh_data);
    set_loading(true);
    refresh_data();
}

void RiskMetricsWidget::refresh_data() {
    set_loading(true);
    services::MarketDataService::instance().fetch_quotes(kRiskSymbols,
                                                         [this](bool ok, QVector<services::QuoteData> quotes) {
                                                             set_loading(false);
                                                             if (!ok || quotes.isEmpty())
                                                                 return;
                                                             populate(quotes);
                                                         });
}

void RiskMetricsWidget::populate(const QVector<services::QuoteData>& quotes) {
    QMap<QString, const services::QuoteData*> map;
    for (const auto& q : quotes)
        map[q.symbol] = &q;

    // VIX
    if (map.contains("^VIX")) {
        double vix = map["^VIX"]->price;
        vix_value_->setText(QString::number(vix, 'f', 2));
        QString regime, color;
        if (vix < 15) {
            regime = "LOW VOLATILITY";
            color = ui::colors::POSITIVE;
        } else if (vix < 20) {
            regime = "NORMAL";
            color = ui::colors::TEXT_PRIMARY;
        } else if (vix < 30) {
            regime = "ELEVATED";
            color = ui::colors::WARNING;
        } else if (vix < 40) {
            regime = "HIGH STRESS";
            color = ui::colors::NEGATIVE;
        } else {
            regime = "EXTREME FEAR";
            color = "#ff0000";
        }
        vix_value_->setStyleSheet(
            QString("color: %1; font-size: 22px; font-weight: bold; background: transparent;").arg(color));
        vix_regime_->setText(regime);
        vix_regime_->setStyleSheet(
            QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;").arg(color));

        // Position indicator on 5-segment bar (0-40 range, 8 per segment)
        double pct = qBound(0.0, vix / 40.0, 1.0);
        int margin_left = static_cast<int>(pct * 140); // approx bar width
        vix_bar_fill_->setStyleSheet(QString("color: %1; font-size: 8px; background: transparent; margin-left: %2px;")
                                         .arg(color)
                                         .arg(margin_left));
    }

    // High-beta stocks
    static const QStringList kVolatile = {"NVDA", "TSLA", "AMD", "META", "PLTR", "COIN"};
    for (int i = 0; i < kVolatile.size() && i < stock_rows_.size(); ++i) {
        const QString& sym = kVolatile[i];
        if (!map.contains(sym))
            continue;
        const auto& q = *map[sym];

        double chg = q.change_pct;
        QString chg_str = QString("%1%2%").arg(chg >= 0 ? "+" : "").arg(chg, 0, 'f', 2);
        QString chg_col = chg > 0 ? ui::colors::POSITIVE : chg < 0 ? ui::colors::NEGATIVE : ui::colors::TEXT_PRIMARY;
        stock_rows_[i].chg_pct->setText(chg_str);
        stock_rows_[i].chg_pct->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;").arg(chg_col));

        // Hi/Lo as range: H/L
        if (q.high > 0 && q.low > 0) {
            stock_rows_[i].hi_lo->setText(QString("H%1 L%2").arg(q.high, 0, 'f', 0).arg(q.low, 0, 'f', 0));
        }
    }

    // Spread proxies
    auto spread_str = [](double a, double b) -> QString {
        double diff = a - b;
        return QString("%1%2%").arg(diff >= 0 ? "+" : "").arg(diff, 0, 'f', 2);
    };
    auto spread_color = [](double a, double b) -> QString {
        double diff = a - b;
        return diff > 0 ? ui::colors::POSITIVE : diff < 0 ? ui::colors::NEGATIVE : ui::colors::TEXT_PRIMARY;
    };

    if (map.contains("SPY") && map.contains("QQQ")) {
        double d = map["SPY"]->change_pct - map["QQQ"]->change_pct;
        spy_qqq_spread_->setText(spread_str(map["SPY"]->change_pct, map["QQQ"]->change_pct));
        spy_qqq_spread_->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                .arg(spread_color(map["SPY"]->change_pct, map["QQQ"]->change_pct)));
        (void)d;
    }
    if (map.contains("SPY") && map.contains("IWM")) {
        spy_iwm_spread_->setText(spread_str(map["SPY"]->change_pct, map["IWM"]->change_pct));
        spy_iwm_spread_->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                .arg(spread_color(map["SPY"]->change_pct, map["IWM"]->change_pct)));
    }
    if (map.contains("SPY") && map.contains("TLT")) {
        equity_bond_lbl_->setText(spread_str(map["SPY"]->change_pct, map["TLT"]->change_pct));
        equity_bond_lbl_->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                .arg(spread_color(map["SPY"]->change_pct, map["TLT"]->change_pct)));
    }
}

} // namespace fincept::screens::widgets
