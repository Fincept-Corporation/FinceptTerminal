// src/screens/equity_research/EquityValuationTab.cpp
//
// Layout, inputs (statement-derived bases, CAPM / WACC, risk-free by the reporting
// currency), Altman / Piotroski / Beneish scores and the cross-check multiples. The model
// run and its renderings live in EquityValuationTab_Dcf.cpp.
#include "screens/equity_research/EquityValuationTab.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/equity_research/EquityResearchFormat.h"
#include "screens/equity_research/EquityResearchUi.h"
#include "screens/equity_research/EquityStatementModel.h"
#include "services/equity/EquityResearchService.h"
#include "services/equity/EquityValuationModels.h"
#include "services/markets/FxRateService.h"
#include "services/markets/MarketDataService.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLocale>
#include <QPointer>
#include <QScrollArea>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace valuation_tab_detail {

namespace val_uf = fincept::ui::formatting;
namespace fmt = equity_fmt;

// Until the US 10-year quote (or the local yield) arrives.
constexpr double kRiskFreeFallback = 4.5;
constexpr double kDefaultErp = 5.0;
constexpr double kTreasuryYieldMaxPct = 20.0;
constexpr qint64 kRiskFreeRefreshMs = 15LL * 60 * 1000;

QString treasury_topic() {
    return QStringLiteral("market:quote:^TNX");
}

void val_repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

bool val_ok(double v) {
    return std::isfinite(v);
}

/// Approximate country risk premium (percent): the sovereign default spread of the
/// country's rating bucket. A starting point the analyst is expected to check and edit.
double country_risk_premium_pct(const QString& country) {
    static const QHash<QString, double> kCrp = {
        {"United States", 0.0}, {"Canada", 0.0},     {"Germany", 0.0},       {"Netherlands", 0.0},
        {"Switzerland", 0.0},   {"Sweden", 0.0},     {"Norway", 0.0},        {"Denmark", 0.0},
        {"Australia", 0.0},     {"Singapore", 0.0},  {"Luxembourg", 0.0},    {"New Zealand", 0.0},
        {"Finland", 0.4},       {"Austria", 0.4},    {"United Kingdom", 0.5}, {"Ireland", 0.6},
        {"France", 0.6},        {"Belgium", 0.6},    {"Hong Kong", 0.6},     {"South Korea", 0.5},
        {"Taiwan", 0.6},        {"Japan", 0.8},      {"China", 0.9},         {"United Arab Emirates", 0.6},
        {"Saudi Arabia", 0.9},  {"Israel", 1.0},     {"Chile", 1.0},         {"Spain", 1.2},
        {"Poland", 1.2},        {"Malaysia", 1.3},   {"Portugal", 1.5},      {"Thailand", 1.7},
        {"Italy", 2.0},         {"Philippines", 2.0}, {"India", 2.2},        {"Indonesia", 2.2},
        {"Mexico", 2.4},        {"Greece", 2.4},     {"Colombia", 3.0},      {"Brazil", 3.0},
        {"South Africa", 3.3},  {"Turkey", 5.5},     {"Egypt", 6.5},         {"Nigeria", 6.0},
        {"Argentina", 9.0},     {"Pakistan", 9.0},
    };
    return kCrp.value(country.trimmed(), std::nan(""));
}

} // namespace valuation_tab_detail

using namespace valuation_tab_detail;

// ═════════════════════════════════════════════════════════════════════════════
// Construction
// ═════════════════════════════════════════════════════════════════════════════

EquityValuationTab::EquityValuationTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    // A failed statements fetch must not leave the model waiting for them forever.
    connect(&svc, &services::equity::EquityResearchService::load_failed, this,
            [this](const services::equity::LoadFailure& f) {
                if (f.leg != QLatin1String("Financials") || financials_loaded_ || current_symbol_.isEmpty() ||
                    f.symbol != current_symbol_)
                    return;
                financials_failed_ = true;
                if (status_)
                    status_->show_error(tr("Statements could not be loaded — the model falls back to Yahoo's trailing "
                                           "figures."),
                                        f.message);
                pre_fill_inputs();
                run_scoring_models();
            });
    connect(&svc, &services::equity::EquityResearchService::historical_loaded, this,
            [this](const QString& symbol, const QString& period, const QVector<services::equity::Candle>& candles) {
                if (symbol != current_symbol_ || period != QLatin1String("max") || candles.isEmpty())
                    return;
                daily_ = candles;
                render_history();
            });
    connect(&svc, &services::equity::EquityResearchService::peers_loaded, this,
            [this](const QVector<services::equity::PeerData>& peers) {
                bool mine = false;
                for (const auto& p : peers)
                    mine = mine || p.symbol.compare(current_symbol_, Qt::CaseInsensitive) == 0;
                if (!mine)
                    return;
                peers_ = peers;
                peers_pending_ = false;
                render_history();
            });
    connect(&svc, &services::equity::EquityResearchService::suggested_peers_loaded, this,
            [this](const QString& symbol, const QStringList& peers) {
                if (symbol != current_symbol_ || !peers_btn_ || !peers_pending_)
                    return;
                if (peers.isEmpty()) {
                    peers_pending_ = false;
                    peers_btn_->setEnabled(true);
                    if (hist_note_)
                        hist_note_->setText(tr("No same-industry peers found for %1.").arg(symbol));
                    return;
                }
                services::equity::EquityResearchService::instance().fetch_peers(symbol, peers);
                peers_btn_->setEnabled(true);
            });
    // A failed peer pull ends the "finding peers" note with the reason (the column
    // keeps "—"); the button retries.
    connect(&svc, &services::equity::EquityResearchService::load_failed, this,
            [this](const services::equity::LoadFailure& f) {
                if (f.leg != QLatin1String("Peers") || f.symbol != current_symbol_ || !peers_pending_)
                    return;
                peers_pending_ = false;
                if (peers_btn_)
                    peers_btn_->setEnabled(true);
                if (hist_note_)
                    hist_note_->setText(tr("Peer medians unavailable: %1").arg(f.message.left(200)));
            });
    // A price in another currency than the statements compares once the FX leg lands.
    connect(&services::FxRateService::instance(), &services::FxRateService::rates_changed, this, [this]() {
        if (!current_symbol_.isEmpty() && prefilled_for_ == current_symbol_ &&
            !equity_fmt::listing_of(last_info_).same_currency())
            on_calculate_clicked();
    });
    // Recompute from the inputs held once a theme / font change has settled, and for a
    // currency / number-format change (synchronous, no new request).
    const auto rerender = [this]() {
        if (!current_symbol_.isEmpty() && prefilled_for_ == current_symbol_)
            on_calculate_clicked();
    };
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, rerender);
    ui::formatting::on_format_changed(this, rerender);
}

// ═════════════════════════════════════════════════════════════════════════════
// UI
// ═════════════════════════════════════════════════════════════════════════════

