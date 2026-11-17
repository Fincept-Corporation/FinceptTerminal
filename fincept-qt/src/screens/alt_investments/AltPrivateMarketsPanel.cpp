// AltPrivateMarketsPanel.cpp — see AltPrivateMarketsPanel.h.
#include "screens/alt_investments/AltPrivateMarketsPanel.h"

#include "core/instrument/Instrument.h"
#include "core/logging/Logger.h"
#include "screens/alt_investments/AltPanels.h"
#include "services/alt_investments/AltInvestmentsService.h"
#include "services/instruments/InstrumentResolver.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/ChartExport.h"
#include "ui/charts/DonutChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/AmountInput.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/DataGridEditor.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/report/AddToReportButton.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShowEvent>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::alt {

using fincept::services::alt::AltCall;
using fincept::services::alt::AltFund;
using fincept::services::alt::AltFundStore;
using fincept::services::alt::AltInvestmentsService;
using fincept::services::alt::AltReply;

namespace {

QString alt_pm_tr(const char* s) {
    return QCoreApplication::translate("AltPrivateMarketsPanel", s);
}

/// Strategy values (stored) and their labels.
QVector<QPair<QString, QString>> alt_pm_strategies() {
    return {{QStringLiteral("buyout"), alt_pm_tr("Buyout")},
            {QStringLiteral("venture"), alt_pm_tr("Venture capital")},
            {QStringLiteral("growth"), alt_pm_tr("Growth equity")},
            {QStringLiteral("credit"), alt_pm_tr("Private credit")},
            {QStringLiteral("real_estate"), alt_pm_tr("Real estate")},
            {QStringLiteral("infrastructure"), alt_pm_tr("Infrastructure")},
            {QStringLiteral("secondaries"), alt_pm_tr("Secondaries")},
            {QStringLiteral("fund_of_funds"), alt_pm_tr("Fund of funds")},
            {QStringLiteral("other"), alt_pm_tr("Other")}};
}

QString alt_pm_strategy_label(const QString& value) {
    for (const auto& s : alt_pm_strategies())
        if (s.first == value)
            return s.second;
    return value.isEmpty() ? QStringLiteral("-") : value;
}

/// The ledger grid: date, kind, amount, note. Paste accepts "date, amount" (calls negative) or
/// "date, kind, amount[, note]" in either column order.
QString alt_pm_kind(const QString& text) {
    static const QHash<QString, QString> kKinds = {
        {QStringLiteral("call"), QStringLiteral("call")},
        {QStringLiteral("capital call"), QStringLiteral("call")},
        {QStringLiteral("contribution"), QStringLiteral("call")},
        {QStringLiteral("drawdown"), QStringLiteral("call")},
        {QStringLiteral("paid-in"), QStringLiteral("call")},
        {QStringLiteral("paid in"), QStringLiteral("call")},
        {QStringLiteral("investment"), QStringLiteral("call")},
        {QStringLiteral("distribution"), QStringLiteral("distribution")},
        {QStringLiteral("dist"), QStringLiteral("distribution")},
        {QStringLiteral("dist."), QStringLiteral("distribution")},
        {QStringLiteral("return of capital"), QStringLiteral("distribution")},
        {QStringLiteral("proceeds"), QStringLiteral("distribution")},
        {QStringLiteral("nav"), QStringLiteral("nav")},
        {QStringLiteral("net asset value"), QStringLiteral("nav")},
        {QStringLiteral("valuation"), QStringLiteral("nav")},
        {QStringLiteral("value"), QStringLiteral("nav")},
        {QStringLiteral("residual value"), QStringLiteral("nav")},
        {QStringLiteral("fair value"), QStringLiteral("nav")},
        {QStringLiteral("fee"), QStringLiteral("fee")},
        {QStringLiteral("fees"), QStringLiteral("fee")},
        {QStringLiteral("management fee"), QStringLiteral("fee")},
    };
    return kKinds.value(text.trimmed().toLower());
}

ui::DataGridEditor* alt_pm_make_ledger(QWidget* parent) {
    using ui::DataGridColumn;
    auto kind = DataGridColumn::choice(QStringLiteral("kind"), alt_pm_tr("KIND"),
                                       {QStringLiteral("call"), QStringLiteral("distribution"), QStringLiteral("nav"),
                                        QStringLiteral("fee")},
                                       {alt_pm_tr("Call"), alt_pm_tr("Distribution"), alt_pm_tr("NAV"), alt_pm_tr("Fee")});
    kind.synonyms = {{QStringLiteral("capital call"), QStringLiteral("call")},
                     {QStringLiteral("contribution"), QStringLiteral("call")},
                     {QStringLiteral("drawdown"), QStringLiteral("call")},
                     {QStringLiteral("dist"), QStringLiteral("distribution")},
                     {QStringLiteral("proceeds"), QStringLiteral("distribution")},
                     {QStringLiteral("net asset value"), QStringLiteral("nav")},
                     {QStringLiteral("valuation"), QStringLiteral("nav")},
                     {QStringLiteral("fair value"), QStringLiteral("nav")},
                     {QStringLiteral("fees"), QStringLiteral("fee")},
                     {QStringLiteral("management fee"), QStringLiteral("fee")}};
    kind.tooltip = alt_pm_tr("Call = capital paid in, Distribution = cash paid back, NAV = the fund's reported value "
                             "on that date, Fee = fees paid in (count as paid-in and against the commitment)");
    // Read like every money field ("2.5M", "1,250,000", "$1.2M"); the currency is the fund's (set on load).
    auto amount = DataGridColumn::amount(QStringLiteral("amount"), alt_pm_tr("AMOUNT"), QString(), 0);
    amount.tooltip = alt_pm_tr("A positive amount in the fund's currency; the kind gives the direction");
    QVector<DataGridColumn> cols = {DataGridColumn::date(QStringLiteral("date"), alt_pm_tr("DATE")), kind, amount,
                                    DataGridColumn::text(QStringLiteral("note"), alt_pm_tr("NOTE"))};
    auto* grid = new ui::DataGridEditor(cols, parent);
    grid->set_paste_adapter([](const QVector<QStringList>& cells) {
        QVector<QStringList> out;
        out.reserve(cells.size());
        for (const QStringList& c : cells) {
            auto by_sign = [](const QString& a) {
                const ui::formatting::AmountInput v = ui::formatting::read_amount_input(a, QString());
                if (!v.ok())
                    return QString();
                return *v.value < 0 ? QStringLiteral("call") : QStringLiteral("distribution");
            };
            auto unsigned_amount = [](const QString& a) {
                QString s = a.trimmed();
                if (s.startsWith(QLatin1Char('-')))
                    s.remove(0, 1);
                if (s.startsWith(QLatin1Char('(')) && s.endsWith(QLatin1Char(')')))
                    s = s.mid(1, s.size() - 2);
                return s;
            };
            const QString c0 = c.value(0), c1 = c.value(1), c2 = c.value(2);
            if (c.size() <= 1)
                out << QStringList{c0, QString(), QString(), QString()};
            else if (c.size() == 2)
                out << QStringList{c0, by_sign(c1), unsigned_amount(c1), QString()};
            else if (!alt_pm_kind(c1).isEmpty())
                out << QStringList{c0, alt_pm_kind(c1), unsigned_amount(c2), c.mid(3).join(QLatin1Char(' '))};
            else if (!alt_pm_kind(c2).isEmpty())
                out << QStringList{c0, alt_pm_kind(c2), unsigned_amount(c1), c.mid(3).join(QLatin1Char(' '))};
            else
                out << QStringList{c0, by_sign(c1), unsigned_amount(c1), c.mid(2).join(QLatin1Char(' '))};
        }
        return out;
    });
    grid->set_hint(alt_pm_tr("Paste from a spreadsheet: date, amount (calls negative) or date, kind, amount. "
                             "Saved as you edit; rows that do not validate are not saved."));
    grid->set_visible_rows(9);
    return grid;
}

/// The illustrative example fund offered on an empty book (labelled as such everywhere).
QJsonArray alt_pm_example_flows() {
    const char* const rows[][3] = {
        {"2020-03-31", "call", "1000000"},       {"2020-12-31", "nav", "950000"},
        {"2021-06-30", "call", "500000"},        {"2021-12-31", "nav", "1600000"},
        {"2022-12-31", "nav", "1500000"},        {"2023-12-31", "distribution", "600000"},
        {"2023-12-31", "nav", "1150000"},        {"2024-12-31", "nav", "1300000"},
        {"2025-12-31", "nav", "1350000"},        {"2026-06-30", "nav", "1400000"},
    };
    QJsonArray out;
    for (const auto& r : rows)
        out.append(QJsonObject{{QStringLiteral("date"), QString::fromLatin1(r[0])},
                               {QStringLiteral("kind"), QString::fromLatin1(r[1])},
                               {QStringLiteral("amount"), QString::fromLatin1(r[2]).toDouble()}});
    return out;
}

QDoubleSpinBox* alt_pm_spin(QWidget* parent, double lo, double hi, double val, int decimals, const QString& suffix) {
    auto* s = new QDoubleSpinBox(parent);
    s->setRange(lo, hi);
    s->setDecimals(decimals);
    s->setValue(val);
    if (!suffix.isEmpty())
        s->setSuffix(suffix);
    s->setGroupSeparatorShown(true);
    s->setKeyboardTracking(false);
    return s;
}

ui::StatTile* alt_pm_tile(ui::KpiStrip* strip, const char* key, const QString& label) {
    return strip->add_tile(QString::fromLatin1(key), label);
}

void alt_pm_set(ui::KpiStrip* strip, const char* key, const QString& value, const QString& sub = QString()) {
    if (ui::StatTile* t = strip->tile(QString::fromLatin1(key))) {
        t->set_value(value);
        t->set_sub_label(sub);
    }
}

QStringList alt_pm_notes(const QJsonValue& v) {
    QStringList out;
    for (const QJsonValue& n : v.toArray())
        if (n.isString() && !n.toString().isEmpty())
            out << n.toString();
    return out;
}

} // namespace

QWidget* make_private_markets_panel(QWidget* parent) {
    return new AltPrivateMarketsPanel(parent);
}

// ═════════════════════════════════════════════════════════════════════════════

AltPrivateMarketsPanel::AltPrivateMarketsPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("altWbRoot"));
    setStyleSheet(alt_wb_style()); // before any child exists
    build_ui();
    connect(&AltFundStore::instance(), &AltFundStore::funds_changed, this, [this]() {
        if (book_pane_)
            book_pane_->invalidate();
    });
    // A number-format change re-renders what is shown from the payloads already held (nothing is fetched).
    ui::formatting::on_format_changed(this, [this]() {
        if (funds_loaded_)
            show_funds();
        if (perf_pane_->has_result())
            render_performance(perf_pane_->payload());
        if (pme_pane_->has_result())
            render_pme(pme_pane_->payload());
        if (pace_pane_->has_result())
            render_pacing(pace_pane_->payload());
        if (book_pane_->has_result())
            render_book(book_pane_->payload());
    });
}

