// AltRealEstatePanel.cpp — see AltRealEstatePanel.h.
#include "screens/alt_investments/AltRealEstatePanel.h"

#include "core/instrument/Instrument.h"
#include "screens/alt_investments/AltPanels.h"
#include "services/alt_investments/AltInvestmentsService.h"
#include "services/instruments/InstrumentResolver.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/ChartExport.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/DataGridEditor.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/ResultLayout.h"
#include "ui/widgets/SensitivityTable.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/report/AddToReportButton.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QDoubleSpinBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::alt {

using fincept::services::alt::AltCall;
using fincept::services::alt::AltInvestmentsService;
using fincept::services::alt::AltReply;

namespace {

constexpr int kAltReMaxReits = 10;

struct AltReDefault {
    const char* key;
    double value;
};

// Example inputs (a stabilised multi-tenant property); the user's own values replace them and are
// remembered. Rates in percent as typed.
constexpr AltReDefault kAltReDefaults[] = {
    {"price", 10000000}, {"closing_costs", 2},  {"gross_rent", 900000}, {"other_income", 30000},
    {"rent_growth", 3},  {"vacancy", 5},        {"opex", 280000},       {"opex_growth", 3},
    {"mgmt_fee", 3},     {"capex_reserve", 40000}, {"ltv", 60},         {"interest_rate", 6.5},
    {"amort_years", 30}, {"io_years", 2},       {"loan_fees", 1},       {"hold_years", 10},
    {"exit_cap", 6.5},   {"selling_costs", 2},  {"cap_step_bp", 25},    {"growth_step", 1},
};

double alt_re_default(const char* key) {
    for (const auto& d : kAltReDefaults)
        if (qstrcmp(d.key, key) == 0)
            return d.value;
    return 0.0;
}

void alt_re_set(ui::KpiStrip* strip, const char* key, const QString& value, const QString& sub = QString()) {
    if (ui::StatTile* t = strip->tile(QString::fromLatin1(key))) {
        t->set_value(value);
        t->set_sub_label(sub);
    }
}

QWidget* alt_re_group(QWidget* parent, QVBoxLayout* into, const QString& title) {
    auto* g = new QGroupBox(title, parent);
    g->setObjectName(QStringLiteral("altWbGroup"));
    auto* fl = new QFormLayout(g);
    fl->setContentsMargins(8, 12, 8, 8);
    fl->setHorizontalSpacing(8);
    fl->setVerticalSpacing(4);
    fl->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    into->addWidget(g);
    return g;
}

} // namespace

QWidget* make_real_estate_panel(QWidget* parent) {
    return new AltRealEstatePanel(parent);
}

AltRealEstatePanel::AltRealEstatePanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("altWbRoot"));
    setStyleSheet(alt_wb_style()); // before any child exists
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    tabs_ = new QTabWidget(this);
    tabs_->setObjectName(QStringLiteral("altWbTabs"));
    tabs_->setUsesScrollButtons(true);
    tabs_->setElideMode(Qt::ElideNone);
    tabs_->addTab(build_proforma_page(tabs_), tr("PRO FORMA"));
    tabs_->addTab(build_reit_page(tabs_), tr("REIT COMPARISON"));
    root->addWidget(tabs_, 1);
    // A number-format change re-renders the shown results from their payloads (nothing is fetched).
    ui::formatting::on_format_changed(this, [this]() {
        if (pf_pane_->has_result())
            render_proforma(pf_pane_->payload());
        if (reit_pane_->has_result())
            render_reits(reit_pane_->payload());
    });
}

AltRealEstatePanel::~AltRealEstatePanel() {
    for (auto it = service_ids_.cbegin(); it != service_ids_.cend(); ++it)
        AltInvestmentsService::instance().cancel(it.value());
    for (ui::AutoLoadPane* p : {pf_pane_, reit_pane_})
        if (p)
            p->set_cancel({});
}

void AltRealEstatePanel::cancel_service(quint64 pane_request) {
    const auto it = service_ids_.find(pane_request);
    if (it == service_ids_.end())
        return;
    AltInvestmentsService::instance().cancel(it.value());
    service_ids_.erase(it);
}

// ── Pro forma ────────────────────────────────────────────────────────────────

QDoubleSpinBox* AltRealEstatePanel::add_money(QWidget* group, const QString& key, const QString& label, double value) {
    auto* s = new QDoubleSpinBox(group);
    s->setRange(0, 1e13);
    s->setDecimals(0);
    s->setValue(value);
    s->setGroupSeparatorShown(true);
    s->setKeyboardTracking(false);
    // No property currency field: amounts are in the display currency, shown on the box itself.
    ui::formatting::bind_currency_affix(s);
    static_cast<QFormLayout*>(group->layout())->addRow(alt_wb_label(label, "altWbFormLabel", group), s);
    money_.insert(key, s);
    return s;
}

