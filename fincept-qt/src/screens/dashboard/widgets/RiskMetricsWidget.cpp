#include "screens/dashboard/widgets/RiskMetricsWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/DashboardFormat.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>

#include <cmath>

namespace fincept::screens::widgets {

// Symbols: VIX + SPY/QQQ/IWM/TLT for correlation proxies + 6 volatile stocks
static const QStringList kRiskSymbols = {"^VIX", "SPY", "QQQ",  "IWM",  "TLT", "NVDA",
                                         "TSLA", "AMD", "META", "PLTR", "COIN"};

RiskMetricsWidget::RiskMetricsWidget(QWidget* parent) : BaseWidget(tr("RISK METRICS"), parent, ui::colors::NEGATIVE()) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    // ── VIX section ──
    vix_card_ = new QWidget(this);
    auto* vcl = new QVBoxLayout(vix_card_);
    vcl->setContentsMargins(10, 8, 10, 8);
    vcl->setSpacing(4);

    auto* vix_header = new QHBoxLayout;
    vix_header_lbl_ = new QLabel(tr("VIX FEAR GAUGE"));
    vix_header->addWidget(vix_header_lbl_);
    vix_header->addStretch();

    vix_regime_ = new QLabel("--");
    vix_header->addWidget(vix_regime_);
    vcl->addLayout(vix_header);

    vix_value_ = new QLabel("--");
    vcl->addWidget(vix_value_);

    // VIX gradient bar (0-40 range, 5 segments) — uses theme tokens
    auto* bar_row = new QWidget(this);
    bar_row->setFixedHeight(6);
    auto* brl = new QHBoxLayout(bar_row);
    brl->setContentsMargins(0, 0, 0, 0);
    brl->setSpacing(1);
    for (int i = 0; i < 5; ++i) {
        auto* seg = new QFrame;
        vix_bar_segments_.append(seg);
        brl->addWidget(seg, 1);
    }
    vcl->addWidget(bar_row);

    // Bar position indicator
    vix_bar_fill_ = new QLabel(QString::fromUtf8("\xe2\x96\xb2"));
    vix_bar_fill_->setAlignment(Qt::AlignLeft);
    vcl->addWidget(vix_bar_fill_);

    vl->addWidget(vix_card_);

    // ── Section separator ──
    sep1_ = new QFrame;
    sep1_->setFixedHeight(1);
    vl->addWidget(sep1_);

    // ── Volatile stocks section ──
    stocks_hdr_ = new QLabel(tr("HIGH-BETA STOCKS"));
    vl->addWidget(stocks_hdr_);

    static const QStringList kVolatile = {"NVDA", "TSLA", "AMD", "META", "PLTR", "COIN"};
    for (const auto& sym : kVolatile) {
        auto* row = new QWidget(this);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 2, 0, 2);

        StockRisk sr;
        sr.symbol = new QLabel(sym);
        sr.symbol->setFixedWidth(48);
        rl->addWidget(sr.symbol);

        sr.chg_pct = new QLabel("--");
        sr.chg_pct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rl->addWidget(sr.chg_pct, 1);

        sr.hi_lo = new QLabel("--");
        sr.hi_lo->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rl->addWidget(sr.hi_lo, 1);

        stock_rows_.append(sr);
        vl->addWidget(row);
    }

    // ── Correlation proxies ──
    sep2_ = new QFrame;
    sep2_->setFixedHeight(1);
    vl->addWidget(sep2_);

    corr_hdr_ = new QLabel(tr("SPREAD PROXIES"));
    vl->addWidget(corr_hdr_);

    auto make_spread_row = [&](const QString& label, QLabel*& out) {
        auto* row = new QWidget(this);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 2, 0, 2);
        auto* lbl = new QLabel(label);
        spread_labels_.append(lbl);
        rl->addWidget(lbl);
        rl->addStretch();
        out = new QLabel("--");
        rl->addWidget(out);
        vl->addWidget(row);
    };

    // Differences of two daily % returns — percentage points, not percent.
    make_spread_row(tr("SPY − QQQ day return"), spy_qqq_spread_);
    make_spread_row(tr("SPY − IWM day return"), spy_iwm_spread_);
    make_spread_row(tr("SPY − TLT day return"), equity_bond_lbl_);
    corr_hdr_->setToolTip(tr("Today's return of the first ETF minus the second, in percentage points (pp)."));

    vl->addStretch();

    connect(this, &BaseWidget::refresh_requested, this, &RiskMetricsWidget::refresh_data);

    apply_styles();
    set_loading(true);
}