AltPrivateMarketsPanel::~AltPrivateMarketsPanel() {
    for (auto it = service_ids_.cbegin(); it != service_ids_.cend(); ++it)
        AltInvestmentsService::instance().cancel(it.value());
    for (ui::AutoLoadPane* p : {perf_pane_, pme_pane_, pace_pane_, book_pane_})
        if (p)
            p->set_cancel({});
}

void AltPrivateMarketsPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!started_) {
        started_ = true;
        reload_funds(); // first show only: nothing starts in the constructor
    }
}

QString AltPrivateMarketsPanel::currency() const {
    const AltFund* f = current_fund();
    return f ? f->currency : QStringLiteral("USD");
}

// ── Build ────────────────────────────────────────────────────────────────────

void AltPrivateMarketsPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->setObjectName(QStringLiteral("altWbSplit"));
    split->setHandleWidth(1);
    split->setChildrenCollapsible(false);
    QWidget* left = build_book_side(split);
    tabs_ = new QTabWidget(split);
    tabs_->setObjectName(QStringLiteral("altWbTabs"));
    tabs_->setDocumentMode(false);
    tabs_->setUsesScrollButtons(true);
    tabs_->setElideMode(Qt::ElideNone);
    tabs_->addTab(build_performance_page(tabs_), tr("PERFORMANCE"));
    tabs_->addTab(build_pme_page(tabs_), tr("PME"));
    tabs_->addTab(build_pacing_page(tabs_), tr("PACING"));
    tabs_->addTab(build_book_page(tabs_), tr("BOOK"));
    split->addWidget(left);
    split->addWidget(tabs_);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    const int char_w = QFontMetrics(ui::fonts::font(-1)).averageCharWidth();
    left->setMinimumWidth(std::max(340, char_w * 50));
    left->setMaximumWidth(std::max(520, char_w * 80));
    split->setSizes({std::max(380, char_w * 56), 4000});
    root->addWidget(split, 1);
}

QWidget* AltPrivateMarketsPanel::build_book_side(QWidget* parent) {
    auto* side = new QWidget(parent);
    auto* vl = new QVBoxLayout(side);
    vl->setContentsMargins(10, 8, 8, 8);
    vl->setSpacing(6);

    auto* head = new QWidget(side);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(4);
    hl->addWidget(alt_wb_label(tr("FUND BOOK"), "altWbSection", head), 1);
    add_btn_ = alt_wb_button(tr("ADD"), false, head);
    add_btn_->setToolTip(tr("Add a fund (name, strategy, vintage, commitment)"));
    edit_btn_ = alt_wb_button(tr("EDIT"), false, head);
    delete_btn_ = alt_wb_button(tr("DELETE"), false, head);
    delete_btn_->setProperty("fincept_agent_risk", "delete"); // agents: deletes the fund
    edit_btn_->setEnabled(false);
    delete_btn_->setEnabled(false);
    hl->addWidget(add_btn_);
    hl->addWidget(edit_btn_);
    hl->addWidget(delete_btn_);
    vl->addWidget(head);
    connect(add_btn_, &QPushButton::clicked, this, &AltPrivateMarketsPanel::add_fund);
    connect(edit_btn_, &QPushButton::clicked, this, &AltPrivateMarketsPanel::edit_fund);
    connect(delete_btn_, &QPushButton::clicked, this, &AltPrivateMarketsPanel::delete_fund);

    fund_model_ = new AltWbRowsModel(this);
    fund_view_ = alt_wb_make_table(side, fund_model_, QStringLiteral("fund_book"));
    fund_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    fund_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    fund_view_->horizontalHeader()->setStretchLastSection(true);
    connect(fund_view_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& cur, const QModelIndex&) {
                if (cur.isValid() && cur.row() < funds_.size())
                    select_fund(funds_.at(cur.row()).id);
            });
    connect(fund_view_, &QTableView::doubleClicked, this, [this](const QModelIndex&) { edit_fund(); });
    vl->addWidget(fund_view_);

    book_hint_ = alt_wb_label(tr("Loading the fund book…"), "altWbHint", side);
    book_hint_->setWordWrap(true);
    vl->addWidget(book_hint_);
    example_btn_ = alt_wb_button(tr("ADD EXAMPLE FUND"), false, side);
    example_btn_->setToolTip(tr("Adds an illustrative fund with sample cash flows (clearly named as an example) "
                                "so every view has something to show; replace or delete it any time"));
    example_btn_->setVisible(false);
    connect(example_btn_, &QPushButton::clicked, this, &AltPrivateMarketsPanel::add_example_fund);
    vl->addWidget(example_btn_, 0, Qt::AlignLeft);

    auto* lhead = new QWidget(side);
    auto* lhl = new QHBoxLayout(lhead);
    lhl->setContentsMargins(0, 6, 0, 0);
    lhl->setSpacing(6);
    ledger_title_ = alt_wb_label(tr("CASH FLOWS"), "altWbSection", lhead);
    lhl->addWidget(ledger_title_, 1);
    ledger_status_ = alt_wb_label(QString(), "altWbStatus", lhead);
    lhl->addWidget(ledger_status_);
    vl->addWidget(lhead);

    ledger_ = alt_pm_make_ledger(side);
    ledger_->setEnabled(false);
    vl->addWidget(ledger_, 1);
    connect(ledger_, &ui::DataGridEditor::changed, this, &AltPrivateMarketsPanel::on_ledger_changed);

    save_timer_ = new QTimer(this);
    save_timer_->setSingleShot(true);
    save_timer_->setInterval(800);
    connect(save_timer_, &QTimer::timeout, this, &AltPrivateMarketsPanel::flush_ledger_save);
    return side;
}

QWidget* AltPrivateMarketsPanel::make_pane_body(ui::AutoLoadPane* pane) {
    auto* scroll = new QScrollArea(pane->content_parent());
    scroll->setObjectName(QStringLiteral("altWbScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget(scroll);
    body->setObjectName(QStringLiteral("altWbScrollBody"));
    scroll->setWidget(body);
    pane->set_content(scroll);
    return body;
}

QWidget* AltPrivateMarketsPanel::build_performance_page(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    perf_actions_ = alt_wb_make_actions(page, tr("FUND PERFORMANCE"));
    vl->addWidget(perf_actions_.row);

    perf_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, page);
    vl->addWidget(perf_pane_, 1);
    QWidget* body = make_pane_body(perf_pane_);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(10, 8, 10, 10);
    bl->setSpacing(8);
    perf_stack_ = new QStackedWidget(body);
    bl->addWidget(perf_stack_);

    perf_empty_ = new ui::EmptyState(perf_stack_);
    perf_empty_->set_alignment(Qt::AlignTop);
    perf_stack_->addWidget(perf_empty_);
    connect(perf_empty_, &ui::EmptyState::primary_clicked, this, &AltPrivateMarketsPanel::add_example_fund);
    connect(perf_empty_, &ui::EmptyState::secondary_clicked, this, &AltPrivateMarketsPanel::add_fund);

    auto* res = new QWidget(perf_stack_);
    auto* rl = new QVBoxLayout(res);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(8);
    perf_kpis_ = new ui::KpiStrip(res);
    alt_pm_tile(perf_kpis_, "irr", tr("IRR (dated)"));
    alt_pm_tile(perf_kpis_, "tvpi", tr("TVPI"));
    alt_pm_tile(perf_kpis_, "dpi", tr("DPI"));
    alt_pm_tile(perf_kpis_, "rvpi", tr("RVPI"));
    alt_pm_tile(perf_kpis_, "paid", tr("Paid-in"));
    alt_pm_tile(perf_kpis_, "dist", tr("Distributed"));
    alt_pm_tile(perf_kpis_, "nav", tr("NAV"));
    alt_pm_tile(perf_kpis_, "unfunded", tr("Unfunded"));
    rl->addWidget(perf_kpis_);
    perf_notes_ = alt_wb_label(QString(), "altWbNote", res);
    perf_notes_->setWordWrap(true);
    rl->addWidget(perf_notes_);
    rl->addWidget(alt_wb_label(tr("J-CURVE: CUMULATIVE PAID-IN, DISTRIBUTED, NAV AND NET CASH"), "altWbSection", res));
    jcurve_ = new ui::TimeSeriesChart(res);
    jcurve_->set_x_is_time(true);
    jcurve_->set_show_legend(true);
    jcurve_->set_show_mode_buttons(false);
    jcurve_->set_export_name(QStringLiteral("fund_jcurve"));
    jcurve_->setMinimumHeight(260);
    rl->addWidget(jcurve_);
    rl->addWidget(alt_wb_label(tr("YEAR-END FIGURES"), "altWbSection", res));
    perf_model_ = new AltWbRowsModel(this);
    perf_table_ = alt_wb_make_table(res, perf_model_, QStringLiteral("fund_year_end"));
    rl->addWidget(perf_table_);
    rl->addStretch(1);
    perf_stack_->addWidget(res);

    perf_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Result);
    perf_pane_->set_loading_text(tr("Computing the fund's performance…"));
    perf_pane_->set_cache_key(QStringLiteral("alt_investments/pm_performance"));
    perf_pane_->set_max_age(ui::autoload::kNeverRefresh);
    perf_pane_->set_status_label(perf_actions_.status);
    perf_pane_->set_timeout(120);
    perf_pane_->set_start([this](quint64 id) { start_performance(id); });
    perf_pane_->set_cancel([this](quint64 id) { cancel_service(id); });
    perf_pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render_performance(p); });
    perf_pane_->set_view(QStringLiteral("perf/loading"));

    connect(perf_actions_.copy, &QPushButton::clicked, this, [this]() { alt_wb_copy(perf_export_); });
    connect(perf_actions_.csv, &QPushButton::clicked, this,
            [this]() { alt_wb_save_csv(perf_export_, QStringLiteral("fund_performance"), this); });
    connect(perf_actions_.image, &QPushButton::clicked, this,
            [this]() { ui::ChartExport::save_png(perf_stack_, QStringLiteral("fund_performance")); });
    perf_actions_.report->set_payload_provider(
        [this]() { return alt_wb_payload(perf_export_, {{tr("J-curve"), jcurve_}}); });
    return page;
}