QDoubleSpinBox* AltRealEstatePanel::add_pct(QWidget* group, const QString& key, const QString& label, double value,
                                            double lo, double hi, int decimals) {
    auto* s = new QDoubleSpinBox(group);
    s->setRange(lo, hi);
    s->setDecimals(decimals);
    s->setValue(value);
    s->setSuffix(QStringLiteral("%"));
    s->setKeyboardTracking(false);
    static_cast<QFormLayout*>(group->layout())->addRow(alt_wb_label(label, "altWbFormLabel", group), s);
    pct_.insert(key, s);
    return s;
}

QSpinBox* AltRealEstatePanel::add_int(QWidget* group, const QString& key, const QString& label, int value, int lo,
                                      int hi, const QString& suffix) {
    auto* s = new QSpinBox(group);
    s->setRange(lo, hi);
    s->setValue(value);
    if (!suffix.isEmpty())
        s->setSuffix(suffix);
    s->setKeyboardTracking(false);
    static_cast<QFormLayout*>(group->layout())->addRow(alt_wb_label(label, "altWbFormLabel", group), s);
    ints_.insert(key, s);
    return s;
}

QWidget* AltRealEstatePanel::build_proforma_page(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* pl = new QHBoxLayout(page);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(0);
    auto* split = new QSplitter(Qt::Horizontal, page);
    split->setObjectName(QStringLiteral("altWbSplit"));
    split->setHandleWidth(1);
    split->setChildrenCollapsible(false);
    pl->addWidget(split);

    // ── Inputs (scrolls) ──
    auto* scroll = new QScrollArea(split);
    scroll->setObjectName(QStringLiteral("altWbScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* form = new QWidget(scroll);
    form->setObjectName(QStringLiteral("altWbScrollBody"));
    auto* fl = new QVBoxLayout(form);
    fl->setContentsMargins(10, 6, 10, 10);
    fl->setSpacing(4);

    auto* head = new QWidget(form);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);
    hl->addWidget(alt_wb_label(tr("PROPERTY INPUTS"), "altWbSection", head), 1);
    auto* reset = alt_wb_button(tr("RESET TO EXAMPLE"), false, head);
    reset->setToolTip(tr("Put the example property's inputs back"));
    auto* run = alt_wb_button(tr("RUN"), true, head);
    hl->addWidget(reset);
    hl->addWidget(run);
    fl->addWidget(head);

    QWidget* g = alt_re_group(form, fl, tr("PURCHASE"));
    add_money(g, QStringLiteral("price"), tr("Purchase price"), alt_re_default("price"));
    add_pct(g, QStringLiteral("closing_costs"), tr("Closing costs"), alt_re_default("closing_costs"), 0, 20);
    g = alt_re_group(form, fl, tr("INCOME (YEAR 1, ANNUAL)"));
    add_money(g, QStringLiteral("gross_rent"), tr("Gross potential rent"), alt_re_default("gross_rent"));
    add_money(g, QStringLiteral("other_income"), tr("Other income"), alt_re_default("other_income"));
    add_pct(g, QStringLiteral("rent_growth"), tr("Rent growth / yr"), alt_re_default("rent_growth"), -20, 30);
    add_pct(g, QStringLiteral("vacancy"), tr("Vacancy and credit loss"), alt_re_default("vacancy"), 0, 95);
    g = alt_re_group(form, fl, tr("EXPENSES (YEAR 1, ANNUAL)"));
    add_money(g, QStringLiteral("opex"), tr("Operating expenses"), alt_re_default("opex"));
    add_pct(g, QStringLiteral("opex_growth"), tr("Expense growth / yr"), alt_re_default("opex_growth"), -20, 30);
    add_pct(g, QStringLiteral("mgmt_fee"), tr("Management fee (of EGI)"), alt_re_default("mgmt_fee"), 0, 30);
    add_money(g, QStringLiteral("capex_reserve"), tr("Capex reserve"), alt_re_default("capex_reserve"));
    g = alt_re_group(form, fl, tr("FINANCING"));
    add_pct(g, QStringLiteral("ltv"), tr("Loan to value"), alt_re_default("ltv"), 0, 95, 1);
    add_pct(g, QStringLiteral("interest_rate"), tr("Interest rate"), alt_re_default("interest_rate"), 0, 30, 3);
    add_int(g, QStringLiteral("amort_years"), tr("Amortisation (0 = interest only)"), int(alt_re_default("amort_years")),
            0, 40, tr(" yrs"));
    add_int(g, QStringLiteral("io_years"), tr("Interest-only years"), int(alt_re_default("io_years")), 0, 30, tr(" yrs"));
    add_pct(g, QStringLiteral("loan_fees"), tr("Loan fees"), alt_re_default("loan_fees"), 0, 10);
    g = alt_re_group(form, fl, tr("EXIT"));
    add_int(g, QStringLiteral("hold_years"), tr("Hold period"), int(alt_re_default("hold_years")), 1, 30, tr(" yrs"));
    add_pct(g, QStringLiteral("exit_cap"), tr("Exit cap rate"), alt_re_default("exit_cap"), 0.5, 25, 2);
    add_pct(g, QStringLiteral("selling_costs"), tr("Selling costs"), alt_re_default("selling_costs"), 0, 20);
    g = alt_re_group(form, fl, tr("SENSITIVITY STEPS"));
    add_int(g, QStringLiteral("cap_step_bp"), tr("Exit cap step"), int(alt_re_default("cap_step_bp")), 5, 200, tr(" bp"));
    add_pct(g, QStringLiteral("growth_step"), tr("Rent growth step"), alt_re_default("growth_step"), 0.1, 10, 2);
    fl->addStretch(1);
    scroll->setWidget(form);
    const int cw = QFontMetrics(ui::fonts::font(-1)).averageCharWidth();
    scroll->setMinimumWidth(std::max(320, cw * 46));
    scroll->setMaximumWidth(std::max(460, cw * 64));

    pf_memory_ = new ui::InputMemory(QStringLiteral("alt_investments"), QStringLiteral("re_proforma"), form);
    for (auto it = money_.cbegin(); it != money_.cend(); ++it)
        pf_memory_->track(it.key(), it.value());
    for (auto it = pct_.cbegin(); it != pct_.cend(); ++it)
        pf_memory_->track(it.key(), it.value());
    for (auto it = ints_.cbegin(); it != ints_.cend(); ++it)
        pf_memory_->track(it.key(), it.value());
    pf_memory_->restore();

    // ── Result ──
    auto* right = new QWidget(split);
    auto* rl = new QVBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);
    pf_actions_ = alt_wb_make_actions(right, tr("PRO FORMA"));
    rl->addWidget(pf_actions_.row);
    pf_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, right);
    rl->addWidget(pf_pane_, 1);
    auto* rscroll = new QScrollArea(pf_pane_->content_parent());
    rscroll->setObjectName(QStringLiteral("altWbScroll"));
    rscroll->setWidgetResizable(true);
    rscroll->setFrameShape(QFrame::NoFrame);
    rscroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget(rscroll);
    body->setObjectName(QStringLiteral("altWbScrollBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(10, 8, 10, 10);
    bl->setSpacing(8);
    pf_kpis_ = new ui::KpiStrip(body);
    for (const auto& t : QList<QPair<const char*, QString>>{{"lirr", tr("Levered IRR")},
                                                            {"uirr", tr("Unlevered IRR")},
                                                            {"em", tr("Equity multiple")},
                                                            {"cap", tr("Going-in cap")},
                                                            {"coc", tr("Cash-on-cash Y1")},
                                                            {"dscr", tr("DSCR (min)")},
                                                            {"ltv", tr("LTV")},
                                                            {"dy", tr("Debt yield")}})
        pf_kpis_->add_tile(QString::fromLatin1(t.first), t.second);
    bl->addWidget(pf_kpis_);
    pf_notes_ = alt_wb_label(QString(), "altWbNote", body);
    pf_notes_->setWordWrap(true);
    bl->addWidget(pf_notes_);
    bl->addWidget(alt_wb_label(tr("CASH FLOW TO EQUITY BY YEAR (EXIT PROCEEDS IN THE LAST YEAR)"), "altWbSection", body));
    pf_chart_ = new ui::BarChart(body);
    pf_chart_->set_mode(ui::BarChart::Mode::Stacked);
    pf_chart_->set_show_legend(true);
    pf_chart_->setMinimumHeight(220);
    bl->addWidget(pf_chart_);
    pf_sens_ = new ui::SensitivityTable(body);
    pf_sens_->set_export_name(QStringLiteral("property_irr_sensitivity"));
    bl->addWidget(pf_sens_);
    bl->addWidget(alt_wb_label(tr("YEARLY PRO FORMA"), "altWbSection", body));
    pf_model_ = new AltWbRowsModel(this);
    pf_table_ = alt_wb_make_table(body, pf_model_, QStringLiteral("property_proforma"));
    bl->addWidget(pf_table_);
    bl->addStretch(1);
    rscroll->setWidget(body);
    pf_pane_->set_content(rscroll);

    split->addWidget(scroll);
    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({std::max(340, cw * 50), 4000});

    pf_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Result);
    pf_pane_->set_loading_text(tr("Building the pro forma…"));
    pf_pane_->set_cache_key(QStringLiteral("alt_investments/re_proforma"));
    pf_pane_->set_max_age(ui::autoload::kNeverRefresh);
    pf_pane_->set_status_label(pf_actions_.status);
    pf_pane_->set_auto_delay(150);
    pf_pane_->set_timeout(60);
    pf_pane_->set_start([this](quint64 id) { start_proforma(id); });
    pf_pane_->set_cancel([this](quint64 id) { cancel_service(id); });
    pf_pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render_proforma(p); });
    pf_pane_->set_view(QStringLiteral("proforma"));
    pf_pane_->set_signature(pf_memory_->signature());
    connect(pf_memory_, &ui::InputMemory::changed, this, [this]() { pf_pane_->set_signature(pf_memory_->signature()); });
    connect(run, &QPushButton::clicked, pf_pane_, &ui::AutoLoadPane::run);
    connect(reset, &QPushButton::clicked, this, &AltRealEstatePanel::reset_example);

    connect(pf_actions_.copy, &QPushButton::clicked, this, [this]() {
        alt_wb_copy(pf_export_);
    });
    connect(pf_actions_.csv, &QPushButton::clicked, this,
            [this]() { alt_wb_save_csv(pf_export_, QStringLiteral("property_proforma"), this); });
    connect(pf_actions_.image, &QPushButton::clicked, this,
            [this]() { ui::ChartExport::save_png(pf_pane_->content(), QStringLiteral("property_proforma")); });
    pf_actions_.report->set_payload_provider([this]() {
        report::ReportPayload p = alt_wb_payload(pf_export_, {{tr("Cash flow to equity"), pf_chart_}});
        if (!pf_sens_->is_empty()) {
            report::PayloadTable t;
            t.title = tr("Levered IRR: exit cap x rent growth");
            t.columns = pf_sens_->headers();
            t.rows = pf_sens_->rows();
            t.note = tr("Rows: exit cap rate; columns: rent growth; the middle cell is the base case.");
            p.tables.prepend(t);
        }
        return p;
    });
    return page;
}

