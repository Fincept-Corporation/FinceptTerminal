// src/screens/portfolio/PortfolioPerfChart.cpp
#include "screens/portfolio/PortfolioPerfChart.h"

#include "screens/portfolio/PortfolioFormat.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QDate>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

const QStringList& ppc_presets() {
    static const QStringList p = {"1W", "1M", "3M", "YTD", "1Y", "5Y", "ALL"};
    return p;
}

/// Value of a (date-sorted) series at the last point on/before x; NaN when none.
double ppc_asof(const QVector<QPointF>& pts, double x) {
    double v = portfolio::kAbsent;
    for (const auto& p : pts) {
        if (p.x() > x)
            break;
        if (std::isfinite(p.y()))
            v = p.y();
    }
    return v;
}

} // namespace

PortfolioPerfChart::PortfolioPerfChart(QWidget* parent) : QWidget(parent) {
    build_ui();
    // Currency / number format changed: the axis / tooltip formatters run at paint time;
    // the cost line's label and the info strip are baked text (zoom is kept).
    ui::formatting::on_format_changed(this, [this]() {
        if (mode_ == Mode::Value)
            rebuild_series();
        else {
            chart_->update();
            update_info();
        }
    });
}

void PortfolioPerfChart::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto header = ui::make_panel_header(tr("PERFORMANCE"), this);
    title_label_ = header.title_label;
    auto* slot = header.controls_slot->layout();
    auto make_toggle = [this](const QString& text, const QString& tip) {
        auto* b = new QPushButton(text, this);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        b->setObjectName("ppcToggle");
        return b;
    };
    value_btn_ = make_toggle(tr("VALUE"), tr("Net asset value in the base currency (flows included)"));
    return_btn_ = make_toggle(tr("RETURN"), tr("Time-weighted return vs the benchmark, rebased to the visible period"));
    bench_btn_ = make_toggle(tr("BENCH"), tr("Show the benchmark in RETURN mode"));
    bench_btn_->setChecked(true);
    slot->addWidget(value_btn_);
    slot->addWidget(return_btn_);
    slot->addWidget(bench_btn_);
    connect(value_btn_, &QPushButton::clicked, this, [this]() { set_mode(Mode::Value); });
    connect(return_btn_, &QPushButton::clicked, this, [this]() { set_mode(Mode::Return); });
    connect(bench_btn_, &QPushButton::toggled, this, [this](bool on) {
        show_benchmark_ = on;
        if (bench_series_ > 0)
            chart_->set_series_visible(bench_series_, on && mode_ == Mode::Return);
        update_info();
    });
    layout->addWidget(header.header);

    info_label_ = new QLabel(this);
    info_label_->setObjectName("ppcInfo");
    info_label_->setTextFormat(Qt::RichText);
    info_label_->setContentsMargins(10, 3, 10, 3);
    layout->addWidget(info_label_);

    chart_ = new ui::TimeSeriesChart(this);
    chart_->set_utc(true); // daily NAV rows are calendar dates
    chart_->set_range_presets(ppc_presets());
    chart_->set_active_preset(QStringLiteral("1Y"));
    chart_->set_show_mode_buttons(false);
    chart_->set_export_name(QStringLiteral("portfolio_performance"));
    chart_->set_time_formatter([](double x) { return ui::formatting::bar_date(static_cast<qint64>(x / 1000.0)); });
    connect(chart_, &ui::TimeSeriesChart::range_changed, this, [this](double, double) { update_info(); });
    connect(chart_, &ui::TimeSeriesChart::range_preset_selected, this, [this](const QString& p) {
        // Ask for history the stored snapshots do not cover yet.
        if (snapshots_.isEmpty())
            return;
        const QDate earliest = pfmt::iso_date(snapshots_.first().snapshot_date);
        const int needed = (p == "5Y" || p == "ALL") ? 1825 : (p == "1Y" || p == "YTD") ? 365 : p == "3M" ? 95 : 32;
        if (earliest.isValid() && earliest.daysTo(QDate::currentDate()) < needed - 5)
            emit backfill_period_requested((p == "5Y" || p == "ALL") ? QStringLiteral("5y") : QStringLiteral("1y"));
    });
    layout->addWidget(chart_, 1);

    set_mode(Mode::Value);
    apply_styles();
}