QWidget* AltPrivateMarketsPanel::build_pme_page(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    pme_actions_ = alt_wb_make_actions(page, tr("PUBLIC MARKET EQUIVALENT"));
    vl->addWidget(pme_actions_.row);

    auto* ctl = new QWidget(page);
    auto* cl = new QHBoxLayout(ctl);
    cl->setContentsMargins(10, 6, 10, 2);
    cl->setSpacing(6);
    cl->addWidget(alt_wb_label(tr("INDEX"), "altWbFormLabel", ctl));
    pme_index_ = new ui::SymbolSearchBox(ctl);
    pme_index_->setObjectName(QStringLiteral("alt_pm_pme_index"));
    services::ResolveOptions opts;
    opts.ctx.allowed_classes = {core::instrument::AssetClass::Etf, core::instrument::AssetClass::Index,
                                core::instrument::AssetClass::Fund, core::instrument::AssetClass::Equity};
    pme_index_->set_options(opts);
    pme_index_->set_compact(true);
    pme_index_->set_clear_on_pick(false);
    pme_index_->set_recent_key(QStringLiteral("alt_pme_index"));
    pme_index_->set_placeholder(tr("Index or ETF: SPY, ^GSPC, IWM, ACWI…"));
    pme_index_->set_text(QStringLiteral("SPY"));
    pme_index_->setMinimumWidth(QFontMetrics(ui::fonts::font(-1)).averageCharWidth() * 28);
    cl->addWidget(pme_index_);
    pme_run_ = alt_wb_button(tr("RUN PME"), true, ctl);
    pme_run_->setToolTip(tr("Download the index prices (Yahoo Finance) and compare the fund with them"));
    cl->addWidget(pme_run_);
    auto* hint = alt_wb_label(tr("Total-return index (adjusted close). Not run automatically: it downloads prices."),
                              "altWbHint", ctl);
    hint->setWordWrap(true);
    cl->addWidget(hint, 1);
    vl->addWidget(ctl);
    connect(pme_index_, &ui::SymbolSearchBox::instrument_picked, this, [this](const core::instrument::Instrument& i) {
        if (!i.symbol.isEmpty())
            pme_index_->set_text(i.symbol);
        // Remember the fund's benchmark (a small fund update), then show that index's result.
        if (const AltFund* f = current_fund()) {
            AltFund copy = *f;
            copy.benchmark = pme_symbol();
            if (copy.benchmark != f->benchmark)
                AltFundStore::instance().save_fund(copy, this, [](const AltFund&, const QString&) {});
            for (AltFund& x : funds_)
                if (x.id == copy.id)
                    x.benchmark = copy.benchmark;
        }
        update_views();
    });
    connect(pme_run_, &QPushButton::clicked, this, [this]() {
        update_views();
        pme_pane_->run();
    });

    pme_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::Manual, page);
    vl->addWidget(pme_pane_, 1);
    QWidget* body = make_pane_body(pme_pane_);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(10, 8, 10, 10);
    bl->setSpacing(8);
    pme_stack_ = new QStackedWidget(body);
    bl->addWidget(pme_stack_);
    pme_empty_ = new ui::EmptyState(pme_stack_);
    pme_empty_->set_alignment(Qt::AlignTop);
    pme_stack_->addWidget(pme_empty_);
    connect(pme_empty_, &ui::EmptyState::primary_clicked, this, &AltPrivateMarketsPanel::add_example_fund);
    auto* res = new QWidget(pme_stack_);
    auto* rl = new QVBoxLayout(res);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(8);
    pme_kpis_ = new ui::KpiStrip(res);
    alt_pm_tile(pme_kpis_, "ks", tr("KS-PME"));
    alt_pm_tile(pme_kpis_, "alpha", tr("Direct Alpha"));
    alt_pm_tile(pme_kpis_, "fund", tr("Fund IRR"));
    alt_pm_tile(pme_kpis_, "ln", tr("Long-Nickels PME IRR"));
    alt_pm_tile(pme_kpis_, "plus", tr("PME+ IRR"));
    alt_pm_tile(pme_kpis_, "index", tr("Index return"));
    rl->addWidget(pme_kpis_);
    pme_notes_ = alt_wb_label(QString(), "altWbNote", res);
    pme_notes_->setWordWrap(true);
    rl->addWidget(pme_notes_);
    rl->addWidget(alt_wb_label(tr("FUND VALUE VS THE SAME CASH FLOWS IN THE INDEX"), "altWbSection", res));
    pme_chart_ = new ui::TimeSeriesChart(res);
    pme_chart_->set_x_is_time(true);
    pme_chart_->set_show_legend(true);
    pme_chart_->set_show_mode_buttons(false);
    pme_chart_->set_export_name(QStringLiteral("fund_pme"));
    pme_chart_->setMinimumHeight(240);
    rl->addWidget(pme_chart_);
    pme_model_ = new AltWbRowsModel(this);
    pme_table_ = alt_wb_make_table(res, pme_model_, QStringLiteral("fund_pme_measures"));
    rl->addWidget(pme_table_);
    rl->addStretch(1);
    pme_stack_->addWidget(res);

    pme_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Result);
    pme_pane_->set_loading_text(tr("Downloading index prices and computing the PME…"));
    pme_pane_->set_cache_key(QStringLiteral("alt_investments/pm_pme"), 60);
    pme_pane_->set_max_age(ui::autoload::kDefaultMaxAgeSeconds);
    pme_pane_->set_status_label(pme_actions_.status);
    pme_pane_->set_timeout(120);
    pme_pane_->set_start([this](quint64 id) { start_pme(id); });
    pme_pane_->set_cancel([this](quint64 id) { cancel_service(id); });
    pme_pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render_pme(p); });
    configure_pme_preview();
    pme_pane_->set_view(QStringLiteral("pme/loading"), ui::AutoLoadPane::Policy::AutoRun);

    connect(pme_actions_.copy, &QPushButton::clicked, this, [this]() { alt_wb_copy(pme_export_); });
    connect(pme_actions_.csv, &QPushButton::clicked, this,
            [this]() { alt_wb_save_csv(pme_export_, QStringLiteral("fund_pme"), this); });
    connect(pme_actions_.image, &QPushButton::clicked, this,
            [this]() { ui::ChartExport::save_png(pme_stack_, QStringLiteral("fund_pme")); });
    pme_actions_.report->set_payload_provider(
        [this]() { return alt_wb_payload(pme_export_, {{tr("Fund vs index"), pme_chart_}}); });
    return page;
}

QWidget* AltPrivateMarketsPanel::build_pacing_page(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    pace_actions_ = alt_wb_make_actions(page, tr("COMMITMENT PACING"));
    vl->addWidget(pace_actions_.row);

    auto* form = new QWidget(page);
    form->setObjectName(QStringLiteral("altWbForm"));
    auto* grid = new QGridLayout(form);
    grid->setContentsMargins(10, 6, 10, 4);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(4);
    pace_commitment_ = alt_pm_spin(form, 1, 1e13, 10000000, 0, QString());
    ui::formatting::bind_currency_affix(pace_commitment_); // pacing is in the display currency
    pace_life_ = new QSpinBox(form);
    pace_life_->setRange(2, 30);
    pace_life_->setValue(12);
    pace_life_->setSuffix(tr(" yrs"));
    pace_rates_ = new QLineEdit(form);
    pace_rates_->setText(QStringLiteral("25, 33.3, 50"));
    pace_rates_->setToolTip(tr("Share of the uncalled commitment called in year 1, 2, 3 … (in %); the last value "
                               "repeats for the remaining years"));
    pace_bow_ = alt_pm_spin(form, 0.1, 10, 2.5, 2, QString());
    pace_bow_->setToolTip(tr("Bow: how late distributions come (higher = later; about 2.5 for buyout)"));
    pace_growth_ = alt_pm_spin(form, -50, 100, 13, 1, QStringLiteral("%"));
    pace_growth_->setToolTip(tr("Annual growth of the NAV"));
    pace_yield_ = alt_pm_spin(form, 0, 100, 0, 1, QStringLiteral("%"));
    pace_yield_->setToolTip(tr("Minimum annual distribution rate (income funds)"));
    pace_vintages_ = new QSpinBox(form);
    pace_vintages_->setRange(1, 20);
    pace_vintages_->setValue(1);
    pace_vintages_->setToolTip(tr("Commit the same amount every year for this many years (a programme)"));
    const QList<QPair<QString, QWidget*>> fields = {
        {tr("COMMITMENT PER FUND"), pace_commitment_}, {tr("FUND LIFE"), pace_life_},
        {tr("CALL RATES (%)"), pace_rates_},           {tr("BOW"), pace_bow_},
        {tr("NAV GROWTH"), pace_growth_},              {tr("YIELD"), pace_yield_},
        {tr("VINTAGES (YEARLY)"), pace_vintages_}};
    // Labels on even grid rows, inputs under them (four per line).
    for (int i = 0; i < fields.size(); ++i) {
        grid->addWidget(alt_wb_label(fields.at(i).first, "altWbFormLabel", form), (i / 4) * 2, i % 4);
        grid->addWidget(fields.at(i).second, (i / 4) * 2 + 1, i % 4);
    }
    pace_run_ = alt_wb_button(tr("RUN"), true, form);
    grid->addWidget(pace_run_, 3, 3, Qt::AlignBottom | Qt::AlignRight);
    vl->addWidget(form);

    pace_memory_ = new ui::InputMemory(QStringLiteral("alt_investments"), QStringLiteral("pm_pacing"), form);
    pace_memory_->track(QStringLiteral("commitment"), pace_commitment_);
    pace_memory_->track(QStringLiteral("life"), pace_life_);
    pace_memory_->track(QStringLiteral("call_rates"), pace_rates_);
    pace_memory_->track(QStringLiteral("bow"), pace_bow_);
    pace_memory_->track(QStringLiteral("growth"), pace_growth_);
    pace_memory_->track(QStringLiteral("yield"), pace_yield_);
    pace_memory_->track(QStringLiteral("vintages"), pace_vintages_);
    pace_memory_->restore();

    pace_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, page);
    vl->addWidget(pace_pane_, 1);
    QWidget* body = make_pane_body(pace_pane_);
    auto* rl = new QVBoxLayout(body);
    rl->setContentsMargins(10, 8, 10, 10);
    rl->setSpacing(8);
    pace_kpis_ = new ui::KpiStrip(body);
    alt_pm_tile(pace_kpis_, "peak", tr("Peak net outflow"));
    alt_pm_tile(pace_kpis_, "peak_year", tr("Peak in year"));
    alt_pm_tile(pace_kpis_, "breakeven", tr("Cash breakeven"));
    alt_pm_tile(pace_kpis_, "called", tr("Total called"));
    alt_pm_tile(pace_kpis_, "dist", tr("Total distributed"));
    alt_pm_tile(pace_kpis_, "multiple", tr("Multiple"));
    rl->addWidget(pace_kpis_);
    pace_notes_ = alt_wb_label(QString(), "altWbNote", body);
    pace_notes_->setWordWrap(true);
    rl->addWidget(pace_notes_);
    rl->addWidget(alt_wb_label(tr("PROJECTED CALLS AND DISTRIBUTIONS BY YEAR"), "altWbSection", body));
    pace_chart_ = new ui::BarChart(body);
    pace_chart_->set_mode(ui::BarChart::Mode::Grouped);
    pace_chart_->set_show_legend(true);
    pace_chart_->setMinimumHeight(240);
    rl->addWidget(pace_chart_);
    pace_model_ = new AltWbRowsModel(this);
    pace_table_ = alt_wb_make_table(body, pace_model_, QStringLiteral("pacing"));
    rl->addWidget(pace_table_);
    rl->addStretch(1);

    pace_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Result);
    pace_pane_->set_loading_text(tr("Projecting calls, distributions and NAV…"));
    pace_pane_->set_cache_key(QStringLiteral("alt_investments/pm_pacing"));
    pace_pane_->set_max_age(ui::autoload::kNeverRefresh);
    pace_pane_->set_status_label(pace_actions_.status);
    pace_pane_->set_timeout(60);
    pace_pane_->set_start([this](quint64 id) { start_pacing(id); });
    pace_pane_->set_cancel([this](quint64 id) { cancel_service(id); });
    pace_pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render_pacing(p); });
    pace_pane_->set_view(QStringLiteral("pacing"));
    pace_pane_->set_signature(pace_memory_->signature());
    connect(pace_memory_, &ui::InputMemory::changed, this,
            [this]() { pace_pane_->set_signature(pace_memory_->signature()); });
    connect(pace_run_, &QPushButton::clicked, pace_pane_, &ui::AutoLoadPane::run);

    connect(pace_actions_.copy, &QPushButton::clicked, this, [this]() { alt_wb_copy(pace_export_); });
    connect(pace_actions_.csv, &QPushButton::clicked, this,
            [this]() { alt_wb_save_csv(pace_export_, QStringLiteral("commitment_pacing"), this); });
    connect(pace_actions_.image, &QPushButton::clicked, this,
            [this]() { ui::ChartExport::save_png(pace_pane_->content(), QStringLiteral("commitment_pacing")); });
    pace_actions_.report->set_payload_provider(
        [this]() { return alt_wb_payload(pace_export_, {{tr("Calls and distributions"), pace_chart_}}); });
    return page;
}