void AltRealEstatePanel::reset_example() {
    for (auto it = money_.cbegin(); it != money_.cend(); ++it)
        it.value()->setValue(alt_re_default(it.key().toLatin1().constData()));
    for (auto it = pct_.cbegin(); it != pct_.cend(); ++it)
        it.value()->setValue(alt_re_default(it.key().toLatin1().constData()));
    for (auto it = ints_.cbegin(); it != ints_.cend(); ++it)
        it.value()->setValue(int(alt_re_default(it.key().toLatin1().constData())));
    pf_pane_->run();
}

QJsonObject AltRealEstatePanel::proforma_input() const {
    QJsonObject in;
    for (auto it = money_.cbegin(); it != money_.cend(); ++it)
        in.insert(it.key(), it.value()->value());
    for (auto it = pct_.cbegin(); it != pct_.cend(); ++it)
        if (it.key() != QLatin1String("growth_step"))
            in.insert(it.key(), it.value()->value() / 100.0);
    for (auto it = ints_.cbegin(); it != ints_.cend(); ++it)
        if (it.key() != QLatin1String("cap_step_bp"))
            in.insert(it.key(), it.value()->value());
    in.insert(QStringLiteral("exit_cap_step"), ints_.value(QStringLiteral("cap_step_bp"))->value() / 10000.0);
    in.insert(QStringLiteral("rent_growth_step"), pct_.value(QStringLiteral("growth_step"))->value() / 100.0);
    in.insert(QStringLiteral("steps"), 5);
    return in;
}