void PortfolioPerfChart::apply_styles() {
    // Also runs on every appearance change (PortfolioScreen::refresh_theme): set only a
    // changed sheet (each set re-polishes the widget).
    const auto set_css = [](QWidget* w, const QString& css) {
        if (w && w->styleSheet() != css)
            w->setStyleSheet(css);
    };
    const QString toggle = QString("QPushButton#ppcToggle { background:transparent; color:%1; border:1px solid %2;"
                                   "  padding:1px 8px; font-size:%3px; font-weight:700; }"
                                   "QPushButton#ppcToggle:checked { color:%4; border-color:%4; background:%5; }"
                                   "QPushButton#ppcToggle:hover { color:%6; }")
                               .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::fonts::px(-4),
                                    ui::colors::AMBER(), ui::colors::ACCENT_BG(), ui::colors::TEXT_PRIMARY());
    for (auto* b : {value_btn_, return_btn_, bench_btn_})
        set_css(b, toggle);
    set_css(info_label_, QString("#ppcInfo { background:%1; color:%2; font-size:%3px; border-bottom:1px solid %4; }")
                             .arg(ui::colors::BG_BASE(), ui::colors::TEXT_SECONDARY(), ui::fonts::px(-3),
                                  ui::colors::BORDER_DIM()));
}

void PortfolioPerfChart::set_mode(Mode m) {
    mode_ = m;
    value_btn_->setChecked(m == Mode::Value);
    return_btn_->setChecked(m == Mode::Return);
    bench_btn_->setEnabled(m == Mode::Return);
    rebuild_series();
}

void PortfolioPerfChart::set_summary(const portfolio::PortfolioSummary& summary) {
    const bool cost_changed = std::abs(summary.total_cost_basis - summary_.total_cost_basis) > 0.005;
    summary_ = summary;
    if (cost_changed && mode_ == Mode::Value)
        rebuild_series();
    else
        update_info();
}

void PortfolioPerfChart::set_snapshots(const QVector<portfolio::PortfolioSnapshot>& snapshots) {
    snapshots_ = snapshots;
    std::sort(snapshots_.begin(), snapshots_.end(),
              [](const auto& a, const auto& b) { return a.snapshot_date < b.snapshot_date; });
    rebuild_series();
}

void PortfolioPerfChart::set_currency(const QString& currency) {
    if (currency.isEmpty() || currency == currency_)
        return;
    currency_ = currency;
    rebuild_series();
}

void PortfolioPerfChart::set_benchmark_history(const QString& symbol, const QStringList& dates,
                                               const QVector<double>& closes) {
    bench_symbol_ = symbol;
    bench_dates_ = dates;
    bench_closes_ = closes;
    if (mode_ == Mode::Return)
        rebuild_series();
    else
        update_info();
}

void PortfolioPerfChart::set_twr_series(const QVector<portfolio::TwrPoint>& series) {
    twr_ = series;
    if (mode_ == Mode::Return)
        rebuild_series();
    else
        update_info();
}

void PortfolioPerfChart::rebuild_series() {
    const QString preset = chart_->active_preset();
    const double from = chart_->x_from();
    const double to = chart_->x_to();
    const bool had_data = nav_series_ > 0 || twr_series_id_ > 0;
    chart_->clear();
    nav_series_ = twr_series_id_ = bench_series_ = -1;
    const QString ccy = currency_;

    if (mode_ == Mode::Value) {
        chart_->set_value_mode(ui::TimeSeriesChart::ValueMode::Absolute);
        QVector<QPointF> pts;
        pts.reserve(snapshots_.size());
        for (const auto& s : snapshots_) {
            const double x = pfmt::utc_ms(s.snapshot_date);
            if (std::isfinite(x) && s.total_value > 0)
                pts.append({x, s.total_value});
        }
        ui::TimeSeriesChart::SeriesSpec spec;
        spec.name = tr("NAV (%1)").arg(ccy);
        spec.points = pts;
        spec.style = ui::TimeSeriesChart::Style::Area;
        spec.color = ui::TimeSeriesChart::ColorRole::Accent;
        spec.rebase = false;
        spec.formatter = [ccy](double v) { return pfmt::money(v, ccy); };
        nav_series_ = chart_->add_series(spec);
        chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [ccy](double v) { return pfmt::money_c(v, ccy); });
        if (summary_.total_cost_basis > 0 && summary_.complete)
            chart_->add_hline(summary_.total_cost_basis, tr("Cost %1").arg(pfmt::money_c(summary_.total_cost_basis, ccy)),
                              ui::TimeSeriesChart::ColorRole::Muted);
        chart_->set_state(pts.isEmpty() ? ui::TimeSeriesChart::State::Empty : ui::TimeSeriesChart::State::Ready,
                          tr("No NAV history yet — it is rebuilt from the ledger; check back after the first "
                             "refresh."));
    } else {
        // TWR index and benchmark level, both rebased to the first visible point.
        chart_->set_value_mode(ui::TimeSeriesChart::ValueMode::Percent);
        QVector<QPointF> idx;
        idx.reserve(twr_.size());
        for (const auto& p : twr_) {
            const double x = pfmt::utc_ms(p.date);
            if (std::isfinite(x) && std::isfinite(p.cumulative))
                idx.append({x, 1.0 + p.cumulative});
        }
        ui::TimeSeriesChart::SeriesSpec ts;
        ts.name = tr("Portfolio (TWR)");
        ts.points = idx;
        ts.color = ui::TimeSeriesChart::ColorRole::Accent;
        ts.line_width = 2.0;
        twr_series_id_ = chart_->add_series(ts);
        if (!bench_dates_.isEmpty()) {
            QVector<QPointF> b;
            for (int i = 0; i < bench_dates_.size() && i < bench_closes_.size(); ++i) {
                const double x = pfmt::utc_ms(bench_dates_[i]);
                if (std::isfinite(x) && bench_closes_[i] > 0)
                    b.append({x, bench_closes_[i]});
            }
            ui::TimeSeriesChart::SeriesSpec bs;
            bs.name = bench_symbol_.isEmpty() ? tr("Benchmark") : bench_symbol_;
            bs.points = b;
            bs.color = ui::TimeSeriesChart::ColorRole::Muted;
            bs.dashed = true;
            bs.visible = show_benchmark_;
            bench_series_ = chart_->add_series(bs);
        }
        chart_->set_state(idx.size() < 2 ? ui::TimeSeriesChart::State::Empty : ui::TimeSeriesChart::State::Ready,
                          tr("Not enough history for a time-weighted return yet."));
    }
    // Keep the user's zoom / preset across data refreshes.
    if (had_data && preset.isEmpty() && std::isfinite(from) && std::isfinite(to) && to > from)
        chart_->set_x_range(from, to);
    else
        chart_->set_active_preset(preset.isEmpty() ? QStringLiteral("1Y") : preset);
    update_info();
}