void EquityValuationTab::build_ui() {
    setObjectName(QStringLiteral("equityValuationTab"));
    er_ui::apply_style(this, []() {
        namespace c = ui::colors;
        namespace f = ui::fonts;
        return QStringLiteral(
                   "#equityValuationTab, QWidget#valContent { background:%1; }"
                   "QScrollArea#valScroll { background:transparent; border:0; }"
                   "QFrame#valBox { background:%2; border:1px solid %3; }"
                   "QLabel { background:transparent; border:0; }"
                   "QLabel#valBoxTitle { color:%4; font-size:%10px; font-weight:700; letter-spacing:1px; }"
                   "QLabel#valKey { color:%5; font-size:%10px; }"
                   "QLabel#valValue { color:%6; font-size:%11px; font-weight:600; }"
                   "QLabel#valValue[tone=\"good\"] { color:%7; } QLabel#valValue[tone=\"bad\"] { color:%8; }"
                   "QLabel#valValue[tone=\"warn\"] { color:%4; }"
                   "QLabel#valNote { color:%9; font-size:%12px; }")
                   .arg(c::BG_BASE(), c::BG_SURFACE(), c::BORDER_DIM(), c::AMBER(), c::TEXT_SECONDARY(),
                        c::TEXT_PRIMARY(), c::SUCCESS(), c::DANGER(), c::TEXT_TERTIARY())
                   .arg(f::px(-3), f::px(-1), f::px(-4)) +
               QStringLiteral(
                   "QLabel#valWarn { color:%1; font-size:%8px; }"
                   "QDoubleSpinBox, QComboBox { background:%2; color:%3; border:1px solid %4; padding:2px 4px;"
                   " font-size:%9px; }"
                   "QPushButton#valCalc { background:%5; color:%6; border:0; padding:5px 14px; font-size:%8px;"
                   " font-weight:700; letter-spacing:1px; }"
                   "QPushButton#valCalc:hover { background:%7; }")
                   .arg(c::WARNING(), c::BG_RAISED(), c::TEXT_PRIMARY(), c::BORDER_DIM(), c::AMBER(),
                        c::TEXT_ON_ACCENT(), c::AMBER_DIM(), f::px(-3), f::px(-2)) +
               QStringLiteral(
                   "QLabel#valBadge { padding:2px 10px; font-size:%7px; font-weight:700; color:%1; background:%2; }"
                   "QLabel#valBadge[tone=\"good\"] { background:%3; color:%6; }"
                   "QLabel#valBadge[tone=\"bad\"] { background:%4; color:%6; }"
                   "QLabel#valBadge[tone=\"warn\"] { background:%5; color:%6; }"
                   "QLabel#valBadge[tone=\"none\"] { background:transparent; color:%8; border:1px solid %9; }"
                   "QCheckBox#valFade { color:%10; font-size:%11px; }"
                   "QTableWidget#valSens, QTableWidget#valProj { background:%12; color:%1; gridline-color:%9;"
                   " border:1px solid %9; font-size:%11px; }"
                   "QTableWidget#valSens QHeaderView::section, QTableWidget#valProj QHeaderView::section {"
                   " background:%2; color:%10; border:0; border-bottom:1px solid %9; padding:3px 6px; font-size:%7px;"
                   " font-weight:700; }")
                   .arg(c::TEXT_PRIMARY(), c::BG_RAISED(), c::SUCCESS(), c::DANGER(), c::AMBER(), c::TEXT_ON_ACCENT(),
                        f::px(-4), c::TEXT_TERTIARY(), c::BORDER_MED())
                   .arg(c::TEXT_SECONDARY(), f::px(-3), c::BG_SURFACE()) +
               er_ui::stylesheet();
    });

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
    status_ = new er_ui::StatusStrip(content);
    connect(status_, &er_ui::StatusStrip::retry_requested, this, [this]() {
        if (current_symbol_.isEmpty())
            return;
        financials_failed_ = false;
        status_->show_loading(tr("Loading statements…"));
        services::equity::EquityResearchService::instance().fetch_financials(current_symbol_);
        services::equity::EquityResearchService::instance().load_info_only(current_symbol_);
    });
    vbox->addWidget(status_);
    vbox->addWidget(build_dcf_section());
    vbox->addWidget(build_history_section());
    vbox->addWidget(build_scoring_section());
    vbox->addWidget(build_multiples_section());
    vbox->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll);
    // Texts of the method / sensitivity controls are set by the retranslation pass.
    QEvent language(QEvent::LanguageChange);
    changeEvent(&language);
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

QDoubleSpinBox* EquityValuationTab::make_spin(double min, double max, double value, const QString& suffix,
                                              int decimals) {
    auto* spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setDecimals(decimals);
    spin->setValue(value);
    spin->setSuffix(suffix);
    spin->setAlignment(Qt::AlignRight);
    spin->setGroupSeparatorShown(true);
    spin->setKeyboardTracking(false);
    ui::metrics::follow_fixed_width(spin, 150);
    return spin;
}