void RiskMetricsWidget::apply_styles() {
    // Theme change: the data-driven colours below must be re-applied next tick.
    for (QWidget* w : {static_cast<QWidget*>(vix_value_), static_cast<QWidget*>(vix_regime_),
                       static_cast<QWidget*>(vix_bar_fill_), static_cast<QWidget*>(spy_qqq_spread_),
                       static_cast<QWidget*>(spy_iwm_spread_), static_cast<QWidget*>(equity_bond_lbl_)})
        reset_css_cache(w);
    for (const auto& sr : stock_rows_)
        reset_css_cache(sr.chg_pct);
    vix_card_->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(ui::colors::BG_RAISED()));
    vix_header_lbl_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                       .arg(ui::colors::TEXT_TERTIARY()));
    vix_regime_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
    vix_value_->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold; background: transparent;")
                                  .arg(ui::colors::TEXT_TERTIARY()));
    vix_bar_fill_->setStyleSheet(
        QString("color: %1; font-size: 8px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));

    // VIX gradient bar: green → yellow-green → amber → orange → red (theme-aware)
    if (vix_bar_segments_.size() == 5) {
        vix_bar_segments_[0]->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::POSITIVE()));
        vix_bar_segments_[1]->setStyleSheet(
            QString("background: %1; border-radius: 0;").arg(ui::colors::POSITIVE_DIM()));
        vix_bar_segments_[2]->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::WARNING()));
        vix_bar_segments_[3]->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::AMBER()));
        vix_bar_segments_[4]->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::NEGATIVE()));
    }

    sep1_->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM()));
    sep2_->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM()));

    stocks_hdr_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
    corr_hdr_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                 .arg(ui::colors::TEXT_TERTIARY()));

    for (const auto& sr : stock_rows_) {
        sr.symbol->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                     .arg(ui::colors::TEXT_PRIMARY()));
        sr.chg_pct->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                      .arg(ui::colors::TEXT_TERTIARY()));
        sr.hi_lo->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    }

    for (auto* lbl : spread_labels_)
        lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));

    spy_qqq_spread_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                       .arg(ui::colors::TEXT_PRIMARY()));
    spy_iwm_spread_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                       .arg(ui::colors::TEXT_PRIMARY()));
    equity_bond_lbl_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                        .arg(ui::colors::TEXT_PRIMARY()));
}

void RiskMetricsWidget::on_theme_changed() {
    apply_styles();
}

void RiskMetricsWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_subscribe_all();
}

void RiskMetricsWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void RiskMetricsWidget::refresh_data() {
    auto& hub = datahub::DataHub::instance();
    QStringList topics;
    topics.reserve(kRiskSymbols.size());
    for (const auto& sym : kRiskSymbols)
        topics.append(QStringLiteral("market:quote:") + sym);
    hub.request(topics, /*force=*/true); // user-triggered: bypass min_interval
}

void RiskMetricsWidget::hub_subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    set_loading_progress(row_cache_.size(), kRiskSymbols.size());
    for (const auto& sym : kRiskSymbols) {
        const QString topic = QStringLiteral("market:quote:") + sym;
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            row_cache_.insert(sym, v.value<services::QuoteData>());
            set_loading_progress(row_cache_.size(), kRiskSymbols.size());
            // One redraw per delivery burst, not one per symbol.
            schedule_render([this]() { rebuild_from_cache(); });
        });
    }
    hub_active_ = true;
}

void RiskMetricsWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void RiskMetricsWidget::rebuild_from_cache() {
    QVector<services::QuoteData> quotes;
    quotes.reserve(row_cache_.size());
    for (const auto& sym : kRiskSymbols) {
        auto it = row_cache_.constFind(sym);
        if (it != row_cache_.constEnd())
            quotes.append(it.value());
    }
    if (!quotes.isEmpty())
        populate(quotes);
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
            regime = tr("LOW VOLATILITY");
            color = ui::colors::POSITIVE();
        } else if (vix < 20) {
            regime = tr("NORMAL");
            color = ui::colors::TEXT_PRIMARY();
        } else if (vix < 30) {
            regime = tr("ELEVATED");
            color = ui::colors::WARNING();
        } else if (vix < 40) {
            regime = tr("HIGH STRESS");
            color = ui::colors::NEGATIVE();
        } else {
            regime = tr("EXTREME FEAR");
            color = ui::colors::NEGATIVE();
        }
        set_css_cached(vix_value_,
                       QString("color: %1; font-size: 22px; font-weight: bold; background: transparent;").arg(color));
        vix_regime_->setText(regime);
        set_css_cached(vix_regime_,
                       QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;").arg(color));

        // Marker on the 5-segment 0-40 bar. Positioned with a contents margin
        // from the bar's real width (was a hard-coded 140 px inside a
        // stylesheet, re-parsed on every tick and wrong at any other size).
        const double pct = qBound(0.0, vix / 40.0, 1.0);
        const int bar_w = vix_bar_fill_->width() > 12 ? vix_bar_fill_->width() - 8 : 140;
        vix_bar_fill_->setContentsMargins(static_cast<int>(pct * bar_w), 0, 0, 0);
        set_css_cached(vix_bar_fill_, QString("color: %1; font-size: 8px; background: transparent;").arg(color));
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
        QString chg_col = chg > 0   ? ui::colors::POSITIVE()
                          : chg < 0 ? ui::colors::NEGATIVE()
                                    : ui::colors::TEXT_PRIMARY();
        stock_rows_[i].chg_pct->setText(chg_str);
        set_css_cached(stock_rows_[i].chg_pct,
                       QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;").arg(chg_col));

        // Day range. Was printed with 0 decimals ("H31 L29" for a $30 stock).
        if (q.high > 0 && q.low > 0)
            stock_rows_[i].hi_lo->setText(
                QString("H %1  L %2").arg(dash_fmt::price(q.high, sym), dash_fmt::price(q.low, sym)));
    }

    // Spread proxies
    auto spread_str = [](double a, double b) -> QString { return dash_fmt::signed_pp(a - b); };
    auto spread_color = [](double a, double b) -> QString {
        double diff = a - b;
        return diff > 0 ? ui::colors::POSITIVE() : diff < 0 ? ui::colors::NEGATIVE() : ui::colors::TEXT_PRIMARY();
    };

    if (map.contains("SPY") && map.contains("QQQ")) {
        spy_qqq_spread_->setText(spread_str(map["SPY"]->change_pct, map["QQQ"]->change_pct));
        set_css_cached(spy_qqq_spread_, QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                            .arg(spread_color(map["SPY"]->change_pct, map["QQQ"]->change_pct)));
    }
    if (map.contains("SPY") && map.contains("IWM")) {
        spy_iwm_spread_->setText(spread_str(map["SPY"]->change_pct, map["IWM"]->change_pct));
        set_css_cached(spy_iwm_spread_, QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                            .arg(spread_color(map["SPY"]->change_pct, map["IWM"]->change_pct)));
    }
    if (map.contains("SPY") && map.contains("TLT")) {
        equity_bond_lbl_->setText(spread_str(map["SPY"]->change_pct, map["TLT"]->change_pct));
        set_css_cached(equity_bond_lbl_, QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                             .arg(spread_color(map["SPY"]->change_pct, map["TLT"]->change_pct)));
    }
}

void RiskMetricsWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("RISK METRICS"));
    rebuild_from_cache(); // re-renders header/regime/section labels in the new language
}

} // namespace fincept::screens::widgets