void AltRealEstatePanel::start_proforma(quint64 id) {
    AltCall c;
    c.script = services::alt::kRealEstateScript;
    c.command = QStringLiteral("proforma");
    c.input = proforma_input();
    c.timeout_ms = 60 * 1000;
    QPointer<ui::AutoLoadPane> pane = pf_pane_;
    const quint64 sid = AltInvestmentsService::instance().call(c, this, [this, id, pane](const AltReply& r) {
        service_ids_.remove(id);
        if (!pane || r.cancelled)
            return;
        r.ok ? pane->deliver(id, r.data) : pane->fail(id, r.error, tr("Check the property inputs"));
    });
    service_ids_.insert(id, sid);
}

void AltRealEstatePanel::render_proforma(const QJsonValue& payload) {
    const QJsonObject d = payload.toObject();
    const QJsonObject s = d.value(QStringLiteral("summary")).toObject();
    const QString ccy; // amounts in the user's display currency
    auto money = [&ccy](double v) { return alt_wb_money(v, ccy); };
    auto num = [&s](const char* k) { return alt_wb_num(s.value(QLatin1String(k))); };
    alt_re_set(pf_kpis_, "lirr", alt_wb_pct(num("levered_irr")), tr("on %1 equity").arg(money(num("equity"))));
    alt_re_set(pf_kpis_, "uirr", alt_wb_pct(num("unlevered_irr")));
    alt_re_set(pf_kpis_, "em", alt_wb_multiple(num("equity_multiple")));
    alt_re_set(pf_kpis_, "cap", alt_wb_pct(num("going_in_cap")), tr("NOI Y1 %1").arg(money(num("noi_year1"))));
    alt_re_set(pf_kpis_, "coc", alt_wb_pct(num("cash_on_cash_y1")));
    alt_re_set(pf_kpis_, "dscr", alt_wb_multiple(num("dscr_min")),
               std::isfinite(num("dscr_year1")) ? tr("Y1 %1").arg(alt_wb_multiple(num("dscr_year1"))) : tr("no debt"));
    alt_re_set(pf_kpis_, "ltv", alt_wb_pct(num("ltv"), 1), tr("loan %1").arg(money(num("loan"))));
    alt_re_set(pf_kpis_, "dy", alt_wb_pct(num("debt_yield")));
    QStringList notes;
    for (const QJsonValue& n : d.value(QStringLiteral("notes")).toArray())
        notes << n.toString();
    notes << d.value(QStringLiteral("method")).toString();
    notes << tr("Exit: sale %1, net of costs %2, loan repaid %3, equity back %4.")
                 .arg(money(num("sale_price")), money(num("net_sale")), money(num("loan_payoff")),
                      money(num("exit_equity")));
    pf_notes_->setText(notes.join(QLatin1Char('\n')));

    // Cash flow by year (operating CF after debt; exit equity stacked on the last year).
    const QJsonArray years = d.value(QStringLiteral("years")).toArray();
    QStringList cats;
    QVector<double> op, exit_eq;
    QVector<QStringList> rows;
    QVector<QStringList> tones;
    for (int i = 0; i < years.size(); ++i) {
        const QJsonObject y = years.at(i).toObject();
        auto v = [&y](const char* k) { return alt_wb_num(y.value(QLatin1String(k))); };
        cats << tr("Y%1").arg(y.value(QStringLiteral("year")).toInt());
        op << v("cfads");
        exit_eq << (i == years.size() - 1 ? num("exit_equity") : 0.0);
        const double dscr = v("dscr");
        rows << QStringList{QString::number(y.value(QStringLiteral("year")).toInt()),
                            money(v("potential_rent") + v("other_income")),
                            money(v("vacancy")),
                            money(v("egi")),
                            money(v("opex") + v("mgmt_fee")),
                            money(v("noi")),
                            money(v("capex")),
                            money(v("debt_service")),
                            money(v("cfads")),
                            alt_wb_multiple(dscr),
                            money(v("loan_balance"))};
        QStringList t(11);
        if (v("cfads") < 0)
            t[8] = QStringLiteral("bad");
        if (std::isfinite(dscr) && dscr < 1.0)
            t[9] = QStringLiteral("bad");
        else if (std::isfinite(dscr) && dscr < 1.25)
            t[9] = QStringLiteral("warn");
        tones << t;
    }
    pf_chart_->set_categories(cats);
    ui::BarSeries a;
    a.name = tr("Cash flow after debt service");
    a.values = op;
    ui::BarSeries b;
    b.name = tr("Exit equity (sale less costs and loan)");
    b.values = exit_eq;
    pf_chart_->set_series({a, b});
    pf_chart_->set_value_formatter([money](double v) { return money(v); });

    const QStringList headers = {tr("Year"),   tr("Rent + other"), tr("Vacancy"), tr("EGI"),
                                 tr("Opex + mgmt"), tr("NOI"),      tr("Capex"),   tr("Debt service"),
                                 tr("CF after debt"), tr("DSCR"),  tr("Loan balance")};
    pf_model_->set_rows(headers, rows, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, tones);
    alt_wb_fit_table(pf_table_, 30);

    // Sensitivity: exit cap (rows) x rent growth (cols).
    const QJsonObject sens = d.value(QStringLiteral("sensitivity")).toObject();
    QVector<double> caps, growths;
    for (const QJsonValue& v : sens.value(QStringLiteral("rows")).toArray())
        caps << v.toDouble();
    for (const QJsonValue& v : sens.value(QStringLiteral("cols")).toArray())
        growths << v.toDouble();
    auto matrix = [](const QJsonArray& m) {
        QVector<QVector<double>> out;
        for (const QJsonValue& r : m) {
            QVector<double> row;
            for (const QJsonValue& c : r.toArray())
                row << alt_wb_num(c);
            out << row;
        }
        return out;
    };
    pf_sens_->clear();
    pf_sens_->set_title(tr("IRR sensitivity"), tr("exit cap rate x rent growth"));
    pf_sens_->set_axes(tr("Exit cap"), caps, tr("Rent growth"), growths,
                       [](double v) { return alt_wb_pct(v, 2); }, [](double v) { return alt_wb_pct(v, 1); });
    auto pct_fmt = [](double v) { return alt_wb_pct(v, 1); };
    pf_sens_->add_metric(tr("Levered IRR"), matrix(sens.value(QStringLiteral("values")).toArray()), pct_fmt, true);
    pf_sens_->add_metric(tr("Unlevered IRR"), matrix(sens.value(QStringLiteral("unlevered_values")).toArray()), pct_fmt,
                         true);
    pf_sens_->set_base_cell(sens.value(QStringLiteral("base_row")).toInt(), sens.value(QStringLiteral("base_col")).toInt());
    pf_sens_->set_note(tr("Each cell is the full model re-run with that exit cap and rent growth; colours centre on "
                          "the base case."));

    pf_export_ = AltWbExport{};
    pf_export_.title = tr("Property pro forma");
    pf_export_.figures = {{tr("Levered IRR"), alt_wb_pct(num("levered_irr"))},
                          {tr("Unlevered IRR"), alt_wb_pct(num("unlevered_irr"))},
                          {tr("Equity multiple"), alt_wb_multiple(num("equity_multiple"))},
                          {tr("Going-in cap"), alt_wb_pct(num("going_in_cap"))},
                          {tr("Cash-on-cash Y1"), alt_wb_pct(num("cash_on_cash_y1"))},
                          {tr("DSCR (min)"), alt_wb_multiple(num("dscr_min"))},
                          {tr("LTV"), alt_wb_pct(num("ltv"), 1)},
                          {tr("Debt yield"), alt_wb_pct(num("debt_yield"))},
                          {tr("Equity"), money(num("equity"))},
                          {tr("Exit equity"), money(num("exit_equity"))}};
    const QJsonObject in = proforma_input();
    for (auto it = in.constBegin(); it != in.constEnd(); ++it)
        pf_export_.inputs.append({ui::result::humanize(it.key()), QString::number(it.value().toDouble(), 'g', 10)});
    pf_export_.headers = headers;
    pf_export_.rows = rows;
    pf_export_.notes = notes.join(QLatin1Char(' '));
    alt_wb_set_actions_enabled(pf_actions_, true);
}