QWidget* AltPrivateMarketsPanel::build_book_page(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    book_actions_ = alt_wb_make_actions(page, tr("THE WHOLE BOOK"));
    vl->addWidget(book_actions_.row);
    book_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, page);
    vl->addWidget(book_pane_, 1);
    QWidget* body = make_pane_body(book_pane_);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(10, 8, 10, 10);
    bl->setSpacing(8);
    book_stack_ = new QStackedWidget(body);
    bl->addWidget(book_stack_);
    book_empty_ = new ui::EmptyState(book_stack_);
    book_empty_->set_alignment(Qt::AlignTop);
    book_stack_->addWidget(book_empty_);
    connect(book_empty_, &ui::EmptyState::primary_clicked, this, &AltPrivateMarketsPanel::add_example_fund);
    connect(book_empty_, &ui::EmptyState::secondary_clicked, this, &AltPrivateMarketsPanel::add_fund);
    auto* res = new QWidget(book_stack_);
    auto* rl = new QVBoxLayout(res);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(8);
    book_kpis_ = new ui::KpiStrip(res);
    alt_pm_tile(book_kpis_, "irr", tr("Pooled IRR"));
    alt_pm_tile(book_kpis_, "tvpi", tr("Pooled TVPI"));
    alt_pm_tile(book_kpis_, "dpi", tr("Pooled DPI"));
    alt_pm_tile(book_kpis_, "nav", tr("NAV"));
    alt_pm_tile(book_kpis_, "unfunded", tr("Unfunded"));
    alt_pm_tile(book_kpis_, "commitment", tr("Committed"));
    rl->addWidget(book_kpis_);
    book_notes_ = alt_wb_label(QString(), "altWbNote", res);
    book_notes_->setWordWrap(true);
    rl->addWidget(book_notes_);
    auto* donuts = new QWidget(res);
    auto* dl = new QHBoxLayout(donuts);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->setSpacing(10);
    auto* sbox = new QWidget(donuts);
    auto* sl = new QVBoxLayout(sbox);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->addWidget(alt_wb_label(tr("COMMITMENT BY STRATEGY"), "altWbSection", sbox));
    book_strategy_ = new ui::DonutChart(sbox);
    book_strategy_->setMinimumHeight(220);
    sl->addWidget(book_strategy_);
    auto* vbox = new QWidget(donuts);
    auto* vbl = new QVBoxLayout(vbox);
    vbl->setContentsMargins(0, 0, 0, 0);
    vbl->addWidget(alt_wb_label(tr("COMMITMENT BY VINTAGE"), "altWbSection", vbox));
    book_vintage_ = new ui::DonutChart(vbox);
    book_vintage_->setMinimumHeight(220);
    vbl->addWidget(book_vintage_);
    dl->addWidget(sbox, 1);
    dl->addWidget(vbox, 1);
    rl->addWidget(donuts);
    book_model_ = new AltWbRowsModel(this);
    book_table_ = alt_wb_make_table(res, book_model_, QStringLiteral("fund_book_summary"));
    rl->addWidget(book_table_);
    rl->addStretch(1);
    book_stack_->addWidget(res);

    book_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Result);
    book_pane_->set_loading_text(tr("Pooling every fund in the book…"));
    book_pane_->set_cache_key(QStringLiteral("alt_investments/pm_book"));
    book_pane_->set_max_age(ui::autoload::kNeverRefresh);
    book_pane_->set_status_label(book_actions_.status);
    book_pane_->set_timeout(120);
    book_pane_->set_start([this](quint64 id) { start_book(id); });
    book_pane_->set_cancel([this](quint64 id) { cancel_service(id); });
    book_pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render_book(p); });
    book_pane_->set_view(QStringLiteral("book"));

    connect(book_actions_.copy, &QPushButton::clicked, this, [this]() { alt_wb_copy(book_export_); });
    connect(book_actions_.csv, &QPushButton::clicked, this,
            [this]() { alt_wb_save_csv(book_export_, QStringLiteral("fund_book"), this); });
    connect(book_actions_.image, &QPushButton::clicked, this,
            [this]() { ui::ChartExport::save_png(book_stack_, QStringLiteral("fund_book")); });
    book_actions_.report->set_payload_provider([this]() {
        return alt_wb_payload(book_export_, {{tr("By strategy"), book_strategy_}, {tr("By vintage"), book_vintage_}});
    });
    return page;
}

// ── Fund book ────────────────────────────────────────────────────────────────

void AltPrivateMarketsPanel::reload_funds(const QString& select_id) {
    AltFundStore::instance().list_funds(this, [this, select_id](const QVector<AltFund>& funds, const QString& error) {
        funds_loaded_ = true;
        store_error_ = error;
        funds_ = funds;
        show_funds();
        QString pick = select_id;
        if (pick.isEmpty() || std::none_of(funds_.cbegin(), funds_.cend(), [&](const AltFund& f) { return f.id == pick; })) {
            pick = current_id_;
            if (std::none_of(funds_.cbegin(), funds_.cend(), [&](const AltFund& f) { return f.id == pick; })) {
                // The most recently edited fund.
                pick.clear();
                qint64 best = -1;
                for (const AltFund& f : funds_)
                    if (f.updated_at > best) {
                        best = f.updated_at;
                        pick = f.id;
                    }
            }
        }
        select_fund(pick);
    });
}

void AltPrivateMarketsPanel::show_funds() {
    QVector<QStringList> rows;
    for (const AltFund& f : funds_)
        rows << QStringList{f.name, alt_pm_strategy_label(f.strategy),
                            f.vintage_year > 0 ? QString::number(f.vintage_year) : QStringLiteral("-"),
                            alt_wb_money(f.commitment, f.currency)};
    fund_model_->set_rows({tr("Fund"), tr("Strategy"), tr("Vintage"), tr("Commitment")}, rows, {2, 3});
    const int cw = QFontMetrics(ui::fonts::font(-1)).averageCharWidth();
    fund_view_->setColumnWidth(0, cw * 18);
    fund_view_->setColumnWidth(1, cw * 13);
    fund_view_->setColumnWidth(2, cw * 7);
    alt_wb_fit_table(fund_view_, 6);
    const bool empty = funds_.isEmpty();
    example_btn_->setVisible(empty && store_error_.isEmpty());
    if (!store_error_.isEmpty())
        book_hint_->setText(store_error_);
    else if (empty)
        book_hint_->setText(tr("No fund yet. ADD a fund and paste its cash flows, or start from the illustrative "
                               "example fund."));
    else
        book_hint_->setText(tr("%n fund(s). Cash flows stay on this computer (not synced).", nullptr,
                               static_cast<int>(funds_.size())));
    add_btn_->setEnabled(store_error_.isEmpty());
}

const AltFund* AltPrivateMarketsPanel::current_fund() const {
    for (const AltFund& f : funds_)
        if (f.id == current_id_)
            return &f;
    return nullptr;
}

void AltPrivateMarketsPanel::select_fund(const QString& id) {
    if (ledger_dirty_ && !loaded_ledger_id_.isEmpty() && loaded_ledger_id_ != id)
        flush_ledger_save(); // the previous fund's last edit is saved first
    current_id_ = id;
    const AltFund* f = current_fund();
    edit_btn_->setEnabled(f != nullptr);
    delete_btn_->setEnabled(f != nullptr);
    for (int r = 0; r < funds_.size(); ++r)
        if (funds_.at(r).id == id && fund_view_->currentIndex().row() != r) {
            const QSignalBlocker block(fund_view_->selectionModel());
            fund_view_->selectRow(r);
        }
    if (!f) {
        loaded_ledger_id_.clear();
        loading_ledger_ = true;
        ledger_->clear_rows();
        loading_ledger_ = false;
        ledger_->setEnabled(false);
        ledger_title_->setText(tr("CASH FLOWS"));
        set_ledger_status(QString());
        update_views();
        return;
    }
    ledger_title_->setText(tr("CASH FLOWS · %1").arg(f->name.toUpper()));
    set_ledger_status(tr("Loading…"));
    ledger_->setEnabled(false);
    pme_index_->set_text(f->benchmark.isEmpty() ? QStringLiteral("SPY") : f->benchmark);
    const quint64 req = ++ledger_request_;
    AltFundStore::instance().load_flows(id, this, [this, req](const QString& fund_id, const QJsonArray& rows,
                                                              const QString& error) {
        if (req != ledger_request_ || fund_id != current_id_)
            return; // another fund was picked meanwhile
        loading_ledger_ = true;
        // Amounts are in the fund's currency: one typed in another is refused, never converted.
        ledger_->set_column_currency(QStringLiteral("amount"), currency());
        ledger_->from_json(rows);
        loading_ledger_ = false;
        ledger_dirty_ = false;
        loaded_ledger_id_ = fund_id;
        ledger_->setEnabled(error.isEmpty());
        set_ledger_status(error.isEmpty() ? tr("%n flow(s)", nullptr, ledger_->valid_row_count()) : error, !error.isEmpty());
        update_views();
    });
}