QWidget* EquityValuationTab::build_dcf_section() {
    QVBoxLayout* body = nullptr;
    auto* box = make_box(QT_TR_NOOP("INTRINSIC VALUE"), body);

    method_seg_ = new er_ui::Segmented(box);
    method_seg_->add(QStringLiteral("fcfe"), QString());
    method_seg_->add(QStringLiteral("fcff"), QString());
    method_seg_->add(QStringLiteral("ddm"), QString());
    method_seg_->add(QStringLiteral("ri"), QString());
    connect(method_seg_, &er_ui::Segmented::changed, this, [this](const QString& k) {
        method_ = k == QLatin1String("fcff") ? Method::Fcff
                  : k == QLatin1String("ddm") ? Method::Ddm
                  : k == QLatin1String("ri")  ? Method::ResidualIncome
                                              : Method::Fcfe;
        discount_overridden_ = false;
        apply_method_visibility();
        apply_base_source();
        apply_discount();
        on_calculate_clicked();
    });
    body->addWidget(method_seg_, 0, Qt::AlignLeft);

    auto* row = new QHBoxLayout;
    row->setSpacing(28);

    // ── Inputs ───────────────────────────────────────────────────────────────
    auto* inputs = new QWidget(box);
    auto* grid = new QGridLayout(inputs);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(5);
    int r = 0;
    auto add_row = [&](const char* key, QWidget* w, QList<QWidget*>* group = nullptr) {
        auto* k = add_key_label(inputs, key);
        grid->addWidget(k, r, 0);
        grid->addWidget(w, r, 1);
        if (group)
            *group << k << w;
        ++r;
        return k;
    };
    base_source_ = new QComboBox(inputs);
    connect(base_source_, &QComboBox::currentIndexChanged, this, [this]() {
        apply_base_source();
        on_calculate_clicked();
    });
    add_row(QT_TR_NOOP("BASE CASH FLOW FROM"), base_source_, &dcf_only_);
    // Base cash flow in millions of the reporting currency. The range covers the largest
    // KRW / JPY / IDR filers (Samsung's FCF is ~10 trillion won = 10,000,000 M).
    fcf_input_ = make_spin(-1e12, 1e12, 0.0, tr(" M"), 1);
    base_key_ = add_row(QT_TR_NOOP("BASE CASH FLOW"), fcf_input_, &dcf_only_);
    growth_input_ = make_spin(-50.0, 100.0, 6.0, QStringLiteral(" %"), 2);
    add_row(QT_TR_NOOP("GROWTH (STAGE 1)"), growth_input_);
    terminal_input_ = make_spin(-2.0, 10.0, 2.5, QStringLiteral(" %"), 2);
    add_row(QT_TR_NOOP("TERMINAL GROWTH"), terminal_input_);
    years_input_ = make_spin(1.0, 30.0, 10.0, tr(" yrs"), 0);
    add_row(QT_TR_NOOP("PROJECTION YEARS"), years_input_);
    shares_input_ = make_spin(0.0001, 1e9, 1.0, tr(" M"), 3);
    add_row(QT_TR_NOOP("SHARES OUTSTANDING"), shares_input_);
    fade_check_ = new QCheckBox(tr("Fade growth to terminal rate"), inputs);
    fade_check_->setObjectName(QStringLiteral("valFade"));
    fade_check_->setChecked(true);
    grid->addWidget(fade_check_, r++, 0, 1, 2);
    dcf_only_ << fade_check_;

    rf_input_ = make_spin(-2.0, 40.0, kRiskFreeFallback, QStringLiteral(" %"), 2);
    add_row(QT_TR_NOOP("RISK-FREE (10Y GOVT)"), rf_input_);
    rf_source_lbl_ = new QLabel(inputs);
    rf_source_lbl_->setObjectName(QStringLiteral("valNote"));
    rf_source_lbl_->setWordWrap(true);
    grid->addWidget(rf_source_lbl_, r++, 0, 1, 2);
    erp_input_ = make_spin(0.0, 20.0, kDefaultErp, QStringLiteral(" %"), 2);
    add_row(QT_TR_NOOP("EQUITY RISK PREMIUM"), erp_input_);
    crp_input_ = make_spin(0.0, 25.0, 0.0, QStringLiteral(" %"), 2);
    add_row(QT_TR_NOOP("COUNTRY RISK PREMIUM"), crp_input_);
    beta_input_ = make_spin(-1.0, 5.0, 1.0, QString(), 2);
    add_row(QT_TR_NOOP("BETA"), beta_input_);
    kd_input_ = make_spin(0.0, 40.0, 5.0, QStringLiteral(" %"), 2);
    add_row(QT_TR_NOOP("COST OF DEBT (PRE-TAX)"), kd_input_, &fcff_only_);
    tax_input_ = make_spin(0.0, 60.0, 25.0, QStringLiteral(" %"), 1);
    add_row(QT_TR_NOOP("TAX RATE"), tax_input_, &fcff_only_);
    discount_input_ = make_spin(1.0, 60.0, 10.0, QStringLiteral(" %"), 2);
    discount_key_ = add_row(QT_TR_NOOP("DISCOUNT RATE"), discount_input_);

    auto* btns = new QHBoxLayout;
    capm_btn_ = new QPushButton(inputs);
    capm_btn_->setObjectName(QStringLiteral("erBtn"));
    capm_btn_->setCursor(Qt::PointingHandCursor);
    connect(capm_btn_, &QPushButton::clicked, this, [this]() {
        discount_overridden_ = false;
        apply_discount();
        on_calculate_clicked();
    });
    btns->addWidget(capm_btn_);
    calc_btn_ = new QPushButton(tr("CALCULATE"), inputs);
    calc_btn_->setObjectName(QStringLiteral("valCalc"));
    calc_btn_->setCursor(Qt::PointingHandCursor);
    calc_btn_->setDefault(true);
    connect(calc_btn_, &QPushButton::clicked, this, &EquityValuationTab::on_calculate_clicked);
    btns->addWidget(calc_btn_);
    grid->addLayout(btns, r++, 0, 1, 2);

    auto recalc = [this]() {
        if (!current_symbol_.isEmpty())
            on_calculate_clicked();
    };
    for (auto* spin : {fcf_input_, growth_input_, terminal_input_, years_input_, shares_input_})
        connect(spin, &QDoubleSpinBox::editingFinished, this, recalc);
    connect(fade_check_, &QCheckBox::toggled, this, recalc);
    // A rate input rebuilds the discount (unless the discount itself was typed).
    for (auto* spin : {erp_input_, crp_input_, beta_input_, kd_input_, tax_input_})
        connect(spin, &QDoubleSpinBox::editingFinished, this, [this]() {
            apply_discount();
            on_calculate_clicked();
        });
    connect(rf_input_, &QDoubleSpinBox::editingFinished, this, [this]() {
        if (std::fabs(rf_input_->value() - rf_shown_pct_) < 1e-9)
            return;
        rf_shown_pct_ = rf_input_->value();
        rf_source_ = RfSource::User;
        apply_discount();
        on_calculate_clicked();
    });
    connect(discount_input_, &QDoubleSpinBox::editingFinished, this, [this]() {
        discount_overridden_ = true;
        on_calculate_clicked();
    });
    row->addWidget(inputs);

    // ── Results ──────────────────────────────────────────────────────────────
    auto* results = new QWidget(box);
    auto* rg = new QGridLayout(results);
    rg->setContentsMargins(0, 0, 0, 0);
    rg->setHorizontalSpacing(12);
    rg->setVerticalSpacing(5);
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
    intrinsic_quote_val_ = make_result(QT_TR_NOOP("IN THE QUOTE CURRENCY"));
    current_price_val_ = make_result(QT_TR_NOOP("CURRENT PRICE"));
    margin_val_ = make_result(QT_TR_NOOP("UPSIDE TO INTRINSIC"));
    bridge_val_ = make_result(QT_TR_NOOP("EV → EQUITY BRIDGE"));
    bridge_val_->setWordWrap(true);
    terminal_share_val_ = make_result(QT_TR_NOOP("TERMINAL VALUE SHARE"));
    implied_growth_val_ = make_result(QT_TR_NOOP("MARKET-IMPLIED GROWTH"));
    implied_growth_val_->setToolTip(tr("Reverse DCF: the stage-1 growth at which the model's value equals today's "
                                       "price, every other input unchanged — what the market is pricing in."));
    verdict_badge_ = new QLabel(results);
    verdict_badge_->setObjectName(QStringLiteral("valBadge"));
    verdict_badge_->setAlignment(Qt::AlignCenter);
    verdict_badge_->setVisible(false);
    rg->addWidget(verdict_badge_, rr++, 0, 1, 2, Qt::AlignLeft);
    // Bear / base / bull.
    auto* scen = new QHBoxLayout;
    scen->setSpacing(8);
    for (int i = 0; i < 3; ++i) {
        scenario_lbl_[i] = new QLabel(results);
        scenario_lbl_[i]->setObjectName(QStringLiteral("valValue"));
        scenario_lbl_[i]->setTextFormat(Qt::RichText);
        scen->addWidget(scenario_lbl_[i]);
    }
    scen->addStretch(1);
    // Spare width goes to an empty third column: the values sit next to their labels
    // instead of ~600 px to the right of them at 1600 px.
    rg->addLayout(scen, rr++, 0, 1, 3);
    rg->setColumnStretch(2, 1);
    rg->setRowStretch(rr, 1);
    row->addWidget(results, 1);
    body->addLayout(row);

    dcf_note_ = new QLabel(box);
    dcf_note_->setObjectName(QStringLiteral("valWarn"));
    dcf_note_->setWordWrap(true);
    dcf_note_->setVisible(false);
    body->addWidget(dcf_note_);

    // ── Projection: table + chart ────────────────────────────────────────────
    auto* proj_row = new QHBoxLayout;
    proj_row->setSpacing(10);
    proj_table_ = new QTableWidget(box);
    proj_table_->setObjectName(QStringLiteral("valProj"));
    proj_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    proj_table_->setSelectionMode(QAbstractItemView::NoSelection);
    proj_table_->verticalHeader()->hide();
    proj_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui::metrics::follow_row_height(proj_table_, 22);
    proj_table_->setMinimumHeight(ui::metrics::height(240));
    proj_row->addWidget(proj_table_, 1);
    proj_chart_ = new ui::BarChart(box);
    proj_chart_->setMinimumHeight(ui::metrics::height(240));
    proj_row->addWidget(proj_chart_, 1);
    body->addLayout(proj_row);

    // ── Sensitivity ──────────────────────────────────────────────────────────
    auto* sens_ctrl = new QHBoxLayout;
    sens_title_ = new QLabel(box);
    sens_title_->setObjectName(QStringLiteral("valKey"));
    sens_ctrl->addWidget(sens_title_);
    sens_ctrl->addStretch();
    sens_cols_ = new QComboBox(box);
    sens_size_ = new QComboBox(box);
    sens_size_->addItems({QStringLiteral("5 × 5"), QStringLiteral("7 × 7")});
    sens_step_rows_ = make_spin(0.1, 5.0, 1.0, QStringLiteral(" pp"), 2);
    sens_step_cols_ = make_spin(0.1, 5.0, 0.5, QStringLiteral(" pp"), 2);
    for (QWidget* w : {static_cast<QWidget*>(sens_cols_), static_cast<QWidget*>(sens_size_),
                       static_cast<QWidget*>(sens_step_rows_), static_cast<QWidget*>(sens_step_cols_)})
        sens_ctrl->addWidget(w);
    connect(sens_cols_, &QComboBox::currentIndexChanged, this, [this]() { update_sensitivity(); });
    connect(sens_size_, &QComboBox::currentIndexChanged, this, [this]() { update_sensitivity(); });
    connect(sens_step_rows_, &QDoubleSpinBox::editingFinished, this, [this]() { update_sensitivity(); });
    connect(sens_step_cols_, &QDoubleSpinBox::editingFinished, this, [this]() { update_sensitivity(); });
    body->addLayout(sens_ctrl);
    sens_table_ = new QTableWidget(5, 5, box);
    sens_table_->setObjectName(QStringLiteral("valSens"));
    sens_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sens_table_->setSelectionMode(QAbstractItemView::NoSelection);
    sens_table_->setFocusPolicy(Qt::NoFocus);
    sens_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui::metrics::follow_row_height(sens_table_, 24);
    sens_table_->setMinimumHeight(ui::metrics::height(24) * 8 + 8);
    body->addWidget(sens_table_);

    assumptions_lbl_ = new QLabel(tr("Open a symbol to pre-fill the model from its statements, beta and share count."), box);
    assumptions_lbl_->setObjectName(QStringLiteral("valNote"));
    assumptions_lbl_->setWordWrap(true);
    body->addWidget(assumptions_lbl_);
    return box;
}

QWidget* EquityValuationTab::build_history_section() {
    QVBoxLayout* body = nullptr;
    auto* box = make_box(QT_TR_NOOP("MULTIPLES VS OWN HISTORY AND PEERS"), body);
    auto* top = new QHBoxLayout;
    hist_note_ = new QLabel(box);
    hist_note_->setObjectName(QStringLiteral("valNote"));
    hist_note_->setWordWrap(true);
    top->addWidget(hist_note_, 1);
    peers_btn_ = new QPushButton(tr("REFRESH PEER MEDIANS"), box);
    peers_btn_->setObjectName(QStringLiteral("erBtn"));
    peers_btn_->setCursor(Qt::PointingHandCursor);
    peers_btn_->setToolTip(tr("Peer medians load with the symbol; this pulls them again"));
    connect(peers_btn_, &QPushButton::clicked, this, &EquityValuationTab::request_peer_multiples);
    top->addWidget(peers_btn_);
    body->addLayout(top);
    auto* row = new QHBoxLayout;
    hist_chart_ = new ui::TimeSeriesChart(box);
    row->addWidget(hist_chart_, 3);
    reset_hist_chart(); // configures it
    hist_table_ = new QTableWidget(box);
    hist_table_->setObjectName(QStringLiteral("valProj"));
    hist_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    hist_table_->setSelectionMode(QAbstractItemView::NoSelection);
    hist_table_->verticalHeader()->hide();
    hist_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    hist_table_->horizontalHeader()->setStretchLastSection(true);
    ui::metrics::follow_row_height(hist_table_, 22);
    row->addWidget(hist_table_, 2);
    body->addLayout(row);
    return box;
}