// ── REIT comparison ──────────────────────────────────────────────────────────

QWidget* AltRealEstatePanel::build_reit_page(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* top = new QWidget(page);
    auto* tl = new QVBoxLayout(top);
    tl->setContentsMargins(10, 8, 10, 6);
    tl->setSpacing(6);
    auto* row = new QWidget(top);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(6);
    rl->addWidget(alt_wb_label(tr("ADD REITS"), "altWbFormLabel", row));
    reit_add_ = new ui::SymbolSearchBox(row);
    services::ResolveOptions opts;
    opts.ctx.allowed_classes = {core::instrument::AssetClass::Equity, core::instrument::AssetClass::Etf};
    reit_add_->set_options(opts);
    reit_add_->set_multi(true);
    reit_add_->set_compact(true);
    reit_add_->set_recent_key(QStringLiteral("alt_reits"));
    reit_add_->set_placeholder(tr("REIT ticker or name: O, PLD, Prologis… (up to %1)").arg(kAltReMaxReits));
    rl->addWidget(reit_add_, 1);
    reit_run_ = alt_wb_button(tr("RUN COMPARISON"), true, row);
    rl->addWidget(reit_run_);
    tl->addWidget(row);
    connect(reit_add_, &ui::SymbolSearchBox::instrument_picked, this,
            [this](const core::instrument::Instrument& i) { add_reit_symbols({i.symbol}); });
    connect(reit_add_, &ui::SymbolSearchBox::instruments_picked, this,
            [this](const QVector<core::instrument::Instrument>& list) {
                QStringList s;
                for (const auto& i : list)
                    s << i.symbol;
                add_reit_symbols(s);
            });
    connect(reit_add_, &ui::SymbolSearchBox::unresolved, this,
            [this](const QStringList&, const QString& msg) { reit_status_->setText(msg); });

    using ui::DataGridColumn;
    auto ffo = DataGridColumn::number(QStringLiteral("ffo"), tr("FFO / SHARE (OPTIONAL)"), 2, /*required=*/false);
    ffo.tooltip = tr("Funds from operations per share from the REIT's own report (annual). Yahoo Finance does not "
                     "provide FFO; P/FFO is shown only where you enter it.");
    reit_grid_ = new ui::DataGridEditor({DataGridColumn::text(QStringLiteral("ticker"), tr("TICKER"), true), ffo}, top);
    reit_grid_->set_max_rows(kAltReMaxReits);
    reit_grid_->set_visible_rows(5);
    reit_grid_->set_hint(tr("Example list - replace it with yours. FFO per share is optional."));
    reit_grid_->from_json(QJsonArray{QJsonObject{{QStringLiteral("ticker"), QStringLiteral("O")}},
                                     QJsonObject{{QStringLiteral("ticker"), QStringLiteral("PLD")}},
                                     QJsonObject{{QStringLiteral("ticker"), QStringLiteral("SPG")}}});
    tl->addWidget(reit_grid_);
    reit_status_ = alt_wb_label(QString(), "altWbHint", top);
    reit_status_->setWordWrap(true);
    tl->addWidget(reit_status_);
    // The grid as JSON in a hidden line edit: InputMemory remembers it (tickers are not private data).
    reit_rows_store_ = new QLineEdit(top);
    reit_rows_store_->setVisible(false);
    reit_memory_ = new ui::InputMemory(QStringLiteral("alt_investments"), QStringLiteral("re_reits"), top);
    reit_memory_->track(QStringLiteral("rows"), reit_rows_store_);
    reit_memory_->restore();
    const QJsonDocument saved = QJsonDocument::fromJson(reit_rows_store_->text().toUtf8());
    if (saved.isArray() && !saved.array().isEmpty()) {
        reit_loading_ = true;
        reit_grid_->from_json(saved.array());
        reit_loading_ = false;
    }
    connect(reit_grid_, &ui::DataGridEditor::changed, this, [this]() {
        if (reit_loading_)
            return;
        reit_rows_store_->setText(QString::fromUtf8(QJsonDocument(reit_grid_->raw_json()).toJson(QJsonDocument::Compact)));
        update_reit_view();
    });
    vl->addWidget(top);

    reit_actions_ = alt_wb_make_actions(page, tr("LISTED REITS"));
    vl->addWidget(reit_actions_.row);
    reit_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::Manual, page);
    vl->addWidget(reit_pane_, 1);
    auto* body = new QWidget(reit_pane_->content_parent());
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(10, 8, 10, 10);
    bl->setSpacing(8);
    reit_model_ = new AltWbRowsModel(this);
    reit_table_ = alt_wb_make_table(body, reit_model_, QStringLiteral("reit_comparison"));
    bl->addWidget(reit_table_);
    reit_notes_ = alt_wb_label(QString(), "altWbNote", body);
    reit_notes_->setWordWrap(true);
    bl->addWidget(reit_notes_);
    bl->addStretch(1);
    reit_pane_->set_content(body);

    reit_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Table);
    reit_pane_->set_loading_text(tr("Loading REIT data from Yahoo Finance…"));
    reit_pane_->set_cache_key(QStringLiteral("alt_investments/re_reits"), 30);
    reit_pane_->set_max_age(ui::autoload::kDefaultMaxAgeSeconds);
    reit_pane_->set_status_label(reit_actions_.status);
    reit_pane_->set_timeout(120);
    reit_pane_->set_start([this](quint64 id) { start_reits(id); });
    reit_pane_->set_cancel([this](quint64 id) { cancel_service(id); });
    reit_pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render_reits(p); });
    update_reit_view();
    connect(reit_run_, &QPushButton::clicked, this, [this]() {
        update_reit_view();
        reit_pane_->run();
    });

    connect(reit_actions_.copy, &QPushButton::clicked, this, [this]() { alt_wb_copy(reit_export_); });
    connect(reit_actions_.csv, &QPushButton::clicked, this,
            [this]() { alt_wb_save_csv(reit_export_, QStringLiteral("reit_comparison"), this); });
    connect(reit_actions_.image, &QPushButton::clicked, this,
            [this]() { ui::ChartExport::save_png(reit_table_, QStringLiteral("reit_comparison")); });
    reit_actions_.report->set_payload_provider([this]() { return alt_wb_payload(reit_export_); });
    return page;
}

