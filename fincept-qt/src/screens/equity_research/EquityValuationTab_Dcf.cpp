// src/screens/equity_research/EquityValuationTab_Dcf.cpp
//
// The model run and its renderings: results with the EV → equity bridge, bear / base /
// bull scenarios, the projection table and chart, the sensitivity grid, and multiples
// against the company's own history and its peers.
#include "screens/equity_research/EquityResearchFormat.h"
#include "screens/equity_research/EquityStatementModel.h"
#include "screens/equity_research/EquityValuationTab.h"
#include "services/equity/EquityResearchService.h"
#include "services/equity/EquityTechnicalMath.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QHeaderView>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QTimeZone>

#include <algorithm>
#include <climits>
#include <cmath>
#include <map>

namespace fincept::screens {

namespace valuation_dcf_detail {

namespace vd_uf = fincept::ui::formatting;
namespace vd_fm = fin_model;
namespace vd_val = services::equity::valuation;

bool vd_ok(double v) {
    return std::isfinite(v);
}

void vd_tone(QLabel* l, const char* tone) {
    l->setProperty("tone", QString::fromLatin1(tone));
    l->style()->unpolish(l);
    l->style()->polish(l);
}

QTableWidgetItem* vd_item(const QString& text, Qt::Alignment align = Qt::AlignRight | Qt::AlignVCenter) {
    auto* it = new QTableWidgetItem(text);
    it->setTextAlignment(align);
    return it;
}

struct VdStats {
    double mean = std::nan(""), sd = std::nan(""), min = std::nan(""), max = std::nan(""), last = std::nan("");
};

VdStats vd_stats(const QVector<double>& v) {
    VdStats s;
    QVector<double> x;
    for (double d : v)
        if (vd_ok(d))
            x << d;
    if (x.isEmpty())
        return s;
    s.last = x.last();
    s.min = *std::min_element(x.begin(), x.end());
    s.max = *std::max_element(x.begin(), x.end());
    double m = 0;
    for (double d : x)
        m += d;
    m /= x.size();
    double ss = 0;
    for (double d : x)
        ss += (d - m) * (d - m);
    s.mean = m;
    s.sd = x.size() > 1 ? std::sqrt(ss / (x.size() - 1)) : 0.0;
    return s;
}

double vd_median(QVector<double> v) {
    v.erase(std::remove_if(v.begin(), v.end(), [](double d) { return !(d > 0.0) || !std::isfinite(d); }), v.end());
    if (v.isEmpty())
        return std::nan("");
    std::sort(v.begin(), v.end());
    const qsizetype n = v.size();
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

} // namespace valuation_dcf_detail

using namespace valuation_dcf_detail;

// ── Inputs → model ───────────────────────────────────────────────────────────

vd_val::DcfInputs EquityValuationTab::dcf_inputs() const {
    vd_val::DcfInputs in;
    in.fcf = fcf_input_->value() * 1e6;
    in.growth = growth_input_->value() / 100.0;
    in.terminal_growth = terminal_input_->value() / 100.0;
    in.discount = discount_input_->value() / 100.0;
    in.years = static_cast<int>(years_input_->value());
    in.shares = shares_input_->value() * 1e6;
    in.fade = fade_check_ && fade_check_->isChecked();
    in.unlevered = method_ == Method::Fcff;
    in.net_debt = net_debt_;
    in.minority = minority_;
    return in;
}

double EquityValuationTab::value_for(Method m, double growth, double discount, double terminal) const {
    switch (m) {
        case Method::Fcfe:
        case Method::Fcff: {
            auto in = dcf_inputs();
            in.growth = growth;
            in.discount = discount;
            in.terminal_growth = terminal;
            const auto r = vd_val::dcf(in);
            return r.ok ? r.per_share : std::nan("");
        }
        case Method::Ddm: {
            vd_val::DdmInputs in;
            in.dps = dps_;
            in.growth = growth;
            in.terminal_growth = terminal;
            in.cost_of_equity = discount;
            in.years = static_cast<int>(years_input_->value());
            const auto r = vd_val::ddm(in);
            return r.ok ? r.per_share : std::nan("");
        }
        case Method::ResidualIncome:
        default: {
            vd_val::ResidualIncomeInputs in;
            in.book_value_ps = bvps_;
            // Growth stands in for the ROE assumption: each point of growth above the base
            // case lifts the starting ROE by two thirds of a point.
            in.roe = vd_ok(roe_) ? roe_ + (growth - growth_input_->value() / 100.0) * (2.0 / 3.0) : std::nan("");
            in.long_run_roe = discount + std::max(0.0, terminal - 0.02);
            in.payout = vd_ok(payout_) ? payout_ : 0.4;
            in.cost_of_equity = discount;
            in.years = static_cast<int>(years_input_->value());
            const auto r = vd_val::residual_income(in);
            return r.ok ? r.per_share : std::nan("");
        }
    }
}

void EquityValuationTab::on_calculate_clicked() {
    if (current_symbol_.isEmpty() || !fcf_input_)
        return;
    apply_discount();
    update_assumptions_note();
    render_results();
    update_sensitivity();
}

// ── Results ──────────────────────────────────────────────────────────────────

void EquityValuationTab::render_results() {
    const auto listing = equity_fmt::listing_of(last_info_);
    const double px_q = last_info_.current_price;
    const double px_rep = price_in_reporting();
    const double mcap_rep = market_cap_in_reporting();
    dcf_note_->setVisible(false);
    verdict_badge_->setVisible(false);
    vd_tone(margin_val_, "");

    // Current price: as quoted, and in the reporting currency when that differs.
    if (px_q > 0) {
        QString text = fmt_quote(px_q);
        if (!listing.same_currency() && vd_ok(px_rep))
            text += tr("  (≈ %1)").arg(fmt_rep(px_rep));
        else if (listing.minor_quote())
            text += tr("  (= %1)").arg(equity_fmt::price(listing.to_major(px_q), listing.quote_major()));
        current_price_val_->setText(text);
    } else {
        current_price_val_->setText(equity_fmt::dash());
    }

    QStringList warnings;
    double per_share = std::nan("");
    double equity_value = std::nan("");
    bridge_val_->setText(equity_fmt::dash());
    terminal_share_val_->setText(equity_fmt::dash());
    implied_growth_val_->setText(equity_fmt::dash());
    vd_val::DcfResult res;
    if (method_ == Method::Fcfe || method_ == Method::Fcff) {
        const auto in = dcf_inputs();
        res = vd_val::dcf(in);
        if (!res.ok) {
            intrinsic_val_->setText(equity_fmt::dash());
            intrinsic_quote_val_->setText(equity_fmt::dash());
            margin_val_->setText(equity_fmt::dash());
            dcf_note_->setText(res.error);
            dcf_note_->setVisible(true);
            render_projection(res);
            render_scenarios();
            return;
        }
        per_share = res.per_share;
        equity_value = res.equity_value;
        terminal_share_val_->setText(equity_fmt::pct(res.terminal_fraction, 0));
        if (method_ == Method::Fcff)
            bridge_val_->setText(tr("EV %1 − net debt %2 − minority %3 = equity %4")
                                     .arg(fmt_amount(res.enterprise_value), fmt_amount(net_debt_), fmt_amount(minority_),
                                          fmt_amount(res.equity_value)));
        else
            bridge_val_->setText(tr("Levered cash flow → equity value %1 directly").arg(fmt_amount(res.equity_value)));
        if (res.terminal_fraction > 0.75)
            warnings << tr("%1 of the value sits in the terminal year — very sensitive to terminal growth and the "
                           "discount rate (see the grid).")
                            .arg(equity_fmt::pct(res.terminal_fraction, 0));
        if (vd_ok(px_rep) && px_rep > 0) {
            const double g = vd_val::implied_growth(in, px_rep);
            implied_growth_val_->setText(vd_ok(g) ? equity_fmt::pct(g, 1, true) : equity_fmt::dash());
            vd_tone(implied_growth_val_, !vd_ok(g) ? "" : (g > in.growth + 0.02 ? "bad" : (g < in.growth - 0.02 ? "good" : "warn")));
        }
        render_projection(res);
    } else if (method_ == Method::Ddm) {
        per_share = value_for(Method::Ddm, growth_input_->value() / 100.0, discount_input_->value() / 100.0,
                              terminal_input_->value() / 100.0);
        if (!vd_ok(per_share)) {
            warnings << tr("A dividend discount model needs a dividend — none in the trailing cash-flow statement.");
        } else {
            bridge_val_->setText(tr("Dividend %1 / share growing %2, then %3")
                                     .arg(fmt_rep(dps_), equity_fmt::pct(growth_input_->value() / 100.0, 1),
                                          equity_fmt::pct(terminal_input_->value() / 100.0, 1)));
        }
        render_projection(res);
    } else {
        per_share = value_for(Method::ResidualIncome, growth_input_->value() / 100.0, discount_input_->value() / 100.0,
                              terminal_input_->value() / 100.0);
        if (!vd_ok(per_share))
            warnings << tr("Residual income needs book value per share and a return on equity from the statements.");
        else
            bridge_val_->setText(tr("Book value %1 / share + PV of ROE %2 above the cost of equity %3")
                                     .arg(fmt_rep(bvps_), equity_fmt::pct(roe_, 1),
                                          equity_fmt::pct(discount_input_->value() / 100.0, 1)));
        render_projection(res);
    }

    intrinsic_val_->setText(vd_ok(per_share) ? fmt_rep(per_share) : equity_fmt::dash());
    // Upside: company-level for the cash-flow methods (equity value vs market cap, both in
    // the reporting currency) — independent of how many ordinary shares an ADR represents;
    // per share otherwise.
    double upside = std::nan("");
    if (vd_ok(equity_value) && vd_ok(mcap_rep) && mcap_rep > 0)
        upside = equity_value / mcap_rep - 1.0;
    else if (vd_ok(per_share) && vd_ok(px_rep) && px_rep > 0)
        upside = per_share / px_rep - 1.0;
    intrinsic_quote_val_->setText(vd_ok(upside) && px_q > 0 ? tr("%1  (value at the quote's unit)").arg(fmt_quote(px_q * (1.0 + upside)))
                                                            : equity_fmt::dash());
    if (vd_ok(upside)) {
        margin_val_->setText(equity_fmt::pct(upside, 1, true));
        vd_tone(margin_val_, upside > 0.10 ? "good" : (upside < -0.10 ? "bad" : "warn"));
        if (upside > 0.20)
            set_badge(verdict_badge_, tr("BELOW MODEL VALUE"), "good");
        else if (upside < -0.20)
            set_badge(verdict_badge_, tr("ABOVE MODEL VALUE"), "bad");
        else
            set_badge(verdict_badge_, tr("NEAR MODEL VALUE"), "warn");
        // A result several times the price is a statement about the inputs, not the stock
        // (VOD.L: beta 0.32 → cost of equity 4.7 %, 2.2 pp above terminal growth, on a cash
        // flow that is 17 % of the market cap). Say which inputs drive it.
        if (upside > 1.0 || upside < -0.67) {
            QStringList why;
            const double r = discount_input_->value() / 100.0, tg = terminal_input_->value() / 100.0;
            if (method_ != Method::ResidualIncome && r - tg < 0.03)
                why << tr("the discount rate is only %1 pp above terminal growth")
                           .arg(QLocale().toString((r - tg) * 100.0, 'f', 1));
            if (beta_input_ && beta_input_->value() < 0.6)
                why << tr("beta %1 gives a low cost of equity").arg(QLocale().toString(beta_input_->value(), 'f', 2));
            if ((method_ == Method::Fcfe || method_ == Method::Fcff) && fcf_input_ && vd_ok(mcap_rep) && mcap_rep > 0) {
                const double yield = fcf_input_->value() * 1e6 / mcap_rep;
                if (yield > 0.10)
                    why << tr("the cash-flow base is %1 of the market cap").arg(equity_fmt::pct(yield, 0));
            }
            warnings.prepend(why.isEmpty()
                                 ? tr("An upside of %1 is outside what the model can support — check the inputs.")
                                       .arg(equity_fmt::pct(upside, 0, true))
                                 : tr("An upside of %1 is outside what the model can support: %2.")
                                       .arg(equity_fmt::pct(upside, 0, true), why.join(QStringLiteral("; "))));
            set_badge(verdict_badge_, tr("CHECK INPUTS"), "warn");
        }
    } else {
        margin_val_->setText(equity_fmt::dash());
        if (vd_ok(per_share) && !listing.same_currency())
            warnings << tr("No %1→%2 exchange rate yet — the price is compared once it loads.")
                            .arg(listing.quote_major(), listing.reporting_major());
    }
    if (!listing.same_currency() && last_info_.country != QLatin1String("United States") &&
        (last_info_.exchange == QLatin1String("NYQ") || last_info_.exchange == QLatin1String("NMS") ||
         last_info_.exchange == QLatin1String("NGM") || last_info_.exchange == QLatin1String("NCM")))
        warnings << tr("A US depositary receipt may represent several ordinary shares: the upside is measured on "
                       "equity value vs market cap, not per share.");
    if (!warnings.isEmpty()) {
        dcf_note_->setText(warnings.join(QLatin1Char(' ')));
        dcf_note_->setVisible(true);
    }
    render_scenarios();
}

void EquityValuationTab::render_projection(const vd_val::DcfResult& res) {
    proj_table_->clear();
    proj_table_->setRowCount(0);
    QStringList cats;
    ui::BarSeries cf, pv;
    if (method_ == Method::Fcfe || method_ == Method::Fcff) {
        proj_table_->setColumnCount(5);
        proj_table_->setHorizontalHeaderLabels({tr("YEAR"), tr("GROWTH"),
                                                method_ == Method::Fcff ? tr("FCFF") : tr("FCFE"), tr("DISCOUNT FACTOR"),
                                                tr("PRESENT VALUE")});
        if (!res.ok) {
            proj_chart_->clear();
            proj_chart_->set_state(ui::PaintedChart::Empty, tr("No projection"));
            return;
        }
        const int n = static_cast<int>(res.cash_flow.size());
        proj_table_->setRowCount(n + 2);
        for (int t = 0; t < n; ++t) {
            proj_table_->setItem(t, 0, vd_item(tr("Year %1").arg(t + 1), Qt::AlignLeft | Qt::AlignVCenter));
            proj_table_->setItem(t, 1, vd_item(equity_fmt::pct(res.growth[t], 1, true)));
            proj_table_->setItem(t, 2, vd_item(fmt_amount(res.cash_flow[t])));
            proj_table_->setItem(t, 3, vd_item(vd_uf::number(res.discount_factor[t], 3)));
            proj_table_->setItem(t, 4, vd_item(fmt_amount(res.present_value[t])));
            cats << tr("Y%1").arg(t + 1);
            cf.values << res.cash_flow[t];
            pv.values << res.present_value[t];
        }
        proj_table_->setItem(n, 0, vd_item(tr("Terminal value"), Qt::AlignLeft | Qt::AlignVCenter));
        proj_table_->setItem(n, 2, vd_item(fmt_amount(res.terminal_value)));
        proj_table_->setItem(n, 4, vd_item(fmt_amount(res.pv_terminal)));
        proj_table_->setItem(n + 1, 0,
                             vd_item(method_ == Method::Fcff ? tr("Enterprise value") : tr("Equity value"),
                                     Qt::AlignLeft | Qt::AlignVCenter));
        proj_table_->setItem(n + 1, 4, vd_item(fmt_amount(res.pv_fcf + res.pv_terminal)));
        QFont bold = proj_table_->font();
        bold.setBold(true);
        for (int c = 0; c < 5; ++c)
            if (auto* it = proj_table_->item(n + 1, c))
                it->setFont(bold);
    } else {
        // DDM: dividends per share; residual income: book value and excess return per share.
        const double ke = discount_input_->value() / 100.0, g = growth_input_->value() / 100.0;
        const int n = static_cast<int>(years_input_->value());
        proj_table_->setColumnCount(4);
        if (method_ == Method::Ddm) {
            proj_table_->setHorizontalHeaderLabels({tr("YEAR"), tr("DIVIDEND / SHARE"), tr("DISCOUNT FACTOR"),
                                                    tr("PRESENT VALUE")});
            if (vd_ok(dps_) && dps_ > 0) {
                double d = dps_;
                proj_table_->setRowCount(n);
                for (int t = 1; t <= n; ++t) {
                    d *= 1.0 + g;
                    const double df = 1.0 / std::pow(1.0 + ke, t);
                    proj_table_->setItem(t - 1, 0, vd_item(tr("Year %1").arg(t), Qt::AlignLeft | Qt::AlignVCenter));
                    proj_table_->setItem(t - 1, 1, vd_item(fmt_rep(d)));
                    proj_table_->setItem(t - 1, 2, vd_item(vd_uf::number(df, 3)));
                    proj_table_->setItem(t - 1, 3, vd_item(fmt_rep(d * df)));
                    cats << tr("Y%1").arg(t);
                    cf.values << d;
                    pv.values << d * df;
                }
            }
        } else {
            proj_table_->setHorizontalHeaderLabels({tr("YEAR"), tr("BOOK VALUE / SHARE"), tr("RESIDUAL INCOME / SHARE"),
                                                    tr("PRESENT VALUE")});
            if (vd_ok(bvps_) && bvps_ > 0 && vd_ok(roe_)) {
                double bv = bvps_;
                const double payout = vd_ok(payout_) ? payout_ : 0.4;
                const double lr = ke + std::max(0.0, terminal_input_->value() / 100.0 - 0.02);
                proj_table_->setRowCount(n);
                for (int t = 1; t <= n; ++t) {
                    const double roe = n > 1 ? roe_ + (lr - roe_) * (t - 1.0) / (n - 1.0) : roe_;
                    const double ri = (roe - ke) * bv;
                    const double df = 1.0 / std::pow(1.0 + ke, t);
                    proj_table_->setItem(t - 1, 0, vd_item(tr("Year %1").arg(t), Qt::AlignLeft | Qt::AlignVCenter));
                    proj_table_->setItem(t - 1, 1, vd_item(fmt_rep(bv)));
                    proj_table_->setItem(t - 1, 2, vd_item(fmt_rep(ri)));
                    proj_table_->setItem(t - 1, 3, vd_item(fmt_rep(ri * df)));
                    cats << tr("Y%1").arg(t);
                    cf.values << ri;
                    pv.values << ri * df;
                    bv += roe * bv * (1.0 - payout);
                }
            }
        }
    }
    const QString ccy = reporting_currency();
    const bool per_share_units = method_ == Method::Ddm || method_ == Method::ResidualIncome;
    cf.name = method_ == Method::Ddm ? tr("Dividend") : method_ == Method::ResidualIncome ? tr("Residual income")
                                                                                         : tr("Cash flow");
    pv.name = tr("Present value");
    proj_chart_->set_categories(cats);
    proj_chart_->set_series({cf, pv});
    proj_chart_->set_state(cats.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready);
    proj_chart_->set_value_formatter([ccy, per_share_units](double v) {
        return per_share_units ? equity_fmt::price(v, ccy) : equity_fmt::money(v, ccy);
    });
}

void EquityValuationTab::render_scenarios() {
    const double g = growth_input_->value() / 100.0, r = discount_input_->value() / 100.0,
                 tg = terminal_input_->value() / 100.0;
    const double px_rep = price_in_reporting();
    struct Sc {
        const char* name;
        double dg, dr, dt;
        const char* color;
    };
    const Sc sc[3] = {{QT_TR_NOOP("BEAR"), -0.03, 0.01, -0.005, "danger"},
                      {QT_TR_NOOP("BASE"), 0.0, 0.0, 0.0, "warn"},
                      {QT_TR_NOOP("BULL"), 0.03, -0.01, 0.005, "success"}};
    for (int i = 0; i < 3; ++i) {
        const double v = value_for(method_, g + sc[i].dg, r + sc[i].dr, tg + sc[i].dt);
        const QString color = QLatin1String(sc[i].color) == QLatin1String("danger")    ? ui::colors::DANGER()
                               : QLatin1String(sc[i].color) == QLatin1String("success") ? ui::colors::SUCCESS()
                                                                                        : ui::colors::WARNING();
        QString text = QStringLiteral("<span style='color:%1'>%2</span> %3")
                           .arg(color, tr(sc[i].name), vd_ok(v) ? fmt_rep(v) : equity_fmt::dash());
        if (vd_ok(v) && vd_ok(px_rep) && px_rep > 0)
            text += QStringLiteral(" (%1)").arg(equity_fmt::pct(v / px_rep - 1.0, 0, true));
        scenario_lbl_[i]->setText(text);
        scenario_lbl_[i]->setToolTip(tr("Growth %1 pp, discount %2 pp, terminal growth %3 pp vs the base case")
                                         .arg(vd_uf::signed_number(sc[i].dg * 100, 1), vd_uf::signed_number(sc[i].dr * 100, 1),
                                              vd_uf::signed_number(sc[i].dt * 100, 1)));
    }
}

void EquityValuationTab::update_sensitivity() {
    if (!sens_table_)
        return;
    const int n = sens_size_ && sens_size_->currentIndex() == 1 ? 7 : 5;
    const int half = n / 2;
    const bool cols_stage1 = sens_cols_ && sens_cols_->currentIndex() == 1;
    const double row_step = sens_step_rows_->value() / 100.0, col_step = sens_step_cols_->value() / 100.0;
    const double g = growth_input_->value() / 100.0, r = discount_input_->value() / 100.0,
                 tg = terminal_input_->value() / 100.0;
    const double px_rep = price_in_reporting();
    sens_table_->setRowCount(n);
    sens_table_->setColumnCount(n);
    QStringList rows, cols;
    for (int i = 0; i < n; ++i) {
        rows << equity_fmt::pct(r + (i - half) * row_step, 1);
        cols << equity_fmt::pct((cols_stage1 ? g : tg) + (i - half) * col_step, 1);
    }
    sens_table_->setVerticalHeaderLabels(rows);
    sens_table_->setHorizontalHeaderLabels(cols);
    sens_title_->setText(tr("SENSITIVITY — VALUE PER SHARE in %1 (rows: discount rate; %2)")
                             .arg(reporting_currency().isEmpty() ? tr("the reporting currency") : reporting_currency(),
                                  cols_stage1 ? tr("columns: stage-1 growth") : tr("columns: terminal growth")));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            const double rr = r + (i - half) * row_step;
            const double gg = cols_stage1 ? g + (j - half) * col_step : g;
            const double tt = cols_stage1 ? tg : tg + (j - half) * col_step;
            const double v = value_for(method_, gg, rr, tt);
            auto* item = new QTableWidgetItem(vd_ok(v) ? fmt_rep(v) : equity_fmt::dash());
            item->setTextAlignment(Qt::AlignCenter);
            // Model value vs price is a state, not a price move: status colours.
            QColor fg(ui::colors::TEXT_PRIMARY());
            if (vd_ok(v) && vd_ok(px_rep) && px_rep > 0)
                fg = QColor(v >= px_rep * 1.10   ? ui::colors::SUCCESS()
                            : v <= px_rep * 0.90 ? ui::colors::DANGER()
                                                 : ui::colors::WARNING());
            else if (!vd_ok(v))
                fg = QColor(ui::colors::TEXT_TERTIARY());
            item->setForeground(fg);
            if (i == half && j == half) {
                QFont f = item->font();
                f.setBold(true);
                item->setFont(f);
            }
            sens_table_->setItem(i, j, item);
        }
    }
    sens_table_->setToolTip(tr("Green: at least 10% above today's price · red: at least 10% below · the centre cell "
                               "is the current inputs."));
    // Exactly the grid's height: the fixed 8-row minimum left an empty band under a 5 × 5 grid.
    const int header_h = std::max(sens_table_->horizontalHeader()->sizeHint().height(),
                                  sens_table_->horizontalHeader()->height());
    sens_table_->setFixedHeight(header_h + n * sens_table_->verticalHeader()->defaultSectionSize() +
                                2 * sens_table_->frameWidth() + 2);
}