void EquityValuationTab::reset_hist_chart() {
    if (!hist_chart_)
        return;
    auto* chart = hist_chart_;
    // Only a chart that has been drawn into needs replacing (build time configures the first).
    if (chart->pane_count() > 1) {
        QWidget* host = chart->parentWidget();
        auto* fresh = new ui::TimeSeriesChart(host);
        if (host && host->layout()) {
            if (QLayoutItem* old = host->layout()->replaceWidget(chart, fresh))
                delete old;
        }
        chart->hide();
        chart->deleteLater();
        chart = hist_chart_ = fresh;
    } else {
        chart->clear();
    }
    chart->setMinimumHeight(ui::metrics::height(260));
    chart->set_utc(true);
    chart->set_transparent_background(true);
    chart->set_show_legend(true);
    chart->set_range_presets({QStringLiteral("3Y"), QStringLiteral("5Y"), QStringLiteral("10Y"), QStringLiteral("MAX")});
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
    make_score(QT_TR_NOOP("ALTMAN Z″ (NON-MANUFACTURER)"), altman2_val_, altman2_badge_);
    make_score(QT_TR_NOOP("PIOTROSKI F-SCORE"), piotroski_val_, piotroski_badge_);
    make_score(QT_TR_NOOP("BENEISH M-SCORE"), beneish_val_, beneish_badge_);
    grid->setColumnStretch(3, 1);
    body->addLayout(grid);
    scoring_note_ = new QLabel(box);
    scoring_note_->setObjectName(QStringLiteral("valNote"));
    scoring_note_->setWordWrap(true);
    body->addWidget(scoring_note_);
    // Multi-year trend of the scores and the Piotroski pass / fail grid.
    auto* hist = new QHBoxLayout;
    hist->setSpacing(10);
    for (QTableWidget** t : {&score_history_, &piotroski_grid_}) {
        *t = new QTableWidget(box);
        (*t)->setObjectName(QStringLiteral("valProj"));
        (*t)->setEditTriggers(QAbstractItemView::NoEditTriggers);
        (*t)->setSelectionMode(QAbstractItemView::NoSelection);
        (*t)->verticalHeader()->hide();
        (*t)->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        ui::metrics::follow_row_height(*t, 22);
        hist->addWidget(*t, 1);
    }
    score_history_->setMinimumHeight(ui::metrics::height(22) * 6 + 8);
    piotroski_grid_->setMinimumHeight(ui::metrics::height(22) * 11 + 8);
    body->addLayout(hist);
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
                               "defensive investor (P/E ≤ 15 and P/B ≤ 1.5), in the reporting currency."));
    multiples_note_ = new QLabel(box);
    multiples_note_->setObjectName(QStringLiteral("valNote"));
    multiples_note_->setWordWrap(true);
    body->addWidget(multiples_note_);
    return box;
}

// ═════════════════════════════════════════════════════════════════════════════
// Public setters
// ═════════════════════════════════════════════════════════════════════════════