void AltRealEstatePanel::add_reit_symbols(const QStringList& symbols) {
    QJsonArray rows = reit_grid_->raw_json();
    QStringList have;
    for (const QJsonValue& v : rows)
        have << v.toObject().value(QStringLiteral("ticker")).toString().trimmed().toUpper();
    int added = 0, skipped = 0;
    for (const QString& raw : symbols) {
        const QString s = raw.trimmed().toUpper();
        if (s.isEmpty() || have.contains(s))
            continue;
        if (rows.size() >= kAltReMaxReits) {
            ++skipped;
            continue;
        }
        rows.append(QJsonObject{{QStringLiteral("ticker"), s}});
        have << s;
        ++added;
    }
    if (added > 0)
        reit_grid_->from_json(rows); // emits changed -> remembered, view updated
    reit_status_->setText(skipped > 0 ? tr("At most %1 REITs per comparison: %2 not added.").arg(kAltReMaxReits).arg(skipped)
                                      : QString());
}

QJsonObject AltRealEstatePanel::reit_input() const {
    QJsonArray tickers;
    QJsonObject ffo;
    for (const QJsonValue& v : reit_grid_->to_json()) {
        const QJsonObject o = v.toObject();
        const QString t = o.value(QStringLiteral("ticker")).toString().trimmed().toUpper();
        if (t.isEmpty())
            continue;
        tickers.append(t);
        if (o.value(QStringLiteral("ffo")).isDouble() && o.value(QStringLiteral("ffo")).toDouble() > 0)
            ffo.insert(t, o.value(QStringLiteral("ffo")).toDouble());
    }
    return QJsonObject{{QStringLiteral("tickers"), tickers}, {QStringLiteral("ffo_per_share"), ffo}};
}

