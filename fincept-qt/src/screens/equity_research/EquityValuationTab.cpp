// src/screens/equity_research/EquityValuationTab.cpp
#include "screens/equity_research/EquityValuationTab.h"

#include "screens/equity_research/EquityResearchFormat.h"
#include "services/equity/EquityResearchService.h"
#include "services/equity/EquityValuationModels.h"
#include "ui/theme/Theme.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace valuation_tab_detail {

namespace fmt = equity_fmt;

// CAPM pre-fill: risk-free + beta × equity risk premium, floored — a near-zero beta would
// otherwise discount at ~4.5%, barely above terminal growth, and explode the terminal value.
constexpr double kRiskFree = 0.045;
constexpr double kEquityPremium = 0.055;
constexpr double kDiscountFloor = 0.06;
// Growth pre-fill comes from ONE quarter's YoY revenue growth; held for 10 years it must
// be capped or a cyclical rebound quarter becomes a decade of hyper-growth.
constexpr double kGrowthCapLow = -0.05;
constexpr double kGrowthCapHigh = 0.15;

void repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace valuation_tab_detail

using namespace valuation_tab_detail;

EquityValuationTab::EquityValuationTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    // A failed statements fetch must not leave the scores on "loading" forever.
    connect(&services::equity::EquityResearchService::instance(),
            &services::equity::EquityResearchService::error_occurred, this,
            [this](const QString& ctx, const QString&) {
                if (ctx != QLatin1String("Financials") || financials_loaded_ || current_symbol_.isEmpty())
                    return;
                financials_failed_ = true;
                run_scoring_models();
            });
}

// ─────────────────────────────────────────────────────────────────────────────
// UI
// ─────────────────────────────────────────────────────────────────────────────