void PortfolioPerfChart::update_info() {
    if (!info_label_)
        return;
    const double from = chart_->x_from();
    const double to = chart_->x_to();
    QStringList parts;
    // Period TWR over the visible window.
    QVector<QPointF> idx;
    for (const auto& p : twr_) {
        const double x = pfmt::utc_ms(p.date);
        if (std::isfinite(x))
            idx.append({x, 1.0 + p.cumulative});
    }
    const double i0 = ppc_asof(idx, from);
    const double i1 = ppc_asof(idx, to);
    const bool have_ret = std::isfinite(i0) && std::isfinite(i1) && i0 > 0;
    const double ret = have_ret ? (i1 / i0 - 1.0) * 100.0 : portfolio::kAbsent;
    parts << tr("Period TWR <b style='color:%1'>%2</b>").arg(pfmt::dir_css(ret), pfmt::pct(ret));
    if (!bench_dates_.isEmpty()) {
        QVector<QPointF> b;
        for (int i = 0; i < bench_dates_.size() && i < bench_closes_.size(); ++i)
            b.append({pfmt::utc_ms(bench_dates_[i]), bench_closes_[i]});
        const double b0 = ppc_asof(b, from);
        const double b1 = ppc_asof(b, to);
        const double br = std::isfinite(b0) && std::isfinite(b1) && b0 > 0 ? (b1 / b0 - 1.0) * 100.0 : portfolio::kAbsent;
        parts << tr("%1 <b>%2</b>").arg(bench_symbol_.toHtmlEscaped(), pfmt::pct(br));
        if (have_ret && std::isfinite(br))
            parts << tr("Excess <b style='color:%1'>%2</b>").arg(pfmt::dir_css(ret - br), pfmt::pp(ret - br));
    }
    parts << tr("NAV <b>%1</b>").arg(pfmt::approx_prefix(summary_) + pfmt::money(summary_.total_market_value, currency_)) +
                 (summary_.complete ? QString() : QStringLiteral(" <span style='color:%1'>⚠</span>").arg(ui::colors::WARNING()));
    parts << tr("Cost %1").arg(pfmt::money(summary_.total_cost_basis, currency_));
    info_label_->setText(parts.join(QStringLiteral("&nbsp;&nbsp;·&nbsp;&nbsp;")));
    info_label_->setToolTip((summary_.complete ? tr("Time-weighted return: deposits, withdrawals and trades are not "
                                                    "counted as performance.")
                                               : pfmt::incomplete_line(summary_)) +
                            (summary_.fx_stale.isEmpty() ? QString() : QLatin1Char('\n') + pfmt::fx_stale_line(summary_)));
}

void PortfolioPerfChart::refresh_theme() {
    apply_styles();
    update_info();
}

void PortfolioPerfChart::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioPerfChart::retranslateUi() {
    if (title_label_)
        title_label_->setText(tr("PERFORMANCE"));
    value_btn_->setText(tr("VALUE"));
    return_btn_->setText(tr("RETURN"));
    bench_btn_->setText(tr("BENCH"));
    rebuild_series();
}

} // namespace fincept::screens