bool AltPrivateMarketsPanel::edit_fund_dialog(AltFund* fund, const QString& title) {
    QDialog dlg(this);
    dlg.setWindowTitle(title);
    dlg.setObjectName(QStringLiteral("altWbRoot"));
    dlg.setStyleSheet(alt_wb_style());
    auto* fl = new QFormLayout(&dlg);
    auto* name = new QLineEdit(fund->name, &dlg);
    name->setPlaceholderText(tr("e.g. Example Partners Fund IV"));
    auto* strategy = new QComboBox(&dlg);
    for (const auto& s : alt_pm_strategies())
        strategy->addItem(s.second, s.first);
    const int si = strategy->findData(fund->strategy.isEmpty() ? QStringLiteral("buyout") : fund->strategy);
    strategy->setCurrentIndex(std::max(0, si));
    auto* vintage = new QSpinBox(&dlg);
    vintage->setRange(1980, QDate::currentDate().year() + 2);
    vintage->setValue(fund->vintage_year > 0 ? fund->vintage_year : QDate::currentDate().year());
    auto* commitment = alt_pm_spin(&dlg, 0, 1e13, fund->commitment, 0, QString());
    auto* ccy = new QLineEdit(fund->currency.isEmpty() ? QStringLiteral("USD") : fund->currency, &dlg);
    ccy->setMaxLength(3);
    ccy->setInputMask(QStringLiteral(">AAA"));
    auto* notes = new QLineEdit(fund->notes, &dlg);
    fl->addRow(tr("Name"), name);
    fl->addRow(tr("Strategy"), strategy);
    fl->addRow(tr("Vintage year"), vintage);
    fl->addRow(tr("Currency"), ccy);
    fl->addRow(tr("Commitment (in that currency)"), commitment);
    fl->addRow(tr("Notes"), notes);
    auto* err = alt_wb_label(QString(), "altWbError", &dlg);
    fl->addRow(err);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    fl->addRow(box);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, &dlg, [&]() {
        if (name->text().trimmed().isEmpty()) {
            err->setText(tr("Give the fund a name."));
            return;
        }
        dlg.accept();
    });
    dlg.setMinimumWidth(QFontMetrics(ui::fonts::font(-1)).averageCharWidth() * 56);
    if (dlg.exec() != QDialog::Accepted)
        return false;
    fund->name = name->text().trimmed();
    fund->strategy = strategy->currentData().toString();
    fund->vintage_year = vintage->value();
    fund->commitment = commitment->value();
    fund->currency = ccy->text().trimmed().toUpper();
    fund->notes = notes->text().trimmed();
    return true;
}

void AltPrivateMarketsPanel::add_fund() {
    if (!store_error_.isEmpty())
        return;
    AltFund f;
    f.strategy = QStringLiteral("buyout");
    if (!edit_fund_dialog(&f, tr("Add a fund")))
        return;
    AltFundStore::instance().save_fund(f, this, [this](const AltFund& saved, const QString& error) {
        if (!error.isEmpty()) {
            set_ledger_status(error, true);
            return;
        }
        reload_funds(saved.id);
    });
}

void AltPrivateMarketsPanel::add_example_fund() {
    if (!store_error_.isEmpty())
        return;
    AltFund f;
    f.name = tr("Example Buyout Fund (illustrative)");
    f.strategy = QStringLiteral("buyout");
    f.vintage_year = 2020;
    f.commitment = 2000000;
    f.currency = QStringLiteral("USD");
    f.benchmark = QStringLiteral("SPY");
    f.notes = tr("Illustrative sample cash flows - replace them with a real fund's, or delete this fund.");
    AltFundStore::instance().save_fund(f, this, [this](const AltFund& saved, const QString& error) {
        if (!error.isEmpty()) {
            set_ledger_status(error, true);
            return;
        }
        AltFundStore::instance().save_flows(saved.id, alt_pm_example_flows(), this,
                                            [this, id = saved.id](const QString&) { reload_funds(id); });
    });
}

void AltPrivateMarketsPanel::edit_fund() {
    const AltFund* cur = current_fund();
    if (!cur)
        return;
    AltFund f = *cur;
    if (!edit_fund_dialog(&f, tr("Edit fund")))
        return;
    AltFundStore::instance().save_fund(f, this, [this](const AltFund& saved, const QString& error) {
        if (!error.isEmpty()) {
            set_ledger_status(error, true);
            return;
        }
        for (AltFund& x : funds_)
            if (x.id == saved.id)
                x = saved;
        show_funds();
        select_fund(saved.id);
    });
}

void AltPrivateMarketsPanel::delete_fund() {
    const AltFund* cur = current_fund();
    if (!cur)
        return;
    const QString id = cur->id;
    const auto answer = QMessageBox::question(
        this, tr("Delete fund"),
        tr("Delete \"%1\" and all its cash flows from this computer? This cannot be undone.").arg(cur->name));
    if (answer != QMessageBox::Yes)
        return;
    save_timer_->stop();
    ledger_dirty_ = false;
    AltFundStore::instance().delete_fund(id, this, [this](const QString& error) {
        if (!error.isEmpty()) {
            set_ledger_status(error, true);
            return;
        }
        current_id_.clear();
        loaded_ledger_id_.clear();
        reload_funds();
    });
}

void AltPrivateMarketsPanel::on_ledger_changed() {
    if (loading_ledger_ || loaded_ledger_id_.isEmpty() || loaded_ledger_id_ != current_id_)
        return;
    ledger_dirty_ = true;
    set_ledger_status(tr("Editing…"));
    save_timer_->start();
}

void AltPrivateMarketsPanel::flush_ledger_save() {
    save_timer_->stop();
    if (!ledger_dirty_ || loaded_ledger_id_.isEmpty())
        return;
    ledger_dirty_ = false;
    const QString id = loaded_ledger_id_;
    const int n = ledger_->valid_row_count();
    const int bad = ledger_->invalid_row_count();
    set_ledger_status(tr("Saving…"));
    AltFundStore::instance().save_flows(id, ledger_->to_json(), this, [this, id, n, bad](const QString& error) {
        if (id != current_id_)
            return;
        if (!error.isEmpty()) {
            ledger_dirty_ = true;
            set_ledger_status(error, true);
            return;
        }
        set_ledger_status(bad > 0 ? tr("Saved %1 flow(s) · %2 row(s) not valid, not saved").arg(n).arg(bad)
                                  : tr("Saved · %n flow(s)", nullptr, n));
        update_views();
        // The user's own edit: recompute the visible local view now.
        if (tabs_->currentIndex() == 0 && perf_pane_->isVisible())
            perf_pane_->run();
    });
}

void AltPrivateMarketsPanel::set_ledger_status(const QString& text, bool error) {
    ledger_status_->setObjectName(error ? QStringLiteral("altWbError") : QStringLiteral("altWbStatus"));
    ledger_status_->style()->unpolish(ledger_status_);
    ledger_status_->style()->polish(ledger_status_);
    ledger_status_->setText(text);
    ledger_status_->setToolTip(text);
}

// ── Views ────────────────────────────────────────────────────────────────────

QJsonObject AltPrivateMarketsPanel::ledger_input() const {
    QJsonObject in;
    in.insert(QStringLiteral("flows"), ledger_->to_json());
    if (const AltFund* f = current_fund())
        if (f->commitment > 0)
            in.insert(QStringLiteral("commitment"), f->commitment);
    return in;
}

QString AltPrivateMarketsPanel::ledger_signature() const {
    return ui::autoload::signature_of(QJsonValue(ledger_input()));
}

QString AltPrivateMarketsPanel::pme_symbol() const {
    QString s = pme_index_ ? pme_index_->text().trimmed().toUpper() : QString();
    return s.isEmpty() ? QStringLiteral("SPY") : s;
}

void AltPrivateMarketsPanel::update_views() {
    const bool ready = current_fund() && loaded_ledger_id_ == current_id_;
    const QString fund = ready ? current_id_ : QStringLiteral("none");
    const QString sig = ready ? ledger_signature() : QString();
    perf_pane_->set_view(QStringLiteral("perf/") + fund);
    perf_pane_->set_signature(sig);
    configure_pme_preview();
    pme_pane_->set_view(QStringLiteral("pme/") + fund + QLatin1Char('/') + pme_symbol(),
                        ready ? ui::AutoLoadPane::Policy::Manual : ui::AutoLoadPane::Policy::AutoRun);
    pme_pane_->set_signature(sig);
    pme_run_->setEnabled(ready);
}

void AltPrivateMarketsPanel::configure_pme_preview() {
    ui::EmptyState* pv = pme_pane_->preview();
    pv->apply(ui::EmptyState::preview_spec(
        tr("Public market equivalent"),
        tr("Compares this fund with the same cash flows invested in %1: Kaplan-Schoar PME, Long-Nickels PME IRR, PME+ "
           "and Direct Alpha. Not run automatically: it downloads the index's price history from Yahoo Finance.")
            .arg(pme_symbol()),
        tr("RUN PME"), tr("Yahoo Finance prices · no credits"), ui::EmptyState::Tone::Info,
        ui::LoadingSkeleton::Shape::Result));
    if (const AltFund* f = current_fund())
        pv->set_details({{tr("Fund"), f->name}, {tr("Index"), pme_symbol()}}, tr("WHAT WILL BE COMPARED"));
}

void AltPrivateMarketsPanel::cancel_service(quint64 pane_request) {
    const auto it = service_ids_.find(pane_request);
    if (it == service_ids_.end())
        return;
    AltInvestmentsService::instance().cancel(it.value());
    service_ids_.erase(it);
}

void AltPrivateMarketsPanel::start_performance(quint64 id) {
    if (!funds_loaded_)
        return; // the book is still loading: the skeleton stays until the fund's view replaces this one
    const AltFund* f = current_fund();
    if (!f || loaded_ledger_id_ != current_id_) {
        // Nothing to compute: the designed "no fund" page is the result of this view.
        perf_pane_->deliver(id, QJsonObject{{QStringLiteral("empty"), true},
                                            {QStringLiteral("store_error"), store_error_}});
        return;
    }
    AltCall c;
    c.script = services::alt::kPrivateMarketsScript;
    c.command = QStringLiteral("timeline");
    c.input = ledger_input();
    c.timeout_ms = 60 * 1000;
    QPointer<ui::AutoLoadPane> pane = perf_pane_;
    const quint64 sid = AltInvestmentsService::instance().call(c, this, [this, id, pane](const AltReply& r) {
        service_ids_.remove(id);
        if (!pane || r.cancelled)
            return;
        if (!r.ok) {
            pane->fail(id, r.error, tr("Could not compute the fund's performance"));
            return;
        }
        QJsonObject d = r.data;
        d.insert(QStringLiteral("currency"), currency());
        pane->deliver(id, d);
    });
    service_ids_.insert(id, sid);
}