void EquityValuationTab::set_symbol(const QString& symbol) {
    if (symbol.isEmpty() || symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    prefilled_for_.clear();
    prefill_notes_.clear();
    discount_overridden_ = false;
    daily_.clear();
    peers_.clear();
    peers_pending_ = false;
    if (peers_btn_)
        peers_btn_->setEnabled(true);
    if (assumptions_lbl_)
        assumptions_lbl_->setText(tr("The model pre-fills from %1's statements, beta and share count once they load.")
                                      .arg(symbol));
    clear_results();
    if (last_info_.symbol != symbol)
        last_info_ = {};
    if (last_financials_.symbol != symbol)
        last_financials_ = {};
    financials_loaded_ = !last_financials_.symbol.isEmpty();
    financials_failed_ = false;
    if (status_)
        status_->show_loading(tr("Loading statements and profile for %1…").arg(symbol));
    auto& svc = services::equity::EquityResearchService::instance();
    if (financials_loaded_)
        run_scoring_models();
    else
        svc.fetch_financials(symbol);
    if (last_info_.symbol.isEmpty())
        svc.load_info_only(symbol);
    svc.load_historical_only(symbol, QStringLiteral("max"));
    pre_fill_inputs();
    if (!last_info_.symbol.isEmpty())
        run_multiples();
    // The PEER MEDIAN column fills by default (the same two cached pulls the Peers tab
    // makes); only the visible tab starts it — a hidden one starts it on its next show.
    if (isVisible())
        request_peer_multiples();
    else
        peers_auto_on_show_ = true;
}

void EquityValuationTab::set_stock_info(const services::equity::StockInfo& info) {
    last_info_ = info;
    if (info.symbol != current_symbol_)
        return;
    equity_fmt::listing_of(info).ensure_fx();
    pre_fill_inputs();
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
    if (status_ && !data.stale)
        status_->clear();
    pre_fill_inputs();
    run_scoring_models();
    run_multiples();
    render_history();
}

// ═════════════════════════════════════════════════════════════════════════════
// Inputs
// ═════════════════════════════════════════════════════════════════════════════

QString EquityValuationTab::reporting_currency() const {
    return equity_fmt::listing_of(last_info_).reporting_major();
}

QString EquityValuationTab::quote_currency() const {
    return last_info_.currency;
}

double EquityValuationTab::price_in_reporting() const {
    const double px = last_info_.current_price;
    if (!(px > 0.0))
        return std::nan("");
    return equity_fmt::listing_of(last_info_).quote_to_reporting(px);
}

double EquityValuationTab::market_cap_in_reporting() const {
    const double mc = last_info_.market_cap; // quote's MAJOR currency
    if (!(mc > 0.0))
        return std::nan("");
    const auto listing = equity_fmt::listing_of(last_info_);
    if (listing.same_currency())
        return mc;
    const auto r = services::FxRateService::instance().rate(listing.quote_major(), listing.reporting_major());
    return r ? mc * *r : std::nan("");
}

void EquityValuationTab::pre_fill_inputs() {
    // Once per symbol, with the profile and (unless they failed) the statements in hand:
    // later refreshes must not overwrite inputs the analyst edited.
    if (!fcf_input_ || current_symbol_.isEmpty() || prefilled_for_ == current_symbol_)
        return;
    if (last_info_.symbol != current_symbol_ || (!financials_loaded_ && !financials_failed_))
        return;
    prefilled_for_ = current_symbol_;
    const auto& s = last_info_;
    prefill_notes_.clear();
    namespace fm = fin_model;
    const auto tmpl = financials_loaded_ ? fm::detect_template(last_financials_, s.sector, s.industry)
                                         : fm::Template::Industrial;

    // ── Cash-flow bases from the statements ──────────────────────────────────
    base_ttm_ = base_fy_ = base_avg3_ = base_avg5_ = std::nan("");
    fcff_ttm_ = fcff_fy_ = fcff_avg3_ = fcff_avg5_ = std::nan("");
    net_debt_ = minority_ = 0.0;
    dps_ = bvps_ = roe_ = payout_ = std::nan("");
    double tax = std::nan(""), kd = std::nan("");
    if (financials_loaded_) {
        const auto cf_annual = fm::columns_for(last_financials_, fm::Statement::CashFlow, fm::Period::Annual,
                                               fm::fiscal_year_end_month(last_financials_));
        const auto inc_annual = fm::columns_for(last_financials_, fm::Statement::Income, fm::Period::Annual,
                                                fm::fiscal_year_end_month(last_financials_));
        auto fcf_of = [](const QJsonObject& cf) {
            const double f = fm::line(cf, {"Free Cash Flow"});
            if (std::isfinite(f))
                return f;
            const double ocf = fm::line(cf, {"Operating Cash Flow"}), capex = fm::line(cf, {"Capital Expenditure"});
            return std::isfinite(ocf) && std::isfinite(capex) ? ocf + capex : std::nan("");
        };
        // Interest after tax: FCFF ≈ FCFE + interest × (1 − t) (net borrowing ignored).
        auto after_tax_interest = [&](const QDate& end) {
            for (const auto& c : inc_annual)
                if (qAbs(c.end.daysTo(end)) <= 12) {
                    const double i = fm::line(c.data, {"Interest Expense", "Interest Expense Non Operating"});
                    double t = fm::line(c.data, {"Tax Rate For Calcs"});
                    if (!std::isfinite(t))
                        t = 0.25;
                    return std::isfinite(i) ? std::fabs(i) * (1.0 - std::clamp(t, 0.0, 0.5)) : 0.0;
                }
            return 0.0;
        };
        auto tax_at = [&](const QDate& end) {
            for (const auto& c : inc_annual)
                if (qAbs(c.end.daysTo(end)) <= 12) {
                    const double t = fm::line(c.data, {"Tax Rate For Calcs"});
                    return std::isfinite(t) ? std::clamp(t, 0.0, 0.5) : 0.25;
                }
            return 0.25;
        };
        // IFRS lets a company report interest paid under FINANCING (Vodafone, many
        // European and Asian filers: Yahoo's "Interest Paid Cff"). Operating cash flow is
        // then before interest, so OCF − capex is not the cash left for shareholders:
        //   FCFE = FCF − interest paid,   FCFF = FCF − interest paid × t.
        // Unadjusted, VOD.L's FCFE base was its pre-interest cash flow and the model
        // put the value at ~10× the price.
        int interest_cff_years = 0;
        auto interest_in_cff = [](const QJsonObject& cf) {
            const double ip = fm::line(cf, {"Interest Paid Cff"});
            return std::isfinite(ip) && ip != 0.0 ? std::fabs(ip) : std::nan("");
        };
        auto fcfe_of = [&](const QJsonObject& cf) {
            const double f = fcf_of(cf), ip = interest_in_cff(cf);
            return std::isfinite(f) && std::isfinite(ip) ? f - ip : f;
        };
        auto fcff_of = [&](const QJsonObject& cf, const QDate& end) {
            const double f = fcf_of(cf), ip = interest_in_cff(cf);
            if (!std::isfinite(f))
                return f;
            return std::isfinite(ip) ? f - ip * tax_at(end) : f + after_tax_interest(end);
        };
        QVector<double> fcfe, fcff;
        for (const auto& c : cf_annual) {
            const double f = fcfe_of(c.data);
            if (std::isfinite(f)) {
                fcfe << f;
                fcff << fcff_of(c.data, c.end);
                if (std::isfinite(interest_in_cff(c.data)))
                    ++interest_cff_years;
            }
        }
        if (interest_cff_years > 0)
            prefill_notes_ << tr("interest is paid under financing (IFRS): FCFE = FCF − interest paid");
        auto avg_last = [](const QVector<double>& v, int n) {
            if (v.size() < n)
                return std::nan("");
            double a = 0;
            for (int i = v.size() - n; i < v.size(); ++i)
                a += v[i];
            return a / n;
        };
        if (!fcfe.isEmpty()) {
            base_fy_ = fcfe.last();
            fcff_fy_ = fcff.last();
            base_avg3_ = avg_last(fcfe, std::min<int>(3, fcfe.size()));
            fcff_avg3_ = avg_last(fcff, std::min<int>(3, fcff.size()));
            base_avg5_ = avg_last(fcfe, static_cast<int>(fcfe.size()));
            fcff_avg5_ = avg_last(fcff, static_cast<int>(fcff.size()));
        }
        const auto cf_ttm = fm::columns_for(last_financials_, fm::Statement::CashFlow, fm::Period::Ttm,
                                            fm::fiscal_year_end_month(last_financials_));
        if (!cf_ttm.isEmpty()) {
            const QJsonObject& ttm = cf_ttm.last().data;
            const QDate tax_end = cf_annual.isEmpty() ? QDate() : cf_annual.last().end;
            // A TTM column summed from quarters may lack the interest line even when the
            // annual statements carry it: then take the latest fiscal year's interest paid.
            double ip = interest_in_cff(ttm);
            if (!std::isfinite(ip) && interest_cff_years > 0 && !cf_annual.isEmpty())
                ip = interest_in_cff(cf_annual.last().data);
            const double f = fcf_of(ttm);
            if (std::isfinite(f) && std::isfinite(ip)) {
                base_ttm_ = f - ip;
                fcff_ttm_ = f - ip * tax_at(tax_end);
            } else {
                base_ttm_ = f;
                fcff_ttm_ = std::isfinite(f) ? f + after_tax_interest(tax_end) : std::nan("");
            }
        }
        // Balance sheet: net debt, minority interest, book value per share.
        const auto bal = fm::columns_for(last_financials_, fm::Statement::Balance, fm::Period::Ttm,
                                         fm::fiscal_year_end_month(last_financials_));
        if (!bal.isEmpty()) {
            const auto& b = bal.last().data;
            const double debt = fm::line(b, {"Total Debt"});
            const double cash = fm::line(b, {"Cash Cash Equivalents And Short Term Investments", "Cash And Cash Equivalents"});
            net_debt_ = (std::isfinite(debt) ? debt : 0.0) - (std::isfinite(cash) ? cash : 0.0);
            const double mi = fm::line(b, {"Minority Interest"});
            minority_ = std::isfinite(mi) ? mi : 0.0;
            const double eq = fm::line(b, {"Stockholders Equity", "Common Stock Equity"});
            const double sh = fm::line(b, {"Ordinary Shares Number", "Share Issued"});
            if (std::isfinite(eq) && sh > 0)
                bvps_ = eq / sh;
            if (std::isfinite(debt) && debt > 0 && !inc_annual.isEmpty()) {
                const double i = fm::line(inc_annual.last().data, {"Interest Expense", "Interest Expense Non Operating"});
                if (std::isfinite(i) && i != 0.0)
                    kd = std::clamp(std::fabs(i) / debt, 0.005, 0.25);
            }
        }
        if (!inc_annual.isEmpty()) {
            const auto& last_inc = inc_annual.last().data;
            const double t = fm::line(last_inc, {"Tax Rate For Calcs"});
            const double tp = fm::line(last_inc, {"Tax Provision"}), pt = fm::line(last_inc, {"Pretax Income"});
            tax = std::isfinite(t) ? t : (pt > 0 && std::isfinite(tp) ? tp / pt : std::nan(""));
            // Dividends per share and payout from the TTM cash flow and income.
            const auto inc_ttm = fm::columns_for(last_financials_, fm::Statement::Income, fm::Period::Ttm,
                                                 fm::fiscal_year_end_month(last_financials_));
            const QJsonObject& it = inc_ttm.isEmpty() ? last_inc : inc_ttm.last().data;
            const double ni = fm::line(it, {"Net Income Common Stockholders", "Net Income"});
            const double shares = fm::line(it, {"Diluted Average Shares", "Basic Average Shares"});
            const double div = cf_ttm.isEmpty() ? std::nan("")
                                                : fm::line(cf_ttm.last().data, {"Cash Dividends Paid", "Common Stock Dividend Paid"});
            if (std::isfinite(div) && shares > 0)
                dps_ = std::fabs(div) / shares;
            if (std::isfinite(div) && ni > 0)
                payout_ = std::clamp(std::fabs(div) / ni, 0.0, 1.0);
        }
        const auto rt = fm::ratios(last_financials_, fm::Period::Annual, tmpl);
        const int roe_row = fm::find_row(rt.rows, QStringLiteral("roe"));
        if (roe_row >= 0)
            for (int i = rt.columns.size() - 1; i >= 0; --i)
                if (std::isfinite(rt.rows[roe_row].values[i])) {
                    roe_ = rt.rows[roe_row].values[i];
                    break;
                }
        // Growth: revenue CAGR over the fiscal years on file, not one quarter's YoY.
        QVector<double> rev;
        QVector<QDate> rev_dates;
        for (const auto& c : inc_annual) {
            const double v = fm::line(c.data, {"Total Revenue", "Operating Revenue"});
            if (v > 0) {
                rev << v;
                rev_dates << c.end;
            }
        }
        if (rev.size() >= 2) {
            const double years = rev_dates.first().daysTo(rev_dates.last()) / 365.25;
            const double cagr = years > 0.5 ? std::pow(rev.last() / rev.first(), 1.0 / years) - 1.0 : std::nan("");
            if (std::isfinite(cagr)) {
                const double g = std::clamp(cagr, -0.05, 0.15);
                growth_input_->setValue(g * 100.0);
                prefill_notes_ << tr("growth = %1-year revenue CAGR %2, held within −5%…+15%")
                                      .arg(QString::number(years, 'f', 0), fmt::pct(cagr, 1));
            }
        }
    }
    if (!std::isfinite(base_ttm_) && s.free_cashflow != 0.0)
        base_ttm_ = s.free_cashflow; // Yahoo's levered FCF when the statements are missing
    if (prefill_notes_.isEmpty()) {
        const double g = s.revenue_growth != 0.0 ? s.revenue_growth : s.earnings_growth;
        if (g != 0.0) {
            growth_input_->setValue(std::clamp(g, -0.05, 0.15) * 100.0);
            prefill_notes_ << tr("growth = latest-quarter YoY growth %1 (no statement history), held within −5%…+15%")
                                  .arg(fmt::pct(g, 1));
        }
    }

    // ── Base-source choices (the first with a positive figure wins) ─────────
    {
        const QSignalBlocker block(base_source_);
        base_source_->clear();
        base_source_->addItem(tr("TTM"), QStringLiteral("ttm"));
        base_source_->addItem(tr("Latest fiscal year"), QStringLiteral("fy"));
        base_source_->addItem(tr("3-year average (normalised)"), QStringLiteral("avg3"));
        base_source_->addItem(tr("All years on file (normalised)"), QStringLiteral("avg5"));
        base_source_->addItem(tr("Custom (typed)"), QStringLiteral("custom"));
        int pick = 0;
        if (!(base_ttm_ > 0))
            pick = base_avg3_ > 0 ? 2 : (base_fy_ > 0 ? 1 : 3);
        base_source_->setCurrentIndex(pick);
    }
    if (s.shares_outstanding > 0.0)
        shares_input_->setValue(s.shares_outstanding / 1e6);
    beta_input_->setValue(s.beta > 0.0 ? s.beta : 1.0);
    if (!(s.beta > 0.0))
        prefill_notes_ << tr("no beta reported — 1.0 assumed");
    const double crp = country_risk_premium_pct(s.country);
    crp_input_->setValue(std::isfinite(crp) ? crp : 0.0);
    tax_input_->setValue(std::isfinite(tax) ? std::clamp(tax, 0.0, 0.45) * 100.0 : 25.0);
    // Risk-free: the reporting currency's 10-year government yield. Deliberate USD: ^TNX is the
    // US dollar rate (also used, labelled, while the reporting currency is still unknown).
    const QString rep = reporting_currency();
    if (rep.isEmpty() || rep == QLatin1String("USD")) {
        auto& hub = datahub::DataHub::instance();
        const qint64 hub_ms = hub.last_publish_ms(treasury_topic());
        if (hub_ms > live_rf_ms_) {
            const QVariant v = hub.peek_raw(treasury_topic());
            if (v.canConvert<services::QuoteData>())
                record_treasury_yield(v.value<services::QuoteData>().price, hub_ms);
        }
        if (live_rf_pct_ > 0.0) {
            rf_source_ = RfSource::Market;
            rf_fetched_ms_ = live_rf_ms_;
            rf_label_ = tr("US 10Y Treasury (^TNX), live");
            set_rf_value(live_rf_pct_);
        } else {
            rf_source_ = RfSource::Fallback;
            rf_label_ = tr("fallback until the US 10Y quote loads");
            set_rf_value(kRiskFreeFallback);
        }
    } else {
        rf_source_ = RfSource::Fallback;
        rf_label_ = tr("loading the %1 10-year government yield…").arg(rep);
        request_local_risk_free();
    }
    kd_input_->setValue(std::isfinite(kd) ? kd * 100.0 : rf_input_->value() + 2.0);

    // Banks and insurers: cash flow is not the right lens — value the equity directly.
    const bool financial = tmpl != fm::Template::Industrial;
    method_seg_->set_current(financial ? QStringLiteral("ri") : QStringLiteral("fcfe"));
    method_ = financial ? Method::ResidualIncome : Method::Fcfe;
    apply_method_visibility();
    apply_base_source();
    apply_discount();
    on_calculate_clicked();
}

void EquityValuationTab::apply_base_source() {
    if (!base_source_ || !fcf_input_)
        return;
    const QString k = base_source_->currentData().toString();
    if (k == QLatin1String("custom"))
        return;
    const bool ff = method_ == Method::Fcff;
    const double v = k == QLatin1String("fy")     ? (ff ? fcff_fy_ : base_fy_)
                     : k == QLatin1String("avg3") ? (ff ? fcff_avg3_ : base_avg3_)
                     : k == QLatin1String("avg5") ? (ff ? fcff_avg5_ : base_avg5_)
                                                  : (ff ? fcff_ttm_ : base_ttm_);
    const QSignalBlocker block(fcf_input_);
    fcf_input_->setValue(std::isfinite(v) ? v / 1e6 : 0.0);
}

void EquityValuationTab::apply_method_visibility() {
    const bool dcf = method_ == Method::Fcfe || method_ == Method::Fcff;
    for (auto* w : dcf_only_)
        w->setVisible(dcf);
    for (auto* w : fcff_only_)
        w->setVisible(method_ == Method::Fcff);
    if (discount_key_)
        discount_key_->setText(method_ == Method::Fcff ? tr("DISCOUNT RATE (WACC)") : tr("DISCOUNT RATE (COST OF EQUITY)"));
    if (base_key_)
        base_key_->setText(method_ == Method::Fcff ? tr("BASE FCF TO THE FIRM") : tr("BASE FREE CASH FLOW"));
    if (capm_btn_)
        capm_btn_->setText(method_ == Method::Fcff ? tr("RESET TO WACC") : tr("RESET TO CAPM"));
}

// ── Risk-free / CAPM / WACC ─────────────────────────────────────────────────

void EquityValuationTab::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (peers_auto_on_show_ && !current_symbol_.isEmpty() && !peers_pending_)
        request_peer_multiples();
    if (hub_active_)
        return;
    hub_active_ = true;
    datahub::DataHub::instance().subscribe(this, treasury_topic(), [this](const QVariant& v) {
        if (v.canConvert<services::QuoteData>())
            on_treasury_yield(v.value<services::QuoteData>().price,
                              datahub::DataHub::instance().last_publish_ms(treasury_topic()));
    });
}

void EquityValuationTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (!hub_active_)
        return;
    datahub::DataHub::instance().unsubscribe(this, treasury_topic());
    hub_active_ = false;
}

bool EquityValuationTab::record_treasury_yield(double yield_pct, qint64 fetched_ms) {
    if (!(yield_pct > 0.0 && yield_pct < kTreasuryYieldMaxPct))
        return false;
    live_rf_pct_ = yield_pct;
    live_rf_ms_ = fetched_ms;
    return true;
}

void EquityValuationTab::on_treasury_yield(double yield_pct, qint64 fetched_ms) {
    if (!record_treasury_yield(yield_pct, fetched_ms))
        return;
    // Only a USD reporter takes the US Treasury yield (a USD rate by definition).
    const QString rep = reporting_currency();
    if (!rep.isEmpty() && rep != QLatin1String("USD"))
        return;
    if (rf_source_ == RfSource::User ||
        (rf_source_ == RfSource::Market && fetched_ms - rf_fetched_ms_ < kRiskFreeRefreshMs))
        return;
    rf_source_ = RfSource::Market;
    rf_fetched_ms_ = fetched_ms;
    rf_label_ = tr("US 10Y Treasury (^TNX), live");
    set_rf_value(yield_pct);
    apply_discount();
    if (!current_symbol_.isEmpty() && prefilled_for_ == current_symbol_)
        on_calculate_clicked();
}

void EquityValuationTab::request_local_risk_free() {
    const QString ccy = reporting_currency();
    const QString sym = current_symbol_;
    QPointer<EquityValuationTab> self = this;
    services::equity::EquityResearchService::instance().fetch_risk_free(
        ccy, [self, sym, ccy](bool ok, services::equity::RiskFreeRate rate, QString error) {
            if (!self || self->current_symbol_ != sym || self->rf_source_ == RfSource::User)
                return;
            if (!ok) {
                self->rf_source_ = RfSource::Fallback;
                self->rf_label_ = tr("no %1 government yield available (%2) — type the local 10-year yield")
                                      .arg(ccy, error.left(80));
                self->set_rf_value(kRiskFreeFallback);
            } else {
                self->rf_source_ = rate.source == QLatin1String("reference") ? RfSource::Reference : RfSource::Local;
                self->rf_label_ = rate.source == QLatin1String("reference")
                                      ? tr("%1 — reference value, CHECK and type the current %2 10-year yield")
                                            .arg(rate.label, ccy)
                                      : tr("%1, as of %2").arg(rate.label, rate.as_of);
                self->set_rf_value(rate.rate_pct);
            }
            self->apply_discount();
            self->on_calculate_clicked();
        });
}

void EquityValuationTab::set_rf_value(double pct) {
    const QSignalBlocker block(rf_input_);
    rf_input_->setValue(pct);
    rf_shown_pct_ = rf_input_->value();
}

double EquityValuationTab::cost_of_equity_pct() const {
    // CAPM with a country premium: ke = rf + max(β, 0.3) × ERP + CRP. The β floor keeps a
    // near-zero beta from discounting at about the risk-free rate (the old fixed 6 % floor
    // was a US number; this one scales with the local yield).
    const double beta = std::max(beta_input_->value(), 0.3);
    return rf_input_->value() + beta * erp_input_->value() + crp_input_->value();
}

double EquityValuationTab::wacc_pct() const {
    const double ke = cost_of_equity_pct();
    const double e = market_cap_in_reporting();
    double d = net_debt_ + 0.0;
    // Gross debt weights the WACC; net debt is the bridge.
    namespace fm = fin_model;
    if (financials_loaded_) {
        const auto bal = fm::columns_for(last_financials_, fm::Statement::Balance, fm::Period::Ttm,
                                         fm::fiscal_year_end_month(last_financials_));
        if (!bal.isEmpty()) {
            const double debt = fm::line(bal.last().data, {"Total Debt"});
            if (std::isfinite(debt))
                d = debt;
        }
    }
    if (!(e > 0) || !(d > 0))
        return ke;
    const double kd_after = kd_input_->value() * (1.0 - tax_input_->value() / 100.0);
    return (e * ke + d * kd_after) / (e + d);
}

void EquityValuationTab::apply_discount() {
    if (discount_overridden_ || !discount_input_)
        return;
    const double r = method_ == Method::Fcff ? wacc_pct() : cost_of_equity_pct();
    const QSignalBlocker block(discount_input_);
    discount_input_->setValue(std::round(r * 100.0) / 100.0);
}