QString AltRealEstatePanel::reit_view_key() const {
    QStringList t;
    for (const QJsonValue& v : reit_input().value(QStringLiteral("tickers")).toArray())
        t << v.toString();
    t.sort();
    return QStringLiteral("reits/") + t.join(QLatin1Char(','));
}

void AltRealEstatePanel::update_reit_view() {
    const QJsonObject in = reit_input();
    const int n = static_cast<int>(in.value(QStringLiteral("tickers")).toArray().size());
    ui::EmptyState* pv = reit_pane_->preview();
    pv->apply(ui::EmptyState::preview_spec(
        tr("Listed REIT comparison"),
        tr("Price, dividend yield, market cap, debt / equity, price / book, net debt / EBITDA and 1-year total return "
           "of each REIT, from Yahoo Finance; P/FFO where you entered FFO per share. Not run automatically: it calls a "
           "rate-limited free source."),
        tr("RUN COMPARISON"), tr("Yahoo Finance · no credits"), ui::EmptyState::Tone::Info,
        ui::LoadingSkeleton::Shape::Table));
    QStringList tickers;
    for (const QJsonValue& v : in.value(QStringLiteral("tickers")).toArray())
        tickers << v.toString();
    pv->set_details({{tr("REITs"), tickers.isEmpty() ? tr("none yet - add some above") : tickers.join(QStringLiteral(", "))}},
                    tr("WHAT WILL BE LOADED"));
    pv->set_primary_enabled(n > 0);
    reit_run_->setEnabled(n > 0);
    reit_pane_->set_view(reit_view_key(), ui::AutoLoadPane::Policy::Manual);
    reit_pane_->set_signature(ui::autoload::signature_of(QJsonValue(in)));
}