void AltPrivateMarketsPanel::render_performance(const QJsonValue& payload) {
    const QJsonObject d = payload.toObject();
    if (d.value(QStringLiteral("empty")).toBool()) {
        const QString err = d.value(QStringLiteral("store_error")).toString();
        if (!err.isEmpty()) {
            perf_empty_->set_content(QStringLiteral("!"), tr("The fund book is not available"), err);
            perf_empty_->set_primary_action(QString());
            perf_empty_->set_secondary_action(QString());
        } else {
            perf_empty_->set_content(QStringLiteral("◇"), tr("No fund selected"),
                                     tr("Add a fund and its dated capital calls, distributions and NAV statements to "
                                        "see IRR, TVPI, DPI and the J-curve. The example fund is illustrative."),
                                     tr("ADD EXAMPLE FUND"), tr("Cash flows stay on this computer"),
                                     ui::EmptyState::Tone::Info);
            perf_empty_->set_secondary_action(tr("ADD FUND"));
        }
        perf_stack_->setCurrentIndex(0);
        perf_export_ = AltWbExport{};
        alt_wb_set_actions_enabled(perf_actions_, false);
        return;
    }
    perf_stack_->setCurrentIndex(1);
    const QString ccy = d.value(QStringLiteral("currency")).toString();
    const QJsonObject m = d.value(QStringLiteral("metrics")).toObject();
    auto money = [&ccy](const QJsonValue& v) { return alt_wb_money(alt_wb_num(v), ccy); };
    const double irr = alt_wb_num(m.value(QStringLiteral("irr")));
    alt_pm_set(perf_kpis_, "irr", alt_wb_pct(irr),
               tr("to %1").arg(m.value(QStringLiteral("valuation_date")).toString()));
    alt_pm_set(perf_kpis_, "tvpi", alt_wb_multiple(alt_wb_num(m.value(QStringLiteral("tvpi")))), tr("total value / paid-in"));
    alt_pm_set(perf_kpis_, "dpi", alt_wb_multiple(alt_wb_num(m.value(QStringLiteral("dpi")))), tr("distributed / paid-in"));
    alt_pm_set(perf_kpis_, "rvpi", alt_wb_multiple(alt_wb_num(m.value(QStringLiteral("rvpi")))), tr("NAV / paid-in"));
    const double paid_pct = alt_wb_num(m.value(QStringLiteral("paid_in_pct")));
    alt_pm_set(perf_kpis_, "paid", money(m.value(QStringLiteral("paid_in"))),
               std::isfinite(paid_pct) ? tr("%1 of commitment").arg(alt_wb_pct(paid_pct, 1)) : QString());
    alt_pm_set(perf_kpis_, "dist", money(m.value(QStringLiteral("distributed"))));
    alt_pm_set(perf_kpis_, "nav", money(m.value(QStringLiteral("nav"))),
               m.value(QStringLiteral("nav_date")).isString()
                   ? tr("statement %1").arg(m.value(QStringLiteral("nav_date")).toString())
                   : tr("no NAV entered"));
    alt_pm_set(perf_kpis_, "unfunded", money(m.value(QStringLiteral("unfunded"))),
               m.contains(QStringLiteral("unfunded")) ? QString() : tr("set the commitment"));
    QStringList notes = alt_pm_notes(m.value(QStringLiteral("notes")));
    const int ignored = d.value(QStringLiteral("ignored_rows")).toInt();
    if (ignored > 0)
        notes << tr("%n ledger row(s) were not valid and were left out.", nullptr, ignored);
    perf_notes_->setText(notes.join(QLatin1Char('\n')));
    perf_notes_->setVisible(!notes.isEmpty());

    // J-curve
    jcurve_->clear();
    const QJsonArray series = d.value(QStringLiteral("series")).toArray();
    QVector<double> xs;
    QVector<double> paid, dist, nav, net;
    for (const QJsonValue& v : series) {
        const QJsonObject p = v.toObject();
        const double x = alt_wb_date_ms(p.value(QStringLiteral("date")).toString());
        if (!std::isfinite(x))
            continue;
        xs << x;
        paid << alt_wb_num(p.value(QStringLiteral("paid_in")));
        dist << alt_wb_num(p.value(QStringLiteral("distributed")));
        nav << alt_wb_num(p.value(QStringLiteral("nav")));
        net << alt_wb_num(p.value(QStringLiteral("net_cash")));
    }
    auto add = [this, &xs](const QString& name, const QVector<double>& ys, ui::TimeSeriesChart::Style style,
                           ui::ChartColorRole role) {
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = name;
        s.xs = xs;
        s.ys = ys;
        s.style = style;
        s.color = role;
        s.rebase = false;
        if (style == ui::TimeSeriesChart::Style::Area)
            s.baseline = 0.0;
        jcurve_->add_series(s);
    };
    add(tr("Net cash (distributed - paid-in)"), net, ui::TimeSeriesChart::Style::Area, ui::ChartColorRole::Accent);
    add(tr("Paid-in"), paid, ui::TimeSeriesChart::Style::Step, ui::ChartColorRole::Auto);
    add(tr("Distributed"), dist, ui::TimeSeriesChart::Style::Step, ui::ChartColorRole::Auto);
    add(tr("NAV"), nav, ui::TimeSeriesChart::Style::Line, ui::ChartColorRole::Auto);
    jcurve_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [ccy](double v) { return alt_wb_money(v, ccy); });

    // Year-end table
    QVector<QStringList> rows;
    QVector<QStringList> tones;
    for (const QJsonValue& v : d.value(QStringLiteral("year_end")).toArray()) {
        const QJsonObject r = v.toObject();
        const double ri = alt_wb_num(r.value(QStringLiteral("irr")));
        rows << QStringList{QString::number(r.value(QStringLiteral("year")).toInt()),
                            money(r.value(QStringLiteral("paid_in"))), money(r.value(QStringLiteral("distributed"))),
                            money(r.value(QStringLiteral("nav"))),
                            alt_wb_multiple(alt_wb_num(r.value(QStringLiteral("dpi")))),
                            alt_wb_multiple(alt_wb_num(r.value(QStringLiteral("tvpi")))), alt_wb_pct(ri)};
        tones << QStringList{QString(), QString(), QString(), QString(), QString(), QString(),
                             std::isfinite(ri) ? (ri < 0 ? QStringLiteral("bad") : QString()) : QStringLiteral("dim")};
    }
    const QStringList headers = {tr("Year end"), ui::formatting::header(tr("Paid-in"), ccy),
                                 ui::formatting::header(tr("Distributed"), ccy), ui::formatting::header(tr("NAV"), ccy),
                                 tr("DPI"), tr("TVPI"), tr("IRR")};
    perf_model_->set_rows(headers, rows, {1, 2, 3, 4, 5, 6}, tones);
    alt_wb_fit_table(perf_table_, 20);

    // Export
    perf_export_ = AltWbExport{};
    const AltFund* f = current_fund();
    perf_export_.title = tr("Fund performance: %1").arg(f ? f->name : QString());
    perf_export_.figures = {{tr("IRR (dated)"), alt_wb_pct(irr)},
                            {tr("TVPI"), alt_wb_multiple(alt_wb_num(m.value(QStringLiteral("tvpi"))))},
                            {tr("DPI"), alt_wb_multiple(alt_wb_num(m.value(QStringLiteral("dpi"))))},
                            {tr("RVPI"), alt_wb_multiple(alt_wb_num(m.value(QStringLiteral("rvpi"))))},
                            {tr("Paid-in"), money(m.value(QStringLiteral("paid_in")))},
                            {tr("Distributed"), money(m.value(QStringLiteral("distributed")))},
                            {tr("NAV"), money(m.value(QStringLiteral("nav")))},
                            {tr("Unfunded"), money(m.value(QStringLiteral("unfunded")))},
                            {tr("Valuation date"), m.value(QStringLiteral("valuation_date")).toString()}};
    if (f) {
        perf_export_.inputs = {{tr("Fund"), f->name},
                               {tr("Strategy"), alt_pm_strategy_label(f->strategy)},
                               {tr("Vintage"), QString::number(f->vintage_year)},
                               {tr("Commitment"), alt_wb_money(f->commitment, f->currency, false)},
                               {tr("Cash flows"), QString::number(m.value(QStringLiteral("flow_count")).toInt())}};
    }
    perf_export_.headers = headers;
    perf_export_.rows = rows;
    perf_export_.notes = notes.join(QLatin1Char(' ')) +
                         (notes.isEmpty() ? QString() : QStringLiteral(" ")) +
                         tr("IRR: dated cash flows, actual/365 (Excel XIRR convention).");
    alt_wb_set_actions_enabled(perf_actions_, true);
}

void AltPrivateMarketsPanel::start_pme(quint64 id) {
    if (!funds_loaded_)
        return;
    const AltFund* f = current_fund();
    if (!f || loaded_ledger_id_ != current_id_) {
        pme_pane_->deliver(id, QJsonObject{{QStringLiteral("empty"), true}});
        return;
    }
    AltCall c;
    c.script = services::alt::kPrivateMarketsScript;
    c.command = QStringLiteral("pme");
    c.input = ledger_input();
    c.input.insert(QStringLiteral("index"), pme_symbol());
    c.timeout_ms = 90 * 1000;
    c.source = QStringLiteral("Yahoo Finance");
    QPointer<ui::AutoLoadPane> pane = pme_pane_;
    const QString sym = pme_symbol();
    const quint64 sid = AltInvestmentsService::instance().call(c, this, [this, id, pane, sym](const AltReply& r) {
        service_ids_.remove(id);
        if (!pane || r.cancelled)
            return;
        if (!r.ok) {
            QString msg = r.error;
            if (!msg.contains(sym))
                msg = tr("Could not load prices for %1 - PME not shown; fund metrics above are unaffected. (%2)")
                          .arg(sym, r.error);
            pane->fail(id, msg, tr("PME not available"));
            return;
        }
        QJsonObject d = r.data;
        d.insert(QStringLiteral("currency"), currency());
        pane->deliver(id, d);
    });
    service_ids_.insert(id, sid);
}