void EquityValuationTab::update_assumptions_note() {
    if (!assumptions_lbl_ || current_symbol_.isEmpty() || prefilled_for_ != current_symbol_)
        return;
    QString source;
    switch (rf_source_) {
        case RfSource::User:
            source = tr("your input");
            break;
        default:
            source = rf_label_;
            break;
    }
    if (rf_source_lbl_) {
        rf_source_lbl_->setText(source);
        rf_source_lbl_->setProperty("tone", rf_source_ == RfSource::Reference || rf_source_ == RfSource::Fallback
                                                ? QStringLiteral("warn")
                                                : QString());
        val_repolish(rf_source_lbl_);
    }
    QStringList notes = prefill_notes_;
    notes << tr("cost of equity = %1 risk-free + β %2 × %3 premium + %4 country premium = %5")
                 .arg(val_uf::pct(rf_input_->value(), 2, false), val_uf::number(beta_input_->value(), 2),
                      val_uf::pct(erp_input_->value(), 1, false), val_uf::pct(crp_input_->value(), 1, false),
                      val_uf::pct(cost_of_equity_pct(), 2, false));
    if (method_ == Method::Fcff)
        notes << tr("WACC %1 (market-cap and debt weights, cost of debt %2 pre-tax, tax %3)")
                     .arg(val_uf::pct(wacc_pct(), 2, false), val_uf::pct(kd_input_->value(), 2, false),
                          val_uf::pct(tax_input_->value(), 0, false));
    if (discount_overridden_)
        notes << tr("discount overridden to %1").arg(val_uf::pct(discount_input_->value(), 2, false));
    if (!reporting_currency().isEmpty())
        notes << tr("all model amounts in %1 (the reporting currency)").arg(reporting_currency());
    assumptions_lbl_->setText(tr("Assumptions: %1.").arg(notes.join(QStringLiteral(" · "))));
}

// ═════════════════════════════════════════════════════════════════════════════
// Scores
// ═════════════════════════════════════════════════════════════════════════════

void EquityValuationTab::update_statements_note() {
    if (!scoring_note_)
        return;
    if (financials_failed_)
        scoring_note_->setText(tr("Annual statements could not be loaded for %1 — press RETRY above, or check the "
                                  "symbol.")
                                   .arg(current_symbol_));
    else if (!financials_loaded_)
        scoring_note_->setText(tr("Loading annual statements…"));
}