void EquityValuationTab::build_ui() {
    setObjectName(QStringLiteral("equityValuationTab"));
    // One stylesheet for the whole tab; widgets pick rules up by objectName / "tone".
    setStyleSheet(
        QStringLiteral(
            "#equityValuationTab, QWidget#valContent { background:%1; }"
            "QScrollArea#valScroll { background:transparent; border:0; }"
            "QFrame#valBox { background:%2; border:1px solid %3; }"
            "QLabel { background:transparent; border:0; }"
            "QLabel#valBoxTitle { color:%4; font-size:11px; font-weight:700; letter-spacing:1px; }"
            "QLabel#valKey { color:%5; font-size:11px; }"
            "QLabel#valValue { color:%6; font-size:13px; font-weight:600; }"
            "QLabel#valValue[tone=\"good\"] { color:%7; } QLabel#valValue[tone=\"bad\"] { color:%8; }"
            "QLabel#valValue[tone=\"warn\"] { color:%4; }"
            "QLabel#valNote { color:%9; font-size:10px; }"
            "QLabel#valWarn { color:%10; font-size:11px; }"
            "QDoubleSpinBox { background:%11; color:%6; border:1px solid %3; padding:2px 4px; font-size:12px; }"
            "QPushButton#valCalc { background:%4; color:%1; border:0; padding:5px 14px; font-size:11px;"
            " font-weight:700; letter-spacing:1px; }"
            "QPushButton#valCalc:hover { background:%12; }"
            "QLabel#valBadge { padding:2px 10px; font-size:10px; font-weight:700; color:%1; background:%13; }"
            "QLabel#valBadge[tone=\"good\"] { background:%7; } QLabel#valBadge[tone=\"bad\"] { background:%8; }"
            "QLabel#valBadge[tone=\"warn\"] { background:%4; }"
            "QLabel#valBadge[tone=\"none\"] { background:transparent; color:%9; border:1px solid %3; }")
            .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::AMBER(),
                 ui::colors::TEXT_SECONDARY(), ui::colors::TEXT_PRIMARY(), ui::colors::POSITIVE(),
                 ui::colors::NEGATIVE(), ui::colors::TEXT_TERTIARY())
            .arg(ui::colors::WARNING(), ui::colors::BG_RAISED(), ui::colors::AMBER_DIM(), ui::colors::BORDER_MED()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("valScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget;
    content->setObjectName(QStringLiteral("valContent"));
    auto* vbox = new QVBoxLayout(content);
    vbox->setContentsMargins(12, 12, 12, 12);
    vbox->setSpacing(12);
    vbox->addWidget(build_dcf_section());
    vbox->addWidget(build_scoring_section());
    vbox->addWidget(build_multiples_section());
    vbox->addStretch();

    scroll->setWidget(content);
    root->addWidget(scroll);
}

QFrame* EquityValuationTab::make_box(const char* title_key, QVBoxLayout*& body) {
    auto* box = new QFrame;
    box->setObjectName(QStringLiteral("valBox"));
    body = new QVBoxLayout(box);
    body->setContentsMargins(12, 10, 12, 12);
    body->setSpacing(8);
    auto* title = new QLabel(tr(title_key));
    title->setObjectName(QStringLiteral("valBoxTitle"));
    i18n_labels_.insert(title, title_key);
    body->addWidget(title);
    return box;
}

QLabel* EquityValuationTab::add_key_label(QWidget* parent, const char* key) {
    auto* lbl = new QLabel(tr(key), parent);
    lbl->setObjectName(QStringLiteral("valKey"));
    i18n_labels_.insert(lbl, key);
    return lbl;
}

QWidget* EquityValuationTab::build_dcf_section() {
    QVBoxLayout* body = nullptr;
    auto* box = make_box(QT_TR_NOOP("DCF — LEVERED FREE CASH FLOW"), body);

    auto* row = new QHBoxLayout;
    row->setSpacing(28);

    // Inputs
    auto* inputs = new QWidget(box);
    auto* grid = new QGridLayout(inputs);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(6);
    int r = 0;
    auto make_spin = [&](const char* key, double min, double max, double def, const QString& suffix,
                         int dec) -> QDoubleSpinBox* {
        grid->addWidget(add_key_label(inputs, key), r, 0);
        auto* spin = new QDoubleSpinBox(inputs);
        spin->setRange(min, max);
        spin->setDecimals(dec);
        spin->setValue(def);
        spin->setSuffix(suffix);
        spin->setFixedWidth(150);
        spin->setAlignment(Qt::AlignRight);
        spin->setGroupSeparatorShown(true);
        grid->addWidget(spin, r, 1);
        ++r;
        return spin;
    };
    fcf_input_ = make_spin(QT_TR_NOOP("BASE FREE CASH FLOW"), -9999999.0, 9999999.0, 0.0, tr(" M"), 1);
    growth_input_ = make_spin(QT_TR_NOOP("GROWTH (PROJECTION)"), -50.0, 100.0, 8.0, QStringLiteral(" %"), 2);
    terminal_input_ = make_spin(QT_TR_NOOP("TERMINAL GROWTH"), -2.0, 10.0, 2.5, QStringLiteral(" %"), 2);
    discount_input_ = make_spin(QT_TR_NOOP("DISCOUNT RATE"), 1.0, 40.0, 10.0, QStringLiteral(" %"), 2);
    years_input_ = make_spin(QT_TR_NOOP("PROJECTION YEARS"), 1.0, 30.0, 10.0, tr(" yrs"), 0);
    shares_input_ = make_spin(QT_TR_NOOP("SHARES OUTSTANDING"), 0.01, 99999999.0, 1.0, tr(" M"), 2);

    calc_btn_ = new QPushButton(tr("CALCULATE"), inputs);
    calc_btn_->setObjectName(QStringLiteral("valCalc"));
    calc_btn_->setCursor(Qt::PointingHandCursor);
    calc_btn_->setToolTip(tr("Re-run the DCF with the inputs above (Enter)"));
    calc_btn_->setDefault(true);
    connect(calc_btn_, &QPushButton::clicked, this, &EquityValuationTab::on_calculate_clicked);
    for (auto* spin : {fcf_input_, growth_input_, terminal_input_, discount_input_, years_input_, shares_input_})
        connect(spin, &QDoubleSpinBox::editingFinished, this, [this]() {
            if (!current_symbol_.isEmpty())
                on_calculate_clicked();
        });
    grid->addWidget(calc_btn_, r, 1, Qt::AlignRight);
    row->addWidget(inputs);

    // Results
    auto* results = new QWidget(box);
    auto* rg = new QGridLayout(results);
    rg->setContentsMargins(0, 0, 0, 0);
    rg->setHorizontalSpacing(12);
    rg->setVerticalSpacing(6);
    int rr = 0;
    auto make_result = [&](const char* key) -> QLabel* {
        rg->addWidget(add_key_label(results, key), rr, 0);
        auto* v = new QLabel(fmt::dash(), results);
        v->setObjectName(QStringLiteral("valValue"));
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        rg->addWidget(v, rr, 1);
        ++rr;
        return v;
    };
    intrinsic_val_ = make_result(QT_TR_NOOP("INTRINSIC VALUE / SHARE"));
    current_price_val_ = make_result(QT_TR_NOOP("CURRENT PRICE"));
    margin_val_ = make_result(QT_TR_NOOP("UPSIDE TO INTRINSIC"));
    terminal_share_val_ = make_result(QT_TR_NOOP("TERMINAL VALUE SHARE"));
    verdict_badge_ = new QLabel(results);
    verdict_badge_->setObjectName(QStringLiteral("valBadge"));
    verdict_badge_->setAlignment(Qt::AlignCenter);
    verdict_badge_->setVisible(false);
    rg->addWidget(verdict_badge_, rr, 0, 1, 2, Qt::AlignLeft);
    row->addWidget(results);
    row->addStretch();
    body->addLayout(row);

    dcf_note_ = new QLabel(box);
    dcf_note_->setObjectName(QStringLiteral("valWarn"));
    dcf_note_->setWordWrap(true);
    dcf_note_->setVisible(false);
    body->addWidget(dcf_note_);

    assumptions_lbl_ = new QLabel(tr("Open a symbol to pre-fill the model from its trailing free cash flow, beta and "
                                     "share count."),
                                  box);
    assumptions_lbl_->setObjectName(QStringLiteral("valNote"));
    assumptions_lbl_->setWordWrap(true);
    body->addWidget(assumptions_lbl_);
    return box;
}

QWidget* EquityValuationTab::build_scoring_section() {
    QVBoxLayout* body = nullptr;
    auto* box = make_box(QT_TR_NOOP("CREDIT & QUALITY SCORES — ANNUAL STATEMENTS"), body);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(6);
    int r = 0;
    auto make_score = [&](const char* key, QLabel*& val_out, QLabel*& badge_out) {
        grid->addWidget(add_key_label(box, key), r, 0);
        val_out = new QLabel(fmt::dash(), box);
        val_out->setObjectName(QStringLiteral("valValue"));
        val_out->setMinimumWidth(70);
        grid->addWidget(val_out, r, 1);
        badge_out = new QLabel(box);
        badge_out->setObjectName(QStringLiteral("valBadge"));
        badge_out->setAlignment(Qt::AlignCenter);
        badge_out->setMinimumWidth(170);
        badge_out->setVisible(false);
        grid->addWidget(badge_out, r, 2, Qt::AlignLeft);
        ++r;
    };
    make_score(QT_TR_NOOP("ALTMAN Z-SCORE"), altman_val_, altman_badge_);
    make_score(QT_TR_NOOP("PIOTROSKI F-SCORE"), piotroski_val_, piotroski_badge_);
    make_score(QT_TR_NOOP("BENEISH M-SCORE"), beneish_val_, beneish_badge_);
    grid->setColumnStretch(3, 1);
    body->addLayout(grid);

    scoring_note_ = new QLabel(box);
    scoring_note_->setObjectName(QStringLiteral("valNote"));
    scoring_note_->setWordWrap(true);
    body->addWidget(scoring_note_);
    return box;
}

QWidget* EquityValuationTab::build_multiples_section() {
    QVBoxLayout* body = nullptr;
    auto* box = make_box(QT_TR_NOOP("MULTIPLES — TRAILING 12 MONTHS"), body);
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(6);
    auto make_mult = [&](const char* key, QLabel*& out, int row, int col) {
        grid->addWidget(add_key_label(box, key), row, col * 2);
        out = new QLabel(fmt::dash(), box);
        out->setObjectName(QStringLiteral("valValue"));
        grid->addWidget(out, row, col * 2 + 1);
    };
    make_mult(QT_TR_NOOP("EV / EBITDA"), ev_ebitda_val_, 0, 0);
    make_mult(QT_TR_NOOP("EV / REVENUE"), ev_revenue_val_, 1, 0);
    make_mult(QT_TR_NOOP("PRICE / SALES"), price_sales_val_, 2, 0);
    make_mult(QT_TR_NOOP("FCF YIELD"), fcf_yield_val_, 0, 1);
    make_mult(QT_TR_NOOP("GRAHAM NUMBER"), graham_val_, 1, 1);
    grid->setColumnMinimumWidth(1, 90);
    grid->setColumnStretch(4, 1);
    body->addLayout(grid);
    graham_val_->setToolTip(tr("√(22.5 × trailing EPS × book value per share) — Graham's ceiling price for a "
                               "defensive investor (P/E ≤ 15 and P/B ≤ 1.5)."));
    multiples_note_ = new QLabel(box);
    multiples_note_->setObjectName(QStringLiteral("valNote"));
    multiples_note_->setWordWrap(true);
    body->addWidget(multiples_note_);
    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public setters
// ─────────────────────────────────────────────────────────────────────────────

void EquityValuationTab::set_symbol(const QString& symbol) {
    if (symbol.isEmpty() || symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    prefilled_for_.clear();
    clear_results();
    // set_stock_info()/set_financials() are fed whenever data lands — even while another tab
    // is showing — so what is held may already be this symbol's (keep it) or the previous
    // one's (drop it: it would drive this symbol's DCF, multiples and scores).
    if (last_info_.symbol != symbol)
        last_info_ = {};
    if (last_financials_.symbol != symbol)
        last_financials_ = {};
    financials_loaded_ = !last_financials_.symbol.isEmpty();
    financials_failed_ = false;

    if (!last_info_.symbol.isEmpty()) {
        pre_fill_dcf_inputs();
        run_multiples();
    }
    if (financials_loaded_) {
        run_scoring_models();
    } else {
        update_statements_note();
        // The statements feed the scores; ask for them (a cached copy answers at once and
        // reaches set_financials() through the screen).
        services::equity::EquityResearchService::instance().fetch_financials(symbol);
    }
}

void EquityValuationTab::set_stock_info(const services::equity::StockInfo& info) {
    last_info_ = info;
    if (info.symbol != current_symbol_)
        return; // held until this tab is shown for that symbol
    pre_fill_dcf_inputs();
    run_multiples();
    if (financials_loaded_)
        run_scoring_models(); // Altman needs the market cap
}

void EquityValuationTab::set_financials(const services::equity::FinancialsData& data) {
    last_financials_ = data;
    if (data.symbol != current_symbol_)
        return;
    financials_loaded_ = true;
    financials_failed_ = false;
    run_scoring_models();
}

// ─────────────────────────────────────────────────────────────────────────────
// DCF
// ─────────────────────────────────────────────────────────────────────────────

void EquityValuationTab::pre_fill_dcf_inputs() {
    // Once per symbol: later info refreshes must not overwrite inputs the analyst edited.
    if (!fcf_input_ || prefilled_for_ == current_symbol_)
        return;
    prefilled_for_ = current_symbol_;
    const auto& s = last_info_;
    QStringList notes;
    if (s.free_cashflow != 0.0) {
        fcf_input_->setValue(s.free_cashflow / 1e6);
        notes << tr("FCF = trailing-12-month levered free cash flow (Yahoo)");
    }
    const double g = s.revenue_growth != 0.0 ? s.revenue_growth : s.earnings_growth;
    if (g != 0.0) {
        const double capped = std::clamp(g, kGrowthCapLow, kGrowthCapHigh);
        growth_input_->setValue(capped * 100.0);
        notes << tr("growth = latest-quarter YoY %1 growth (%2), capped to %3…%4 for a multi-year horizon")
                     .arg(s.revenue_growth != 0.0 ? tr("revenue") : tr("earnings"), fmt::pct(g, 1),
                          fmt::pct(kGrowthCapLow, 0), fmt::pct(kGrowthCapHigh, 0));
    }
    if (s.beta > 0.0) {
        const double r = std::max(kDiscountFloor, kRiskFree + s.beta * kEquityPremium);
        discount_input_->setValue(r * 100.0);
        notes << tr("discount = %1 risk-free + β %2 × %3 equity premium (floor %4)")
                     .arg(fmt::pct(kRiskFree, 1), QString::number(s.beta, 'f', 2), fmt::pct(kEquityPremium, 1),
                          fmt::pct(kDiscountFloor, 0));
    } else {
        notes << tr("discount left at %1 — no beta reported").arg(fmt::pct(discount_input_->value() / 100.0, 1));
    }
    if (s.shares_outstanding > 0.0)
        shares_input_->setValue(s.shares_outstanding / 1e6);
    assumptions_lbl_->setText(tr("Pre-filled assumptions (edit and press CALCULATE): %1.").arg(notes.join(QStringLiteral(" · "))));
    on_calculate_clicked();
}

void EquityValuationTab::on_calculate_clicked() {
    if (current_symbol_.isEmpty())
        return;
    services::equity::valuation::DcfInputs in;
    in.fcf = fcf_input_->value() * 1e6;
    in.growth = growth_input_->value() / 100.0;
    in.terminal_growth = terminal_input_->value() / 100.0;
    in.discount = discount_input_->value() / 100.0;
    in.years = static_cast<int>(years_input_->value());
    in.shares = shares_input_->value() * 1e6;
    const auto res = services::equity::valuation::dcf(in);

    const QString stmt_ccy = statements_currency();
    const QString px_ccy = last_info_.currency;
    dcf_note_->setVisible(false);
    verdict_badge_->setVisible(false);
    margin_val_->setProperty("tone", QString());
    repolish(margin_val_);
    current_price_val_->setText(last_info_.current_price > 0.0 ? fmt_money(last_info_.current_price, px_ccy)
                                                               : fmt::dash());
    if (!res.ok) {
        intrinsic_val_->setText(fmt::dash());
        margin_val_->setText(fmt::dash());
        terminal_share_val_->setText(fmt::dash());
        dcf_note_->setText(res.error);
        dcf_note_->setVisible(true);
        return;
    }
    intrinsic_val_->setText(fmt_money(res.per_share, stmt_ccy));
    terminal_share_val_->setText(fmt::pct(res.terminal_fraction, 0));

    QStringList warnings;
    if (res.terminal_fraction > 0.75)
        warnings << tr("%1 of the value sits in the terminal year — the result is very sensitive to terminal "
                       "growth and the discount rate.")
                        .arg(fmt::pct(res.terminal_fraction, 0));
    if (currency_mismatch()) {
        margin_val_->setText(tr("n/c"));
        warnings << tr("Statements are reported in %1 but the stock trades in %2 (and an ADR may represent several "
                       "ordinary shares), so the per-share value is not compared with the price.")
                        .arg(stmt_ccy, px_ccy);
    } else if (last_info_.current_price > 0.0) {
        const double upside = (res.per_share - last_info_.current_price) / last_info_.current_price;
        margin_val_->setText(fmt::pct(upside, 1, true));
        const char* tone = upside > 0.10 ? "good" : (upside < -0.10 ? "bad" : "warn");
        margin_val_->setProperty("tone", QString::fromLatin1(tone));
        repolish(margin_val_);
        if (upside > 0.20)
            set_badge(verdict_badge_, tr("BELOW DCF VALUE"), "good");
        else if (upside < -0.20)
            set_badge(verdict_badge_, tr("ABOVE DCF VALUE"), "bad");
        else
            set_badge(verdict_badge_, tr("NEAR DCF VALUE"), "warn");
    } else {
        margin_val_->setText(fmt::dash());
    }
    if (!warnings.isEmpty()) {
        dcf_note_->setText(warnings.join(QLatin1Char(' ')));
        dcf_note_->setVisible(true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Scores
// ─────────────────────────────────────────────────────────────────────────────

void EquityValuationTab::update_statements_note() {
    if (!scoring_note_)
        return;
    if (financials_failed_)
        scoring_note_->setText(tr("Annual statements could not be loaded for %1 — open the FINANCIALS tab and press "
                                  "RETRY, or check the symbol.")
                                   .arg(current_symbol_));
    else if (!financials_loaded_)
        scoring_note_->setText(tr("Loading annual statements…"));
}

void EquityValuationTab::run_scoring_models() {
    namespace v = services::equity::valuation;
    altman_badge_->setVisible(false);
    piotroski_badge_->setVisible(false);
    beneish_badge_->setVisible(false);
    for (QLabel* l : {altman_val_, piotroski_val_, beneish_val_}) {
        l->setText(fmt::dash());
        l->setToolTip(QString());
    }
    if (!financials_loaded_) {
        update_statements_note();
        return;
    }

    const QVector<v::FiscalYear> years = v::fiscal_years(last_financials_);
    if (years.isEmpty()) {
        scoring_note_->setText(tr("No annual statements on file for %1 — ETFs, funds, indices and some foreign "
                                  "listings have none.")
                                   .arg(current_symbol_));
        return;
    }
    const v::FiscalYear& cur = years.first();
    QStringList notes;
    notes << (years.size() >= 2 ? tr("Fiscal year ended %1 vs %2 (annual statements, as reported).")
                                      .arg(cur.period, years.at(1).period)
                                : tr("Fiscal year ended %1 (annual statements, as reported).").arg(cur.period));
    if (last_financials_.stale)
        notes << tr("Cached statements — refreshing.");
    if (last_info_.sector.contains(QLatin1String("Financial"), Qt::CaseInsensitive))
        notes << tr("Altman Z and Beneish M were built for industrial companies; read them with caution for banks "
                    "and insurers.");

    // ── Altman Z ──
    if (currency_mismatch()) {
        altman_val_->setText(tr("n/c"));
        set_badge(altman_badge_, tr("CURRENCY MISMATCH"), "none");
        altman_val_->setToolTip(tr("Market cap (%1) and statements (%2) are in different currencies.")
                                    .arg(last_info_.currency, statements_currency()));
    } else {
        const auto a = v::altman_z(cur, last_info_.market_cap);
        if (!a.ok) {
            set_badge(altman_badge_, tr("NO DATA — %1").arg(a.missing), "none");
        } else {
            altman_val_->setText(QString::number(a.z, 'f', 2));
            altman_val_->setToolTip(tr("X1 working capital/assets %1 · X2 retained earnings/assets %2 · X3 EBIT/assets "
                                       "%3 · X4 market cap/liabilities %4 · X5 sales/assets %5")
                                        .arg(a.x1, 0, 'f', 3)
                                        .arg(a.x2, 0, 'f', 3)
                                        .arg(a.x3, 0, 'f', 3)
                                        .arg(a.x4, 0, 'f', 3)
                                        .arg(a.x5, 0, 'f', 3));
            if (a.z > 2.99)
                set_badge(altman_badge_, tr("SAFE ZONE (> 2.99)"), "good");
            else if (a.z >= 1.81)
                set_badge(altman_badge_, tr("GREY ZONE (1.81–2.99)"), "warn");
            else
                set_badge(altman_badge_, tr("DISTRESS ZONE (< 1.81)"), "bad");
        }
    }

    // ── Piotroski F / Beneish M need two years ──
    if (years.size() < 2) {
        set_badge(piotroski_badge_, tr("NO DATA — one fiscal year on file"), "none");
        set_badge(beneish_badge_, tr("NO DATA — one fiscal year on file"), "none");
    } else {
        const v::FiscalYear& prev = years.at(1);
        const auto p = v::piotroski_f(cur, prev);
        QStringList lines;
        for (const auto& t : p.tests)
            lines << QStringLiteral("%1  %2").arg(t.second == 1 ? QStringLiteral("✓")
                                                                : (t.second == 0 ? QStringLiteral("✗")
                                                                                 : QStringLiteral("·")),
                                                  t.first);
        if (!p.ok) {
            set_badge(piotroski_badge_, tr("NO DATA — %1").arg(p.missing), "none");
        } else {
            piotroski_val_->setText(QStringLiteral("%1 / %2").arg(p.score).arg(p.tests_available));
            if (p.score >= 8)
                set_badge(piotroski_badge_, tr("STRONG (8–9)"), "good");
            else if (p.score >= 5)
                set_badge(piotroski_badge_, tr("AVERAGE (5–7)"), "warn");
            else
                set_badge(piotroski_badge_, tr("WEAK (0–4)"), "bad");
        }
        piotroski_val_->setToolTip(lines.join(QLatin1Char('\n')));

        const auto b = v::beneish_m(cur, prev);
        QStringList comps;
        for (const auto& c : b.components)
            comps << QStringLiteral("%1 %2").arg(c.first, QString::number(c.second, 'f', 3));
        if (!b.components.isEmpty())
            beneish_val_->setToolTip(comps.join(QStringLiteral(" · ")) +
                                     (b.imputed > 0 ? tr("\n%1 index(es) set to the neutral 1.0 — statement line not "
                                                         "reported.")
                                                          .arg(b.imputed)
                                                    : QString()));
        if (!b.ok) {
            set_badge(beneish_badge_, tr("NO DATA — %1").arg(b.missing), "none");
        } else {
            beneish_val_->setText(QString::number(b.m, 'f', 2));
            if (b.m > services::equity::valuation::kBeneishThreshold)
                set_badge(beneish_badge_, tr("MANIPULATION RISK (> −1.78)"), "bad");
            else
                set_badge(beneish_badge_, tr("UNLIKELY MANIPULATOR (≤ −1.78)"), "good");
        }
    }
    scoring_note_->setText(notes.join(QLatin1Char(' ')));
}

// ─────────────────────────────────────────────────────────────────────────────
// Multiples
// ─────────────────────────────────────────────────────────────────────────────

void EquityValuationTab::run_multiples() {
    const auto& s = last_info_;
    auto times = [](double v) { return v > 0.0 ? QString::number(v, 'f', 1) + QLatin1Char('x') : fmt::dash(); };
    if (currency_mismatch()) {
        for (QLabel* l : {ev_ebitda_val_, ev_revenue_val_, price_sales_val_, fcf_yield_val_, graham_val_})
            l->setText(tr("n/c"));
        multiples_note_->setText(tr("Price is in %1 but the statements are in %2 — Yahoo's TTM ratios mix the two, so "
                                    "they are not shown.")
                                     .arg(s.currency, statements_currency()));
        return;
    }
    ev_ebitda_val_->setText(times(s.ev_to_ebitda));
    ev_revenue_val_->setText(times(s.ev_to_revenue));
    price_sales_val_->setText(s.total_revenue > 0.0 && s.market_cap > 0.0 ? times(s.market_cap / s.total_revenue)
                                                                           : fmt::dash());
    fcf_yield_val_->setText(s.market_cap > 0.0 && s.free_cashflow != 0.0 ? fmt::pct(s.free_cashflow / s.market_cap, 2)
                                                                         : fmt::dash());
    // Graham number = √(22.5 × EPS × BVPS); EPS from price ÷ trailing P/E.
    const double eps = (s.pe_ratio > 0.0 && s.current_price > 0.0) ? s.current_price / s.pe_ratio : 0.0;
    const double graham = (eps > 0.0 && s.book_value > 0.0) ? std::sqrt(22.5 * eps * s.book_value) : 0.0;
    graham_val_->setText(graham > 0.0 ? fmt_money(graham, s.currency) : fmt::dash());
    multiples_note_->setText(s.symbol.isEmpty() ? QString()
                                                : tr("Trailing-12-month figures from Yahoo Finance; FCF yield = "
                                                     "levered FCF ÷ market cap."));
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

QString EquityValuationTab::statements_currency() const {
    return last_info_.financial_currency.isEmpty() ? last_info_.currency : last_info_.financial_currency;
}

bool EquityValuationTab::currency_mismatch() const {
    return !last_info_.financial_currency.isEmpty() && !last_info_.currency.isEmpty() &&
           last_info_.financial_currency.compare(last_info_.currency, Qt::CaseInsensitive) != 0;
}

void EquityValuationTab::clear_results() {
    for (QLabel* l : {intrinsic_val_, current_price_val_, margin_val_, terminal_share_val_, altman_val_,
                      piotroski_val_, beneish_val_, ev_ebitda_val_, ev_revenue_val_, price_sales_val_, fcf_yield_val_,
                      graham_val_}) {
        if (l) {
            l->setText(fmt::dash());
            l->setToolTip(QString());
        }
    }
    for (QLabel* b : {verdict_badge_, altman_badge_, piotroski_badge_, beneish_badge_})
        if (b)
            b->setVisible(false);
    if (dcf_note_)
        dcf_note_->setVisible(false);
    if (scoring_note_)
        scoring_note_->clear();
    if (multiples_note_)
        multiples_note_->clear();
}

void EquityValuationTab::set_badge(QLabel* badge, const QString& text, const char* tone) {
    if (!badge)
        return;
    badge->setText(text);
    badge->setProperty("tone", QString::fromLatin1(tone));
    repolish(badge);
    badge->setVisible(true);
}

QString EquityValuationTab::fmt_money(double value, const QString& currency) const {
    if (!std::isfinite(value))
        return fmt::dash();
    return fmt::price(value, currency.isEmpty() ? QStringLiteral("USD") : currency, std::fabs(value) >= 1000.0 ? 0 : 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// i18n
// ─────────────────────────────────────────────────────────────────────────────

void EquityValuationTab::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        for (auto it = i18n_labels_.begin(); it != i18n_labels_.end(); ++it)
            it.key()->setText(tr(it.value()));
        if (calc_btn_)
            calc_btn_->setText(tr("CALCULATE"));
        if (!current_symbol_.isEmpty()) {
            run_multiples();
            if (financials_loaded_)
                run_scoring_models();
        }
    }
    QWidget::changeEvent(e);
}

} // namespace fincept::screens