// ── Multiples vs own history and peers ───────────────────────────────────────

void EquityValuationTab::request_peer_multiples() {
    if (current_symbol_.isEmpty() || !peers_btn_)
        return;
    peers_auto_on_show_ = false;
    peers_pending_ = true;
    peers_btn_->setEnabled(false);
    hist_note_->setText(tr("Finding same-industry peers on %1's home market…").arg(current_symbol_));
    // Both legs are cached by the service (suggestions 24 h, peer figures 15 min), so a
    // revisit or the Peers tab's own pull costs nothing.
    services::equity::EquityResearchService::instance().fetch_suggested_peers(current_symbol_, 6);
}

void EquityValuationTab::render_history() {
    if (!hist_chart_ || current_symbol_.isEmpty())
        return;
    reset_hist_chart();
    hist_table_->setRowCount(0);
    if (!financials_loaded_ || daily_.isEmpty()) {
        hist_chart_->set_state(ui::TimeSeriesChart::State::Loading, tr("Waiting for statements and price history…"));
        return;
    }
    const auto listing = equity_fmt::listing_of(last_info_);
    // Price in the reporting currency: minor units → major, then today's FX rate when the
    // currencies differ (history converted at today's rate — said in the note).
    double fx = 1.0;
    if (!listing.same_currency()) {
        const auto r = listing.fx_reporting_to_quote();
        if (!r || !(*r > 0)) {
            hist_chart_->set_state(ui::TimeSeriesChart::State::Empty,
                                   tr("No %1→%2 rate yet").arg(listing.quote_major(), listing.reporting_major()));
            return;
        }
        fx = 1.0 / *r;
    }
    const int fy = vd_fm::fiscal_year_end_month(last_financials_);
    const auto inc = vd_fm::columns_for(last_financials_, vd_fm::Statement::Income, vd_fm::Period::Ttm, fy);
    const auto bal = vd_fm::columns_for(last_financials_, vd_fm::Statement::Balance, vd_fm::Period::Ttm, fy);
    std::map<qint64, double> eps_at, bvps_at;
    for (const auto& c : inc) {
        const double e = vd_fm::line(c.data, {"Diluted EPS", "Basic EPS"});
        if (vd_ok(e))
            eps_at[QDateTime(c.end, QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch()] = e;
    }
    for (const auto& c : bal) {
        const double eq = vd_fm::line(c.data, {"Stockholders Equity", "Common Stock Equity"});
        const double sh = vd_fm::line(c.data, {"Ordinary Shares Number", "Share Issued"});
        if (vd_ok(eq) && sh > 0)
            bvps_at[QDateTime(c.end, QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch()] = eq / sh;
    }
    if (eps_at.empty() && bvps_at.empty()) {
        hist_chart_->set_state(ui::TimeSeriesChart::State::Empty, tr("No per-share history in the statements"));
        return;
    }
    const auto daily = services::equity::tmath::normalize_daily(daily_);
    auto as_of = [](const std::map<qint64, double>& m, qint64 t) {
        auto it = m.upper_bound(t);
        if (it == m.begin())
            return std::nan("");
        return std::prev(it)->second;
    };
    QVector<QPointF> pe, pb;
    QVector<double> pe_vals, pb_vals;
    const qint64 first = std::min(eps_at.empty() ? LLONG_MAX : eps_at.begin()->first,
                                  bvps_at.empty() ? LLONG_MAX : bvps_at.begin()->first);
    for (int i = 0; i < daily.size(); i += 5) { // weekly points are plenty for a multiple
        const auto& c = daily[i];
        if (c.timestamp < first)
            continue;
        const double px = listing.to_major(c.close) * fx;
        const double e = as_of(eps_at, c.timestamp), b = as_of(bvps_at, c.timestamp);
        const double x = static_cast<double>(c.timestamp) * 1000.0;
        if (e > 0) {
            pe << QPointF(x, px / e);
            pe_vals << px / e;
        }
        if (b > 0) {
            pb << QPointF(x, px / b);
            pb_vals << px / b;
        }
    }
    hist_chart_->set_state(ui::TimeSeriesChart::State::Ready);
    const VdStats spe = vd_stats(pe_vals), spb = vd_stats(pb_vals);
    if (!pe.isEmpty()) {
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = tr("P/E (trailing)");
        s.points = pe;
        s.rebase = false;
        s.color = ui::ChartColorRole::Accent;
        s.formatter = [](double v) { return vd_uf::ratio(v, 1); };
        hist_chart_->add_series(s);
        hist_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return vd_uf::ratio(v, 1); });
        hist_chart_->add_hline(spe.mean, tr("avg %1").arg(vd_uf::ratio(spe.mean, 1)), ui::ChartColorRole::Muted);
        hist_chart_->add_hline(spe.mean + spe.sd, tr("+1σ"), ui::ChartColorRole::Muted);
        hist_chart_->add_hline(std::max(0.0, spe.mean - spe.sd), tr("−1σ"), ui::ChartColorRole::Muted);
    }
    if (!pb.isEmpty()) {
        const int pane = hist_chart_->add_pane(0.6);
        hist_chart_->set_pane_title(pane, tr("P/B"));
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = tr("P/B");
        s.points = pb;
        s.pane = pane;
        s.rebase = false;
        s.color = ui::ChartColorRole::Info;
        s.formatter = [](double v) { return vd_uf::ratio(v, 2); };
        hist_chart_->add_series(s);
        hist_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return vd_uf::ratio(v, 2); }, pane);
        hist_chart_->add_hline(spb.mean, tr("avg %1").arg(vd_uf::ratio(spb.mean, 2)), ui::ChartColorRole::Muted, pane);
    }
    hist_chart_->set_active_preset(QStringLiteral("10Y"));

    // Table: current vs own history vs peer median.
    QVector<double> peer_pe, peer_pb, peer_ev;
    for (const auto& p : peers_) {
        if (p.symbol.compare(current_symbol_, Qt::CaseInsensitive) == 0)
            continue;
        peer_pe << p.pe_ratio;
        peer_pb << p.price_to_book;
        peer_ev << p.ev_to_ebitda;
    }
    hist_table_->setColumnCount(6);
    hist_table_->setHorizontalHeaderLabels({tr("MULTIPLE"), tr("CURRENT"), tr("HIST. AVG"), tr("HIST. LOW"),
                                            tr("HIST. HIGH"), tr("PEER MEDIAN")});
    struct RowDef {
        QString name;
        VdStats st;
        double current;
        double peer;
        int dec;
    };
    const QVector<RowDef> defs = {
        {tr("P/E (trailing)"), spe, last_info_.pe_ratio > 0 ? last_info_.pe_ratio : spe.last, vd_median(peer_pe), 1},
        {tr("P/B"), spb, last_info_.price_to_book > 0 ? last_info_.price_to_book : spb.last, vd_median(peer_pb), 2},
        {tr("EV/EBITDA"), VdStats{}, last_info_.ev_to_ebitda, vd_median(peer_ev), 1},
    };
    hist_table_->setRowCount(defs.size());
    for (int i = 0; i < defs.size(); ++i) {
        const auto& d = defs[i];
        hist_table_->setItem(i, 0, vd_item(d.name, Qt::AlignLeft | Qt::AlignVCenter));
        hist_table_->setItem(i, 1, vd_item(equity_fmt::multiple(d.current, d.dec)));
        hist_table_->setItem(i, 2, vd_item(equity_fmt::multiple(d.st.mean, d.dec)));
        hist_table_->setItem(i, 3, vd_item(equity_fmt::multiple(d.st.min, d.dec)));
        hist_table_->setItem(i, 4, vd_item(equity_fmt::multiple(d.st.max, d.dec)));
        hist_table_->setItem(i, 5, vd_item(equity_fmt::multiple(d.peer, d.dec)));
        // Premium / discount vs history in the tooltip.
        if (vd_ok(d.current) && vd_ok(d.st.mean) && d.st.mean > 0)
            hist_table_->item(i, 1)->setToolTip(tr("%1 vs its own average").arg(equity_fmt::pct(d.current / d.st.mean - 1.0, 0, true)));
        if (vd_ok(d.current) && vd_ok(d.peer) && d.peer > 0)
            hist_table_->item(i, 5)->setToolTip(tr("%1 premium / discount vs peers").arg(equity_fmt::pct(d.current / d.peer - 1.0, 0, true)));
    }
    QStringList note;
    note << tr("Trailing P/E and P/B from the reported EPS and book value at each period end against the daily close");
    if (!listing.same_currency())
        note << tr("prices converted %1→%2 at today's rate").arg(listing.quote_major(), listing.reporting_major());
    else if (listing.minor_quote())
        note << tr("prices taken in %1, not %2").arg(listing.quote_major(), listing.quote);
    if (!peers_.isEmpty()) {
        QStringList names;
        for (const auto& p : peers_)
            if (p.symbol.compare(current_symbol_, Qt::CaseInsensitive) != 0)
                names << p.symbol;
        note << tr("peers: %1").arg(names.join(QStringLiteral(", ")));
    } else if (peers_pending_) {
        note << tr("peer medians loading…");
    }
    hist_note_->setText(note.join(QStringLiteral(" · ")));
}

} // namespace fincept::screens