void EquityValuationTab::run_scoring_models() {
    namespace v = services::equity::valuation;
    for (QLabel* b : {altman_badge_, altman2_badge_, piotroski_badge_, beneish_badge_})
        b->setVisible(false);
    for (QLabel* l : {altman_val_, altman2_val_, piotroski_val_, beneish_val_}) {
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

    {
        const auto a2 = v::altman_z(cur, std::nan(""));
        if (!a2.z2_ok) {
            set_badge(altman2_badge_, tr("NO DATA — %1").arg(a2.missing), "none");
        } else {
            altman2_val_->setText(val_uf::number(a2.z2, 2));
            altman2_val_->setToolTip(tr("6.56·X1 working capital/assets %1 + 3.26·X2 retained earnings/assets %2 + "
                                        "6.72·X3 EBIT/assets %3 + 1.05·X4 book equity/liabilities %4")
                                         .arg(val_uf::number(a2.x1, 3), val_uf::number(a2.x2, 3),
                                              val_uf::number(a2.x3, 3), val_uf::number(a2.x4_book, 3)));
            if (a2.z2 > v::kAltmanZ2Safe)
                set_badge(altman2_badge_, tr("SAFE ZONE (> 2.60)"), "good");
            else if (a2.z2 >= v::kAltmanZ2Distress)
                set_badge(altman2_badge_, tr("GREY ZONE (1.10–2.60)"), "warn");
            else
                set_badge(altman2_badge_, tr("DISTRESS ZONE (< 1.10)"), "bad");
        }
        static const QStringList kManufacturing = {"Industrials", "Basic Materials", "Energy", "Consumer Cyclical"};
        const bool manufacturer = kManufacturing.contains(last_info_.sector, Qt::CaseInsensitive);
        if (!last_info_.sector.isEmpty())
            notes << (manufacturer ? tr("%1: the original Altman Z applies.").arg(last_info_.sector)
                                   : tr("%1 is not a manufacturing sector — read Altman Z″, not Z.").arg(last_info_.sector));
    }

    // Altman Z: the market cap converted to the statements' currency (FX when they differ).
    const double mcap = market_cap_in_reporting();
    if (!std::isfinite(mcap)) {
        altman_val_->setText(fmt::dash());
        set_badge(altman_badge_, tr("NO FX RATE / MARKET CAP"), "none");
        altman_val_->setToolTip(tr("Market cap (%1) could not be converted to the statements' currency (%2) yet.")
                                    .arg(last_info_.currency, reporting_currency()));
    } else {
        const auto a = v::altman_z(cur, mcap);
        if (!a.ok) {
            set_badge(altman_badge_, tr("NO DATA — %1").arg(a.missing), "none");
        } else {
            altman_val_->setText(val_uf::number(a.z, 2));
            altman_val_->setToolTip(tr("X1 working capital/assets %1 · X2 retained earnings/assets %2 · X3 EBIT/assets "
                                       "%3 · X4 market cap/liabilities %4 · X5 sales/assets %5")
                                        .arg(val_uf::number(a.x1, 3), val_uf::number(a.x2, 3), val_uf::number(a.x3, 3),
                                             val_uf::number(a.x4, 3), val_uf::number(a.x5, 3)));
            if (a.z > 2.99)
                set_badge(altman_badge_, tr("SAFE ZONE (> 2.99)"), "good");
            else if (a.z >= 1.81)
                set_badge(altman_badge_, tr("GREY ZONE (1.81–2.99)"), "warn");
            else
                set_badge(altman_badge_, tr("DISTRESS ZONE (< 1.81)"), "bad");
        }
    }

    if (years.size() < 2) {
        set_badge(piotroski_badge_, tr("NO DATA — one fiscal year on file"), "none");
        set_badge(beneish_badge_, tr("NO DATA — one fiscal year on file"), "none");
    } else {
        const v::FiscalYear& prev = years.at(1);
        const auto p = v::piotroski_f(cur, prev);
        QStringList lines;
        for (const auto& t : p.tests)
            lines << QStringLiteral("%1  %2").arg(t.second == 1   ? QStringLiteral("✓")
                                                  : t.second == 0 ? QStringLiteral("✗")
                                                                  : QStringLiteral("·"),
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
            comps << QStringLiteral("%1 %2").arg(c.first, val_uf::number(c.second, 3));
        if (!b.components.isEmpty())
            beneish_val_->setToolTip(comps.join(QStringLiteral(" · ")) +
                                     (b.imputed > 0 ? tr("\n%1 index(es) set to the neutral 1.0 — statement line not "
                                                         "reported.")
                                                          .arg(b.imputed)
                                                    : QString()));
        if (!b.ok) {
            set_badge(beneish_badge_, tr("NO DATA — %1").arg(b.missing), "none");
        } else {
            beneish_val_->setText(val_uf::number(b.m, 2));
            if (b.m > services::equity::valuation::kBeneishThreshold)
                set_badge(beneish_badge_, tr("MANIPULATION RISK (> −1.78)"), "bad");
            else
                set_badge(beneish_badge_, tr("UNLIKELY MANIPULATOR (≤ −1.78)"), "good");
        }
    }
    scoring_note_->setText(notes.join(QLatin1Char(' ')));

    // ── Trend: every fiscal year on file (oldest → newest) ───────────────────
    const int ny = static_cast<int>(years.size());
    score_history_->clear();
    score_history_->setColumnCount(4);
    score_history_->setHorizontalHeaderLabels({tr("FISCAL YEAR"), tr("ALTMAN Z″"), tr("PIOTROSKI F"), tr("BENEISH M")});
    score_history_->setRowCount(ny);
    auto cell = [](const QString& text, const QString& color = {}) {
        auto* it = new QTableWidgetItem(text);
        it->setTextAlignment(Qt::AlignCenter);
        if (!color.isEmpty())
            it->setForeground(QColor(color));
        return it;
    };
    QStringList year_labels;
    QVector<v::PiotroskiResult> pf(ny);
    for (int i = 0; i < ny; ++i) {
        const int k = ny - 1 - i; // years are newest first
        const v::FiscalYear& fy = years[k];
        year_labels << fy.period.left(7);
        score_history_->setItem(i, 0, cell(fy.period));
        const auto a2 = v::altman_z(fy, std::nan(""));
        score_history_->setItem(i, 1, a2.z2_ok ? cell(val_uf::number(a2.z2, 2), a2.z2 > v::kAltmanZ2Safe ? ui::colors::SUCCESS()
                                                                                : a2.z2 >= v::kAltmanZ2Distress ? ui::colors::WARNING()
                                                                                                                : ui::colors::DANGER())
                                               : cell(fmt::dash()));
        if (k + 1 < ny) {
            pf[i] = v::piotroski_f(fy, years[k + 1]);
            const auto b = v::beneish_m(fy, years[k + 1]);
            score_history_->setItem(i, 2, pf[i].ok ? cell(QStringLiteral("%1 / %2").arg(pf[i].score).arg(pf[i].tests_available),
                                                          pf[i].score >= 8 ? ui::colors::SUCCESS()
                                                          : pf[i].score >= 5 ? ui::colors::WARNING()
                                                                             : ui::colors::DANGER())
                                                   : cell(fmt::dash()));
            score_history_->setItem(i, 3, b.ok ? cell(val_uf::number(b.m, 2), b.m > v::kBeneishThreshold ? ui::colors::DANGER()
                                                                                                        : ui::colors::SUCCESS())
                                               : cell(fmt::dash()));
        } else {
            score_history_->setItem(i, 2, cell(fmt::dash()));
            score_history_->setItem(i, 3, cell(fmt::dash()));
        }
    }
    // Piotroski grid: tests × years, pass ✓ (success) / fail ✗ (danger) / not computable ·.
    piotroski_grid_->clear();
    QStringList grid_cols{tr("PIOTROSKI TEST")};
    QVector<int> cols_with_tests;
    for (int i = 0; i < ny; ++i)
        if (!pf[i].tests.isEmpty()) {
            grid_cols << year_labels[i];
            cols_with_tests << i;
        }
    piotroski_grid_->setColumnCount(static_cast<int>(grid_cols.size()));
    piotroski_grid_->setHorizontalHeaderLabels(grid_cols);
    const int ntests = cols_with_tests.isEmpty() ? 0 : static_cast<int>(pf[cols_with_tests.last()].tests.size());
    piotroski_grid_->setRowCount(ntests);
    for (int t = 0; t < ntests; ++t) {
        auto* name = new QTableWidgetItem(pf[cols_with_tests.last()].tests[t].first);
        piotroski_grid_->setItem(t, 0, name);
        for (int c = 0; c < cols_with_tests.size(); ++c) {
            const auto& tests = pf[cols_with_tests[c]].tests;
            const int r = t < tests.size() ? tests[t].second : -1;
            piotroski_grid_->setItem(t, c + 1, cell(r == 1 ? QStringLiteral("✓") : r == 0 ? QStringLiteral("✗") : QStringLiteral("·"),
                                                    r == 1 ? ui::colors::SUCCESS() : r == 0 ? ui::colors::DANGER()
                                                                                            : ui::colors::TEXT_TERTIARY()));
        }
    }
    piotroski_grid_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
}

// ═════════════════════════════════════════════════════════════════════════════
// Multiples
// ═════════════════════════════════════════════════════════════════════════════

void EquityValuationTab::run_multiples() {
    const auto& s = last_info_;
    // EV / EBITDA and EV / revenue come from the info payload, which converts the statement
    // figures to the quote currency when the two differ (yfinance_data.py).
    ev_ebitda_val_->setText(fmt::multiple(s.ev_to_ebitda, 1));
    ev_revenue_val_->setText(fmt::multiple(s.ev_to_revenue, 2));
    const double mcap = market_cap_in_reporting();
    price_sales_val_->setText(std::isfinite(mcap) && s.total_revenue > 0 ? fmt::multiple(mcap / s.total_revenue, 2)
                                                                        : fmt::dash());
    fcf_yield_val_->setText(std::isfinite(mcap) && mcap > 0 && s.free_cashflow != 0.0
                                ? fmt::pct(s.free_cashflow / mcap, 2)
                                : fmt::dash());
    // Graham number in the REPORTING currency: EPS = price (converted) ÷ trailing P/E;
    // book value per share from the statements (Yahoo's bookValue otherwise).
    const double px_rep = price_in_reporting();
    const double eps = (s.pe_ratio > 0 && std::isfinite(px_rep)) ? px_rep / s.pe_ratio : std::nan("");
    const double bvps = std::isfinite(bvps_) && bvps_ > 0 ? bvps_ : s.book_value;
    const double graham = (eps > 0 && bvps > 0) ? std::sqrt(22.5 * eps * bvps) : std::nan("");
    if (std::isfinite(graham)) {
        const double vs = std::isfinite(px_rep) && px_rep > 0 ? graham / px_rep - 1.0 : std::nan("");
        graham_val_->setText(std::isfinite(vs) ? tr("%1  (%2 vs price)").arg(fmt_rep(graham), fmt::pct(vs, 0, true))
                                               : fmt_rep(graham));
    } else {
        graham_val_->setText(fmt::dash());
    }
    const auto listing = equity_fmt::listing_of(s);
    QString note;
    if (!s.symbol.isEmpty()) {
        note = tr("Trailing-12-month figures from Yahoo Finance; FCF yield = levered FCF ÷ market cap.");
        if (!listing.same_currency())
            note += tr(" Market cap converted from %1 to %2 at the spot rate.").arg(listing.quote_major(), listing.reporting_major());
        else if (listing.minor_quote())
            note += tr(" The price is quoted in %1 and taken in %2 for every comparison.").arg(listing.quote, listing.quote_major());
    }
    multiples_note_->setText(note);
}

// ═════════════════════════════════════════════════════════════════════════════
// Helpers
// ═════════════════════════════════════════════════════════════════════════════

void EquityValuationTab::clear_results() {
    for (QLabel* l : {intrinsic_val_, intrinsic_quote_val_, current_price_val_, margin_val_, bridge_val_,
                      terminal_share_val_, altman_val_, altman2_val_, piotroski_val_, beneish_val_, ev_ebitda_val_,
                      ev_revenue_val_, price_sales_val_, fcf_yield_val_, graham_val_}) {
        if (l) {
            l->setText(fmt::dash());
            l->setToolTip(QString());
        }
    }
    if (implied_growth_val_)
        implied_growth_val_->setText(fmt::dash());
    for (auto* l : scenario_lbl_)
        if (l)
            l->clear();
    if (sens_table_)
        sens_table_->clearContents();
    if (proj_table_)
        proj_table_->setRowCount(0);
    if (proj_chart_)
        proj_chart_->clear();
    reset_hist_chart();
    if (hist_table_)
        hist_table_->setRowCount(0);
    if (score_history_)
        score_history_->setRowCount(0);
    if (piotroski_grid_)
        piotroski_grid_->setRowCount(0);
    for (QLabel* b : {verdict_badge_, altman_badge_, altman2_badge_, piotroski_badge_, beneish_badge_})
        if (b)
            b->setVisible(false);
    if (dcf_note_)
        dcf_note_->setVisible(false);
    if (scoring_note_)
        scoring_note_->clear();
    if (multiples_note_)
        multiples_note_->clear();
    if (hist_note_)
        hist_note_->clear();
}

void EquityValuationTab::set_badge(QLabel* badge, const QString& text, const char* tone) {
    if (!badge)
        return;
    badge->setText(text);
    badge->setProperty("tone", QString::fromLatin1(tone));
    val_repolish(badge);
    badge->setVisible(true);
}

QString EquityValuationTab::fmt_rep(double value) const {
    return fmt::price(value, reporting_currency());
}

QString EquityValuationTab::fmt_quote(double value) const {
    return fmt::price(value, quote_currency());
}

QString EquityValuationTab::fmt_amount(double value) const {
    return fmt::money(value, reporting_currency());
}

// ═════════════════════════════════════════════════════════════════════════════
// i18n
// ═════════════════════════════════════════════════════════════════════════════

void EquityValuationTab::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        for (auto it = i18n_labels_.begin(); it != i18n_labels_.end(); ++it)
            it.key()->setText(tr(it.value()));
        method_seg_->set_text(QStringLiteral("fcfe"), tr("FCFE · COST OF EQUITY"),
                              tr("Levered free cash flow discounted at the cost of equity → equity value"));
        method_seg_->set_text(QStringLiteral("fcff"), tr("FCFF · WACC"),
                              tr("Free cash flow to the firm at the WACC → enterprise value − net debt"));
        method_seg_->set_text(QStringLiteral("ddm"), tr("DIVIDEND DISCOUNT"),
                              tr("Two-stage dividend discount model — for steady dividend payers and banks"));
        method_seg_->set_text(QStringLiteral("ri"), tr("RESIDUAL INCOME"),
                              tr("Book value + PV of returns above the cost of equity — for banks and insurers"));
        if (calc_btn_)
            calc_btn_->setText(tr("CALCULATE"));
        if (fade_check_)
            fade_check_->setText(tr("Fade growth to terminal rate"));
        if (peers_btn_) {
            peers_btn_->setText(tr("REFRESH PEER MEDIANS"));
            peers_btn_->setToolTip(tr("Peer medians load with the symbol; this pulls them again"));
        }
        if (sens_cols_) {
            const QSignalBlocker block(sens_cols_);
            const int idx = std::max(0, sens_cols_->currentIndex());
            sens_cols_->clear();
            sens_cols_->addItem(tr("columns: terminal growth"));
            sens_cols_->addItem(tr("columns: stage-1 growth"));
            sens_cols_->setCurrentIndex(idx);
        }
        if (sens_step_rows_)
            sens_step_rows_->setToolTip(tr("Discount-rate step between rows"));
        if (sens_step_cols_)
            sens_step_cols_->setToolTip(tr("Growth step between columns"));
        apply_method_visibility();
        if (!current_symbol_.isEmpty()) {
            run_multiples();
            if (financials_loaded_)
                run_scoring_models();
            on_calculate_clicked();
        }
    }
    QWidget::changeEvent(e);
}

} // namespace fincept::screens