void AltRealEstatePanel::start_reits(quint64 id) {
    const QJsonObject in = reit_input();
    if (in.value(QStringLiteral("tickers")).toArray().isEmpty()) {
        reit_pane_->fail(id, tr("Add at least one REIT ticker above."), tr("Nothing to compare"));
        return;
    }
    AltCall c;
    c.script = services::alt::kRealEstateScript;
    c.command = QStringLiteral("reit_compare");
    c.input = in;
    c.timeout_ms = 120 * 1000;
    c.source = QStringLiteral("Yahoo Finance");
    QPointer<ui::AutoLoadPane> pane = reit_pane_;
    const quint64 sid = AltInvestmentsService::instance().call(c, this, [this, id, pane](const AltReply& r) {
        service_ids_.remove(id);
        if (!pane || r.cancelled)
            return;
        r.ok ? pane->deliver(id, r.data) : pane->fail(id, r.error, tr("REIT data not available"));
    });
    service_ids_.insert(id, sid);
}

void AltRealEstatePanel::render_reits(const QJsonValue& payload) {
    const QJsonObject d = payload.toObject();
    QVector<QStringList> rows;
    QVector<QStringList> tones;
    bool any_ffo = false;
    for (const QJsonValue& v : d.value(QStringLiteral("rows")).toArray())
        if (v.toObject().contains(QStringLiteral("p_ffo")))
            any_ffo = true;
    for (const QJsonValue& v : d.value(QStringLiteral("rows")).toArray()) {
        const QJsonObject r = v.toObject();
        const QString t = r.value(QStringLiteral("ticker")).toString();
        if (r.contains(QStringLiteral("error"))) {
            QStringList line{t, r.value(QStringLiteral("error")).toString()};
            while (line.size() < (any_ffo ? 11 : 9))
                line << QString();
            rows << line;
            QStringList tone(line.size());
            tone[1] = QStringLiteral("warn");
            tones << tone;
            continue;
        }
        const QString ccy = r.value(QStringLiteral("currency")).toString();
        auto n = [&r](const char* k) { return alt_wb_num(r.value(QLatin1String(k))); };
        const double tr1 = n("total_return_1y");
        QStringList line{t,
                         r.value(QStringLiteral("name")).toString(),
                         alt_wb_money(n("price"), ccy, false),
                         alt_wb_pct(n("dividend_yield")),
                         alt_wb_money(n("market_cap"), ccy),
                         alt_wb_multiple(n("debt_to_equity")),
                         alt_wb_multiple(n("price_to_book")),
                         alt_wb_multiple(n("net_debt_to_ebitda"), 1),
                         alt_wb_pct(tr1, 1, true)};
        if (any_ffo)
            line << alt_wb_money(n("ffo_per_share"), ccy, false) << alt_wb_multiple(n("p_ffo"), 1);
        QStringList tone(line.size());
        tone[8] = std::isfinite(tr1) ? QString() : QStringLiteral("dim");
        rows << line;
        tones << tone;
    }
    QStringList headers = {tr("Ticker"), tr("Name"), tr("Price"), tr("Dividend yield"), tr("Market cap"),
                           tr("Debt / equity"), tr("Price / book"), tr("Net debt / EBITDA"), tr("1y total return")};
    if (any_ffo)
        headers << tr("FFO / share") << tr("P / FFO");
    QVector<int> numeric;
    for (int i = 2; i < headers.size(); ++i)
        numeric << i;
    reit_model_->set_rows(headers, rows, numeric, tones);
    reit_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    alt_wb_fit_table(reit_table_, kAltReMaxReits + 1);
    QStringList notes;
    for (const QJsonValue& n : d.value(QStringLiteral("notes")).toArray())
        notes << n.toString();
    notes << tr("\"-\" = not derivable from the free data for that REIT.");
    reit_notes_->setText(notes.join(QLatin1Char('\n')));

    reit_export_ = AltWbExport{};
    reit_export_.title = tr("Listed REIT comparison");
    reit_export_.headers = headers;
    reit_export_.rows = rows;
    reit_export_.notes = notes.join(QLatin1Char(' ')) + QStringLiteral(" ") + tr("Source: Yahoo Finance.");
    alt_wb_set_actions_enabled(reit_actions_, true);
}

} // namespace fincept::screens::alt