void AltPrivateMarketsPanel::render_pme(const QJsonValue& payload) {
    const QJsonObject d = payload.toObject();
    if (d.value(QStringLiteral("empty")).toBool()) {
        pme_empty_->set_content(QStringLiteral("◇"), tr("No fund selected"),
                                tr("Pick a fund with cash flows (or add the illustrative example fund) to compare it "
                                   "with a public index."),
                                store_error_.isEmpty() && funds_.isEmpty() ? tr("ADD EXAMPLE FUND") : QString());
        pme_stack_->setCurrentIndex(0);
        pme_export_ = AltWbExport{};
        alt_wb_set_actions_enabled(pme_actions_, false);
        return;
    }
    pme_stack_->setCurrentIndex(1);
    const QString ccy = d.value(QStringLiteral("currency")).toString();
    const QString index = d.value(QStringLiteral("index")).toString();
    const double ks = alt_wb_num(d.value(QStringLiteral("ks_pme")));
    const double alpha = alt_wb_num(d.value(QStringLiteral("direct_alpha")));
    const double fund_irr = alt_wb_num(d.value(QStringLiteral("fund_irr")));
    const double ln = alt_wb_num(d.value(QStringLiteral("ln_pme_irr")));
    const double plus = alt_wb_num(d.value(QStringLiteral("pme_plus_irr")));
    const double idx = alt_wb_num(d.value(QStringLiteral("index_return")));
    alt_pm_set(pme_kpis_, "ks", alt_wb_multiple(ks, 2),
               std::isfinite(ks) ? (ks >= 1.0 ? tr("fund beat %1").arg(index) : tr("fund lagged %1").arg(index)) : QString());
    alt_pm_set(pme_kpis_, "alpha", alt_wb_pct(alpha, 2, true), tr("a year over %1").arg(index));
    alt_pm_set(pme_kpis_, "fund", alt_wb_pct(fund_irr));
    alt_pm_set(pme_kpis_, "ln", alt_wb_pct(ln));
    alt_pm_set(pme_kpis_, "plus", alt_wb_pct(plus));
    alt_pm_set(pme_kpis_, "index", alt_wb_pct(idx, 1, true),
               tr("%1 to %2").arg(d.value(QStringLiteral("index_first_price_date")).toString(),
                                  d.value(QStringLiteral("valuation_date")).toString()));
    QStringList notes = alt_pm_notes(d.value(QStringLiteral("notes")));
    notes.prepend(tr("Index: %1, %2.").arg(index, d.value(QStringLiteral("index_source")).toString()));
    pme_notes_->setText(notes.join(QLatin1Char('\n')));

    pme_chart_->clear();
    QVector<double> xs, fund, eq, dist;
    for (const QJsonValue& v : d.value(QStringLiteral("series")).toArray()) {
        const QJsonObject p = v.toObject();
        const double x = alt_wb_date_ms(p.value(QStringLiteral("date")).toString());
        if (!std::isfinite(x))
            continue;
        xs << x;
        const double nav = alt_wb_num(p.value(QStringLiteral("fund_nav")));
        const double dd = alt_wb_num(p.value(QStringLiteral("distributed")));
        fund << (std::isfinite(nav) ? nav + (std::isfinite(dd) ? dd : 0.0) : kAltWbNaN);
        eq << alt_wb_num(p.value(QStringLiteral("index_equivalent")));
        dist << dd;
    }
    ui::TimeSeriesChart::SeriesSpec a;
    a.name = tr("Fund: NAV + distributions to date");
    a.xs = xs;
    a.ys = fund;
    a.rebase = false;
    pme_chart_->add_series(a);
    ui::TimeSeriesChart::SeriesSpec b;
    b.name = tr("Same calls in %1, net of distributions").arg(index);
    b.xs = xs;
    b.ys = eq;
    b.rebase = false;
    b.dashed = true;
    pme_chart_->add_series(b);
    pme_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [ccy](double v) { return alt_wb_money(v, ccy); });

    const QStringList headers = {tr("Measure"), tr("Value"), tr("What it says")};
    QVector<QStringList> rows = {
        {tr("KS-PME"), alt_wb_multiple(ks, 3),
         tr("Index-compounded distributions plus NAV over index-compounded calls; above 1 = the fund beat the index")},
        {tr("Direct Alpha"), alt_wb_pct(alpha, 2, true),
         tr("Annual excess return over the index (Gredil, Griffiths & Stucke 2014)")},
        {tr("Fund IRR"), alt_wb_pct(fund_irr), tr("The fund's own dated IRR")},
        {tr("Long-Nickels PME IRR"), alt_wb_pct(ln), tr("IRR had the calls bought the index and distributions sold it")},
        {tr("PME+ IRR"), alt_wb_pct(plus), tr("Distributions scaled so the index position ends at the fund's NAV")},
        {tr("Index total return"), alt_wb_pct(idx, 1, true), tr("Over the fund's life")}};
    pme_model_->set_rows(headers, rows, {1});
    pme_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    pme_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    alt_wb_fit_table(pme_table_, 8);

    pme_export_ = AltWbExport{};
    const AltFund* f = current_fund();
    pme_export_.title = tr("Public market equivalent: %1 vs %2").arg(f ? f->name : QString(), index);
    for (const QStringList& r : rows)
        pme_export_.figures.append({r.value(0), r.value(1)});
    pme_export_.inputs = {{tr("Index"), index}, {tr("Valuation date"), d.value(QStringLiteral("valuation_date")).toString()}};
    pme_export_.notes = notes.join(QLatin1Char(' '));
    alt_wb_set_actions_enabled(pme_actions_, true);
}

QJsonObject AltPrivateMarketsPanel::pacing_input(QString* problem) const {
    QJsonArray rates;
    static const QRegularExpression sep(QStringLiteral("[,;\\s]+"));
    for (const QString& part : pace_rates_->text().split(sep, Qt::SkipEmptyParts)) {
        double v = 0;
        if (!ui::DataGridEditor::parse_number(part, &v) || v < 0 || v > 100) {
            if (problem)
                *problem = tr("Call rates are percentages between 0 and 100, separated by commas (e.g. 25, 33.3, 50).");
            return {};
        }
        rates.append(v / 100.0);
    }
    if (rates.isEmpty()) {
        if (problem)
            *problem = tr("Enter at least one call rate (e.g. 25, 33.3, 50).");
        return {};
    }
    return QJsonObject{{QStringLiteral("commitment"), pace_commitment_->value()},
                       {QStringLiteral("life_years"), pace_life_->value()},
                       {QStringLiteral("call_rates"), rates},
                       {QStringLiteral("bow"), pace_bow_->value()},
                       {QStringLiteral("growth"), pace_growth_->value() / 100.0},
                       {QStringLiteral("yield"), pace_yield_->value() / 100.0},
                       {QStringLiteral("vintages"), pace_vintages_->value()}};
}

void AltPrivateMarketsPanel::start_pacing(quint64 id) {
    QString problem;
    const QJsonObject in = pacing_input(&problem);
    if (!problem.isEmpty()) {
        pace_pane_->fail(id, problem, tr("Check the inputs"));
        return;
    }
    AltCall c;
    c.script = services::alt::kPrivateMarketsScript;
    c.command = QStringLiteral("pacing");
    c.input = in;
    c.timeout_ms = 60 * 1000;
    QPointer<ui::AutoLoadPane> pane = pace_pane_;
    const quint64 sid = AltInvestmentsService::instance().call(c, this, [this, id, pane](const AltReply& r) {
        service_ids_.remove(id);
        if (!pane || r.cancelled)
            return;
        r.ok ? pane->deliver(id, r.data) : pane->fail(id, r.error, tr("Could not project the commitments"));
    });
    service_ids_.insert(id, sid);
}

void AltPrivateMarketsPanel::render_pacing(const QJsonValue& payload) {
    const QJsonObject d = payload.toObject();
    const QString ccy; // pacing amounts are in the display currency the user typed them in
    auto money = [&ccy](double v) { return alt_wb_money(v, ccy); };
    const QJsonValue peak_year = d.value(QStringLiteral("peak_year"));
    const QJsonValue be = d.value(QStringLiteral("breakeven_year"));
    alt_pm_set(pace_kpis_, "peak", money(alt_wb_num(d.value(QStringLiteral("peak_net_outflow")))),
               tr("largest cumulative net cash out"));
    alt_pm_set(pace_kpis_, "peak_year", peak_year.isDouble() ? tr("Year %1").arg(peak_year.toInt()) : QStringLiteral("-"));
    alt_pm_set(pace_kpis_, "breakeven", be.isDouble() ? tr("Year %1").arg(be.toInt()) : tr("not reached"),
               tr("cumulative net cash back to zero"));
    alt_pm_set(pace_kpis_, "called", money(alt_wb_num(d.value(QStringLiteral("total_called")))));
    alt_pm_set(pace_kpis_, "dist", money(alt_wb_num(d.value(QStringLiteral("total_distributed")))));
    alt_pm_set(pace_kpis_, "multiple", alt_wb_multiple(alt_wb_num(d.value(QStringLiteral("multiple")))),
               tr("(distributions + final NAV) / calls"));
    pace_notes_->setText(d.value(QStringLiteral("method")).toString());

    QStringList cats;
    QVector<double> calls, dists;
    QVector<QStringList> rows;
    for (const QJsonValue& v : d.value(QStringLiteral("yearly")).toArray()) {
        const QJsonObject r = v.toObject();
        const int y = r.value(QStringLiteral("year")).toInt();
        cats << tr("Y%1").arg(y);
        calls << -alt_wb_num(r.value(QStringLiteral("calls")));
        dists << alt_wb_num(r.value(QStringLiteral("distributions")));
        rows << QStringList{QString::number(y), money(alt_wb_num(r.value(QStringLiteral("calls")))),
                            money(alt_wb_num(r.value(QStringLiteral("distributions")))),
                            money(alt_wb_num(r.value(QStringLiteral("net_cash")))),
                            money(alt_wb_num(r.value(QStringLiteral("cumulative_net")))),
                            money(alt_wb_num(r.value(QStringLiteral("nav")))),
                            money(alt_wb_num(r.value(QStringLiteral("unfunded"))))};
    }
    pace_chart_->set_categories(cats);
    ui::BarSeries sc;
    sc.name = tr("Calls (out)");
    sc.values = calls;
    ui::BarSeries sd;
    sd.name = tr("Distributions (in)");
    sd.values = dists;
    pace_chart_->set_series({sc, sd});
    pace_chart_->set_value_formatter([money](double v) { return money(v); });
    const QStringList headers = {tr("Year"), tr("Calls"), tr("Distributions"), tr("Net cash"), tr("Cumulative"),
                                 tr("NAV"), tr("Unfunded")};
    pace_model_->set_rows(headers, rows, {1, 2, 3, 4, 5, 6});
    alt_wb_fit_table(pace_table_, 16);

    pace_export_ = AltWbExport{};
    pace_export_.title = tr("Commitment pacing (Takahashi-Alexander)");
    pace_export_.figures = {{tr("Peak net outflow"), money(alt_wb_num(d.value(QStringLiteral("peak_net_outflow"))))},
                            {tr("Peak in year"), peak_year.isDouble() ? QString::number(peak_year.toInt()) : QStringLiteral("-")},
                            {tr("Cash breakeven year"), be.isDouble() ? QString::number(be.toInt()) : tr("not reached")},
                            {tr("Total called"), money(alt_wb_num(d.value(QStringLiteral("total_called"))))},
                            {tr("Total distributed"), money(alt_wb_num(d.value(QStringLiteral("total_distributed"))))},
                            {tr("Multiple"), alt_wb_multiple(alt_wb_num(d.value(QStringLiteral("multiple"))))}};
    pace_export_.inputs = {{tr("Commitment per fund"), money(pace_commitment_->value())},
                           {tr("Fund life (years)"), QString::number(pace_life_->value())},
                           {tr("Call rates (%)"), pace_rates_->text()},
                           {tr("Bow"), QString::number(pace_bow_->value())},
                           {tr("NAV growth"), alt_wb_pct(pace_growth_->value() / 100.0, 1)},
                           {tr("Yield"), alt_wb_pct(pace_yield_->value() / 100.0, 1)},
                           {tr("Vintages"), QString::number(pace_vintages_->value())}};
    pace_export_.headers = headers;
    pace_export_.rows = rows;
    pace_export_.notes = d.value(QStringLiteral("method")).toString();
    alt_wb_set_actions_enabled(pace_actions_, true);
}

void AltPrivateMarketsPanel::start_book(quint64 id) {
    QPointer<ui::AutoLoadPane> pane = book_pane_;
    AltFundStore::instance().load_book(this, [this, id, pane](const QVector<AltFund>& funds,
                                                               const QHash<QString, QJsonArray>& flows,
                                                               const QString& error) {
        if (!pane || !pane->is_current(id))
            return;
        if (!error.isEmpty()) {
            pane->deliver(id, QJsonObject{{QStringLiteral("empty"), true}, {QStringLiteral("store_error"), error}});
            return;
        }
        if (funds.isEmpty()) {
            pane->deliver(id, QJsonObject{{QStringLiteral("empty"), true}});
            return;
        }
        QJsonArray in;
        for (const AltFund& f : funds)
            in.append(QJsonObject{{QStringLiteral("id"), f.id},
                                  {QStringLiteral("name"), f.name},
                                  {QStringLiteral("strategy"), f.strategy},
                                  {QStringLiteral("vintage"), f.vintage_year},
                                  {QStringLiteral("commitment"), f.commitment},
                                  {QStringLiteral("currency"), f.currency},
                                  {QStringLiteral("flows"), flows.value(f.id)}});
        AltCall c;
        c.script = services::alt::kPrivateMarketsScript;
        c.command = QStringLiteral("book");
        c.input = QJsonObject{{QStringLiteral("funds"), in}};
        c.timeout_ms = 90 * 1000;
        const quint64 sid = AltInvestmentsService::instance().call(c, this, [this, id, pane](const AltReply& r) {
            service_ids_.remove(id);
            if (!pane || r.cancelled)
                return;
            r.ok ? pane->deliver(id, r.data) : pane->fail(id, r.error, tr("Could not pool the book"));
        });
        service_ids_.insert(id, sid);
    });
}

void AltPrivateMarketsPanel::render_book(const QJsonValue& payload) {
    const QJsonObject d = payload.toObject();
    if (d.value(QStringLiteral("empty")).toBool()) {
        const QString err = d.value(QStringLiteral("store_error")).toString();
        if (!err.isEmpty()) {
            book_empty_->set_content(QStringLiteral("!"), tr("The fund book is not available"), err);
            book_empty_->set_primary_action(QString());
            book_empty_->set_secondary_action(QString());
        } else {
            book_empty_->set_content(QStringLiteral("◇"), tr("The book is empty"),
                                     tr("Add funds and their cash flows to see pooled IRR and TVPI, unfunded "
                                        "commitments and the split by strategy and vintage."),
                                     tr("ADD EXAMPLE FUND"), tr("The example fund is illustrative"),
                                     ui::EmptyState::Tone::Info);
            book_empty_->set_secondary_action(tr("ADD FUND"));
        }
        book_stack_->setCurrentIndex(0);
        book_export_ = AltWbExport{};
        alt_wb_set_actions_enabled(book_actions_, false);
        return;
    }
    book_stack_->setCurrentIndex(1);
    const QJsonArray pooled = d.value(QStringLiteral("pooled")).toArray();
    const QJsonObject p = pooled.isEmpty() ? QJsonObject() : pooled.first().toObject();
    const QString ccy = p.value(QStringLiteral("currency")).toString();
    alt_pm_set(book_kpis_, "irr", alt_wb_pct(alt_wb_num(p.value(QStringLiteral("irr")))),
               pooled.size() > 1 ? tr("%1 funds only").arg(ccy) : QString());
    alt_pm_set(book_kpis_, "tvpi", alt_wb_multiple(alt_wb_num(p.value(QStringLiteral("tvpi")))));
    alt_pm_set(book_kpis_, "dpi", alt_wb_multiple(alt_wb_num(p.value(QStringLiteral("dpi")))));
    alt_pm_set(book_kpis_, "nav", alt_wb_money(alt_wb_num(p.value(QStringLiteral("nav"))), ccy));
    alt_pm_set(book_kpis_, "unfunded", alt_wb_money(alt_wb_num(p.value(QStringLiteral("unfunded"))), ccy));
    alt_pm_set(book_kpis_, "commitment", alt_wb_money(alt_wb_num(p.value(QStringLiteral("commitment"))), ccy),
               tr("%n fund(s)", nullptr, p.value(QStringLiteral("funds")).toInt()));
    QStringList notes = alt_pm_notes(d.value(QStringLiteral("notes")));
    if (!p.value(QStringLiteral("irr_note")).toString().isEmpty())
        notes << p.value(QStringLiteral("irr_note")).toString();
    // Funds in several currencies are never added together: the tiles show the first currency's funds and
    // every other currency is listed here in its own money.
    for (int i = 1; i < pooled.size(); ++i) {
        const QJsonObject o = pooled.at(i).toObject();
        const QString c = o.value(QStringLiteral("currency")).toString();
        notes << tr("%1 funds (%2): IRR %3, TVPI %4, NAV %5, unfunded %6, committed %7")
                     .arg(c)
                     .arg(o.value(QStringLiteral("funds")).toInt())
                     .arg(alt_wb_pct(alt_wb_num(o.value(QStringLiteral("irr")))),
                          alt_wb_multiple(alt_wb_num(o.value(QStringLiteral("tvpi")))),
                          alt_wb_money(alt_wb_num(o.value(QStringLiteral("nav"))), c),
                          alt_wb_money(alt_wb_num(o.value(QStringLiteral("unfunded"))), c),
                          alt_wb_money(alt_wb_num(o.value(QStringLiteral("commitment"))), c));
    }
    book_notes_->setText(notes.join(QLatin1Char('\n')));
    book_notes_->setVisible(!notes.isEmpty());

    // Shares of a whole only make sense in one currency: the donuts show the first currency's funds.
    auto slices = [ccy](const QJsonArray& groups, const char* key, bool label_strategy) {
        QVector<ui::DonutSlice> out;
        for (const QJsonValue& v : groups) {
            const QJsonObject g = v.toObject();
            const QString c = g.value(QStringLiteral("currency")).toString();
            if (c != ccy)
                continue;
            ui::DonutSlice s;
            const QString k = g.value(QLatin1String(key)).toVariant().toString();
            s.id = c + QLatin1Char('/') + k;
            s.label = label_strategy ? alt_pm_strategy_label(k) : k;
            s.value = g.value(QStringLiteral("commitment")).toDouble();
            out << s;
        }
        return out;
    };
    book_strategy_->set_center_text(ccy);
    book_vintage_->set_center_text(ccy);
    book_strategy_->set_slices(slices(d.value(QStringLiteral("by_strategy")).toArray(), "strategy", true));
    book_vintage_->set_slices(slices(d.value(QStringLiteral("by_vintage")).toArray(), "vintage", false));

    QVector<QStringList> rows;
    QVector<QStringList> tones;
    for (const QJsonValue& v : d.value(QStringLiteral("funds")).toArray()) {
        const QJsonObject f = v.toObject();
        const QString c = f.value(QStringLiteral("currency")).toString();
        const double irr = alt_wb_num(f.value(QStringLiteral("irr")));
        rows << QStringList{f.value(QStringLiteral("name")).toString(),
                            alt_pm_strategy_label(f.value(QStringLiteral("strategy")).toString()),
                            f.value(QStringLiteral("vintage")).toInt() > 0
                                ? QString::number(f.value(QStringLiteral("vintage")).toInt())
                                : QStringLiteral("-"),
                            c,
                            alt_wb_money(alt_wb_num(f.value(QStringLiteral("commitment"))), c),
                            alt_wb_money(alt_wb_num(f.value(QStringLiteral("paid_in"))), c),
                            alt_wb_money(alt_wb_num(f.value(QStringLiteral("nav"))), c),
                            alt_wb_pct(irr),
                            alt_wb_multiple(alt_wb_num(f.value(QStringLiteral("tvpi")))),
                            alt_wb_multiple(alt_wb_num(f.value(QStringLiteral("dpi")))),
                            alt_wb_money(alt_wb_num(f.value(QStringLiteral("unfunded"))), c)};
        QStringList t(11);
        t[7] = std::isfinite(irr) && irr < 0 ? QStringLiteral("bad") : QString();
        tones << t;
    }
    const QStringList headers = {tr("Fund"), tr("Strategy"), tr("Vintage"), tr("Ccy"),   tr("Commitment"), tr("Paid-in"),
                                 tr("NAV"),  tr("IRR"),      tr("TVPI"),    tr("DPI"),   tr("Unfunded")};
    book_model_->set_rows(headers, rows, {2, 4, 5, 6, 7, 8, 9, 10}, tones);
    alt_wb_fit_table(book_table_, 16);

    book_export_ = AltWbExport{};
    book_export_.title = tr("Private markets book");
    book_export_.figures = {{tr("Pooled IRR"), alt_wb_pct(alt_wb_num(p.value(QStringLiteral("irr"))))},
                            {tr("Pooled TVPI"), alt_wb_multiple(alt_wb_num(p.value(QStringLiteral("tvpi"))))},
                            {tr("Pooled DPI"), alt_wb_multiple(alt_wb_num(p.value(QStringLiteral("dpi"))))},
                            {tr("NAV"), alt_wb_money(alt_wb_num(p.value(QStringLiteral("nav"))), ccy)},
                            {tr("Unfunded"), alt_wb_money(alt_wb_num(p.value(QStringLiteral("unfunded"))), ccy)},
                            {tr("Committed"), alt_wb_money(alt_wb_num(p.value(QStringLiteral("commitment"))), ccy)}};
    book_export_.headers = headers;
    book_export_.rows = rows;
    book_export_.notes = notes.join(QLatin1Char(' '));
    alt_wb_set_actions_enabled(book_actions_, true);
}

} // namespace fincept::screens::alt
