#include "screens/derivatives/DerivCurvesPage.h"

#include "core/logging/Logger.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/MnemonicText.h"
#include "ui/widgets/Requirements.h"
#include "ui/widgets/StatTile.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace fincept::screens::deriv {

namespace mk = services::options::market;
namespace rt = services::options::rates;
namespace fmt = fincept::ui::formatting;
using services::options::CurveDataService;

namespace {

struct DcpRoot {
    const char* code;
    const char* name;
};
const DcpRoot kDcpRoots[] = {
    {"CL", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "WTI crude oil")},
    {"BZ", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "Brent crude oil")},
    {"NG", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "Natural gas")},
    {"RB", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "RBOB gasoline")},
    {"HO", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "Heating oil")},
    {"GC", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "Gold")},
    {"SI", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "Silver")},
    {"HG", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "Copper")},
    {"ZC", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "Corn")},
    {"ZS", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "Soybeans")},
    {"ZW", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "Wheat")},
    {"ES", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "E-mini S&P 500")},
    {"NQ", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "E-mini Nasdaq-100")},
    {"YM", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "E-mini Dow")},
    {"RTY", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "E-mini Russell 2000")},
    {"ZN", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "10-year T-note")},
    {"ZB", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "30-year T-bond")},
    {"6E", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "Euro FX")},
    {"6J", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "Japanese yen")},
    {"BTC", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "Bitcoin")},
};

/// Tab order on screen → view id.
struct DcpTab {
    int view;
    const char* text;
};
const DcpTab kDcpTabs[] = {
    {0, QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "RATES")},
    {2, QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "KEY-RATE DV01")},
    {3, QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "SWAPTION")},
    {4, QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "BOND")},
    {1, QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "FUTURES")},
    {5, QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivCurvesPage", "CARRY")},
};

constexpr int kDcpParCol = 1;
const QString kDcpCcy = QStringLiteral("USD");

/// Nominal maturity of a contract month: its middle (stated on the page).
QDate dcp_nominal(const QDate& month) {
    return QDate(month.year(), month.month(), 15);
}

QStandardItem* dcp_cell(const QString& t) {
    auto* it = new QStandardItem(t);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    it->setEditable(false);
    return it;
}

int dcp_freq(const QComboBox* c) {
    const int i = c ? c->currentIndex() : 1;
    return i == 0 ? 1 : i == 1 ? 2 : 4;
}


QWidget* dcp_grid_host(QWidget* parent, QGridLayout** grid) {
    auto* w = new QWidget(parent);
    auto* g = new QGridLayout(w);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(8);
    g->setVerticalSpacing(4);
    *grid = g;
    return w;
}

QTableView* dcp_table(QWidget* parent, QStandardItemModel** model) {
    auto* t = new QTableView(parent);
    *model = new QStandardItemModel(t);
    t->setModel(*model);
    dense_table(t);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    return t;
}

} // namespace

DerivCurvesPage::DerivCurvesPage(QWidget* parent) : QWidget(parent), book_("fincept::screens::deriv::DerivCurvesPage") {
    build();
}

DerivCurvesPage::~DerivCurvesPage() {
    auto& svc = CurveDataService::instance();
    for (quint64 id : {fut_req_, ust_req_, curve_req_})
        if (id)
            svc.cancel(id);
    if (kr_cancel_)
        kr_cancel_->store(true);
}

void DerivCurvesPage::build() {
    // A one-line strip across the page (view tabs, status, refresh, exports), then the view in
    // view with a narrow margin.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto* root = new QVBoxLayout;
    root->setContentsMargins(10, 8, 10, 6);
    root->setSpacing(6);
    QHBoxLayout* tl = nullptr;
    QWidget* top = make_strip(this, &tl);
    tl->setContentsMargins(4, 0, 10, 0);
    tl->setSpacing(2);
    outer->addWidget(top);
    outer->addLayout(root, 1);
    for (const DcpTab& t : kDcpTabs) {
        auto* b = new QPushButton(ui::literal_ampersands(tr(t.text)), top);
        b->setObjectName(QStringLiteral("derivSubBtn"));
        b->setCursor(Qt::PointingHandCursor);
        book_.button(b, t.text);
        const int v = t.view;
        connect(b, &QPushButton::clicked, this, [this, v] { set_view(v); });
        tl->addWidget(b);
        tab_btns_.append(b);
        tab_views_.append(v);
    }
    tl->addSpacing(12);
    status_ = new QLabel(top);
    status_->setObjectName(QStringLiteral("derivStatusLine"));
    status_->setWordWrap(true);
    tl->addWidget(status_, 1);
    auto* refresh = make_button(top, tr("REFRESH"), false);
    book_.button(refresh, QT_TR_NOOP("REFRESH"));
    refresh->setToolTip(tr("Reload the curve in view (F5)"));
    connect(refresh, &QPushButton::clicked, this, &DerivCurvesPage::refresh);
    tl->addWidget(refresh);
    result_bar_ = new DerivResultBar(QStringLiteral("curves"), top);
    result_bar_->set_table([this] { return export_table(); });
    result_bar_->set_image([this] { return export_image(); });
    result_bar_->set_payload([this] { return report_payload(); });
    tl->addWidget(result_bar_);

    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(200);
    connect(debounce_, &QTimer::timeout, this, &DerivCurvesPage::recompute_view);

    memory_ = new ui::InputMemory(QStringLiteral("derivatives"), QStringLiteral("curves"), this);

    // One holder per view; a view is built inside its holder the first time it is shown.
    views_ = new QStackedWidget(this);
    for (int i = 0; i < kViewCount; ++i) {
        auto* holder = new QWidget(views_);
        auto* hl = new QVBoxLayout(holder);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(0);
        views_->addWidget(holder);
        holders_.append(holder);
        built_.append(false);
    }
    root->addWidget(views_, 1);

    ensure_view(kRates);
    memory_->restore();
    load_overrides();
    // Connected after the restore: restoring must not start anything.
    for (QDoubleSpinBox* s : {sw_start_, sw_tenor_, sw_rate_, sw_notional_, fra_start_, fra_end_, fra_rate_, cap_strike_, cap_vol_})
        connect(s, &QDoubleSpinBox::valueChanged, this, &DerivCurvesPage::recompute_calculators);
    for (QComboBox* c : {sw_freq_, sw_side_, cap_kind_, cap_model_})
        connect(c, &QComboBox::currentIndexChanged, this, &DerivCurvesPage::recompute_calculators);
    ust_pane_->set_view(QStringLiteral("ust"));
    set_view(kRates);
    retranslate();
}

QWidget* DerivCurvesPage::ensure_view(int view) {
    if (view < 0 || view >= kViewCount)
        return nullptr;
    QWidget* holder = holders_[view];
    if (built_[view])
        return holder;
    built_[view] = true;
    QWidget* w = nullptr;
    switch (view) {
        case kRates: w = build_rates(holder); break;
        case kFutures: w = build_futures(holder); break;
        case kKeyRates: w = build_key_rates(holder); break;
        case kSwaption: w = build_swaption(holder); break;
        case kBond: w = build_bond(holder); break;
        case kCarry: w = build_carry(holder); break;
        default: break;
    }
    if (w)
        holder->layout()->addWidget(w);
    return holder;
}

// ── Views: RATES ─────────────────────────────────────────────────────────────

QWidget* DerivCurvesPage::build_rates(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    ust_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, w);
    auto* content = new QWidget(ust_pane_->content_parent());
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(6);
    ust_kpis_ = new ui::KpiStrip(content);
    cl->addWidget(ust_kpis_);
    auto* split = new QSplitter(Qt::Horizontal, content);
    ust_chart_ = make_xy_chart(split, tr("U.S. TREASURY CURVE"), QString(), {});
    split->addWidget(ust_chart_);
    auto* tbox = new QWidget(split);
    auto* tbl = new QVBoxLayout(tbox);
    tbl->setContentsMargins(0, 0, 0, 0);
    tbl->setSpacing(4);
    ust_table_ = dcp_table(tbox, &ust_model_);
    ust_table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                                QAbstractItemView::AnyKeyPressed);
    ust_table_->setToolTip(tr("Double-click a par yield to override it (percent). Your overrides re-price this "
                              "page's calculators."));
    connect(ust_model_, &QStandardItemModel::itemChanged, this, &DerivCurvesPage::apply_override_edit);
    tbl->addWidget(ust_table_, 1);
    auto* orow = new QWidget(tbox);
    auto* ol = new QHBoxLayout(orow);
    ol->setContentsMargins(0, 0, 0, 0);
    ovr_note_ = new QLabel(orow);
    ovr_note_->setObjectName(QStringLiteral("derivStatusLine"));
    ovr_note_->setWordWrap(true);
    ol->addWidget(ovr_note_, 1);
    ovr_reset_ = make_button(orow, tr("RESET TO TREASURY"), false);
    book_.button(ovr_reset_, QT_TR_NOOP("RESET TO TREASURY"));
    ovr_reset_->setToolTip(tr("Remove every override and use the published Treasury par curve"));
    connect(ovr_reset_, &QPushButton::clicked, this, &DerivCurvesPage::reset_overrides);
    ol->addWidget(ovr_reset_);
    tbl->addWidget(orow);
    split->addWidget(tbox);
    cl->addWidget(split, 1);

    // Calculators priced off the (edited) bootstrapped curve.
    Card card = make_card(content, QStringLiteral("IRS"), tr("SWAP · FRA · CAP / FLOOR OFF THIS CURVE"));
    book_.label(card.title, QT_TR_NOOP("SWAP · FRA · CAP / FLOOR OFF THIS CURVE"));
    QGridLayout* g = nullptr;
    QWidget* gw = dcp_grid_host(card.body->parentWidget(), &g);
    card.body->addWidget(gw);
    sw_start_ = make_spin(gw, 0, 0, 30, 2);
    sw_tenor_ = make_spin(gw, 5, 0.25, 30, 2);
    sw_rate_ = make_spin(gw, 4.0, -5, 30, 3, QStringLiteral("%"));
    sw_freq_ = make_combo(gw, {tr("Annual"), tr("Semi-annual"), tr("Quarterly")});
    book_.combo(sw_freq_, {QT_TR_NOOP("Annual"), QT_TR_NOOP("Semi-annual"), QT_TR_NOOP("Quarterly")});
    sw_freq_->setCurrentIndex(1);
    sw_notional_ = make_spin(gw, 1e7, 1, 1e12, 0);
    sw_side_ = make_combo(gw, {tr("Pay fixed"), tr("Receive fixed")});
    book_.combo(sw_side_, {QT_TR_NOOP("Pay fixed"), QT_TR_NOOP("Receive fixed")});
    fra_start_ = make_spin(gw, 0.5, 0, 30, 2);
    fra_end_ = make_spin(gw, 1.0, 0.01, 30, 2);
    fra_rate_ = make_spin(gw, 4.0, -5, 30, 3, QStringLiteral("%"));
    cap_strike_ = make_spin(gw, 4.5, -5, 30, 3, QStringLiteral("%"));
    cap_vol_ = make_spin(gw, 25, 0.01, 500, 2);
    cap_kind_ = make_combo(gw, {tr("Cap"), tr("Floor")});
    book_.combo(cap_kind_, {QT_TR_NOOP("Cap"), QT_TR_NOOP("Floor")});
    cap_model_ = make_combo(gw, {tr("Black-76 (vol %)"), tr("Bachelier (vol bp)")});
    book_.combo(cap_model_, {QT_TR_NOOP("Black-76 (vol %)"), QT_TR_NOOP("Bachelier (vol bp)")});
    struct F {
        int r, c;
        const char* src;
        QWidget* w;
    };
    for (const F& f : {F{0, 0, QT_TR_NOOP("START (YEARS)"), sw_start_}, F{0, 1, QT_TR_NOOP("TENOR (YEARS)"), sw_tenor_},
                       F{0, 2, QT_TR_NOOP("SWAP FIXED RATE"), sw_rate_}, F{0, 3, QT_TR_NOOP("FREQUENCY"), sw_freq_},
                       F{0, 4, QT_TR_NOOP("NOTIONAL"), sw_notional_}, F{0, 5, QT_TR_NOOP("SIDE"), sw_side_},
                       F{1, 0, QT_TR_NOOP("FRA START (YEARS)"), fra_start_}, F{1, 1, QT_TR_NOOP("FRA END (YEARS)"), fra_end_},
                       F{1, 2, QT_TR_NOOP("FRA RATE"), fra_rate_}, F{1, 3, QT_TR_NOOP("CAP / FLOOR STRIKE"), cap_strike_},
                       F{1, 4, QT_TR_NOOP("FLAT VOLATILITY"), cap_vol_}, F{1, 5, QT_TR_NOOP("CAP OR FLOOR"), cap_kind_},
                       F{2, 0, QT_TR_NOOP("CAP MODEL"), cap_model_}}) {
        QLabel* l = add_field(g, f.r, f.c, tr(f.src), f.w);
        book_.label(l, f.src);
    }
    auto* add_book = make_button(gw, tr("ADD SWAP TO BOOK"), false);
    book_.button(add_book, QT_TR_NOOP("ADD SWAP TO BOOK"));
    add_book->setToolTip(tr("Save this swap (start, tenor, fixed rate, frequency, notional, side) in the position book"));
    connect(add_book, &QPushButton::clicked, this, [this] {
        emit add_swap_to_book(sw_start_->value(), sw_tenor_->value(), sw_rate_->value() / 100.0, dcp_freq(sw_freq_),
                              sw_notional_->value(), sw_side_->currentIndex() == 0);
    });
    g->addWidget(add_book, 5, 5);
    compact_grid(g); // natural-width fields; the spare width stays at the right
    calc_kpis_ = new ui::KpiStrip(card.frame);
    card.body->addWidget(calc_kpis_);
    cl->addWidget(card.frame);
    ust_note_ = make_note(content, QString());
    cl->addWidget(ust_note_);
    ust_pane_->set_content(content);
    ust_pane_->set_requirements({ui::Requirement::python()});
    ust_pane_->set_persist_limit(0);
    ust_pane_->set_max_age(6 * 3600);
    ust_pane_->set_timeout(75);
    ust_pane_->set_loading_text(tr("Loading the U.S. Treasury par curve…"));
    ust_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Chart);
    ust_pane_->set_start([this](quint64 id) {
        auto& svc = CurveDataService::instance();
        const bool force = ust_pane_->property("derivForce").toBool();
        ust_pane_->setProperty("derivForce", false);
        if (!force && !treasury_) {
            // The saved curve at once (labelled with its date); a fresh one follows.
            if (auto saved = svc.treasury_now()) {
                treasury_ = saved;
                ust_pane_->deliver_partial(id, QJsonObject{{"date", saved->date.toString(Qt::ISODate)}});
            }
        }
        ust_req_ = svc.treasury_curve(force, this, [this, id](const CurveDataService::TreasuryReply& r) {
            ust_req_ = 0;
            if (!r.curve) {
                ust_pane_->fail(id, r.message, tr("Treasury curve unavailable"));
                return;
            }
            treasury_ = r.curve;
            treasury_stale_ = r.stale;
            ust_pane_->deliver(id, QJsonObject{{"date", r.curve->date.toString(Qt::ISODate)}});
            if (r.stale)
                ust_pane_->set_notice(tr("Treasury curve unavailable (%1) - using the saved curve of %2.")
                                          .arg(r.message, r.curve->date.toString(Qt::ISODate)));
        });
    });
    ust_pane_->set_cancel([this](quint64) {
        CurveDataService::instance().cancel(ust_req_);
        ust_req_ = 0;
    });
    ust_pane_->set_render([this](const QJsonValue&, const QDateTime&, bool) { on_curve_changed(); });
    vl->addWidget(ust_pane_, 1);

    ovr_memory_ = new QLineEdit(w);
    ovr_memory_->hide();
    memory_->track(QStringLiteral("sw_start"), sw_start_);
    memory_->track(QStringLiteral("sw_tenor"), sw_tenor_);
    memory_->track(QStringLiteral("sw_rate"), sw_rate_);
    memory_->track(QStringLiteral("sw_freq"), sw_freq_);
    memory_->track(QStringLiteral("sw_notional"), sw_notional_);
    memory_->track(QStringLiteral("sw_side"), sw_side_);
    memory_->track(QStringLiteral("fra_start"), fra_start_);
    memory_->track(QStringLiteral("fra_end"), fra_end_);
    memory_->track(QStringLiteral("fra_rate"), fra_rate_);
    memory_->track(QStringLiteral("cap_strike"), cap_strike_);
    memory_->track(QStringLiteral("cap_vol"), cap_vol_);
    memory_->track(QStringLiteral("cap_kind"), cap_kind_);
    memory_->track(QStringLiteral("cap_model"), cap_model_);
    memory_->track(QStringLiteral("ust_overrides"), ovr_memory_);
    return w;
}

// ── Views: FUTURES ───────────────────────────────────────────────────────────

QWidget* DerivCurvesPage::build_futures(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    auto* row = new QWidget(w);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(8);
    auto* cap = new QLabel(tr("FUTURES ROOT"), row);
    cap->setObjectName(QStringLiteral("derivLabel"));
    book_.label(cap, QT_TR_NOOP("FUTURES ROOT"));
    root_ = new QComboBox(row);
    for (const DcpRoot& r : kDcpRoots)
        root_->addItem(QStringLiteral("%1 · %2").arg(QLatin1String(r.code), tr(r.name)), QString::fromLatin1(r.code));
    cap->setBuddy(root_);
    rl->addWidget(cap);
    rl->addWidget(root_);
    rl->addStretch(1);
    auto* carry = make_button(row, tr("FAIR VALUE IN CARRY"), false);
    book_.button(carry, QT_TR_NOOP("FAIR VALUE IN CARRY"));
    carry->setToolTip(tr("Open the selected contract month (or the front month) in CARRY: fair value, basis, implied repo"));
    connect(carry, &QPushButton::clicked, this, [this] { use_future_in_carry(selected_future()); });
    rl->addWidget(carry);
    auto* add = make_button(row, tr("ADD TO BOOK"), false);
    book_.button(add, QT_TR_NOOP("ADD TO BOOK"));
    add->setToolTip(tr("Save the selected contract month (or the front month) in the position book"));
    connect(add, &QPushButton::clicked, this, [this] { add_selected_future_to_book(selected_future()); });
    rl->addWidget(add);
    vl->addWidget(row);

    fut_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, w);
    auto* content = new QWidget(fut_pane_->content_parent());
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(6);
    fut_kpis_ = new ui::KpiStrip(content);
    cl->addWidget(fut_kpis_);
    auto* split = new QSplitter(Qt::Horizontal, content);
    fut_chart_ = make_xy_chart(split, tr("FUTURES CURVE"), QString(), {});
    split->addWidget(fut_chart_);
    fut_table_ = dcp_table(split, &fut_model_);
    fut_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(fut_table_, &QTableView::customContextMenuRequested, this, &DerivCurvesPage::futures_menu);
    split->addWidget(fut_table_);
    cl->addWidget(split, 1);
    fut_note_ = make_note(content, QString());
    cl->addWidget(fut_note_);
    fut_pane_->set_content(content);
    fut_pane_->set_requirements({ui::Requirement::python()});
    fut_pane_->set_persist_limit(0);
    fut_pane_->set_max_age(5 * 60);
    fut_pane_->set_timeout(75);
    fut_pane_->set_loading_text(tr("Loading futures contract months (Yahoo Finance, delayed)…"));
    fut_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Chart);
    fut_pane_->set_start([this](quint64 id) {
        const QString r = root_->currentData().toString();
        const bool force = fut_pane_->property("derivForce").toBool();
        fut_pane_->setProperty("derivForce", false);
        fut_req_ = CurveDataService::instance().futures_curve(r, 10, force, this,
                                                              [this, id](const CurveDataService::FuturesReply& rep) {
                                                                  fut_req_ = 0;
                                                                  if (!rep.curve) {
                                                                      fut_pane_->fail(id, rep.message);
                                                                      return;
                                                                  }
                                                                  futures_ = rep.curve;
                                                                  fut_pane_->deliver(id, QJsonObject{{"root", rep.curve->root}});
                                                                  if (!rep.message.isEmpty())
                                                                      fut_pane_->set_notice(rep.message);
                                                              });
    });
    fut_pane_->set_cancel([this](quint64) {
        CurveDataService::instance().cancel(fut_req_);
        fut_req_ = 0;
    });
    fut_pane_->set_render([this](const QJsonValue&, const QDateTime&, bool) { render_futures(); });
    vl->addWidget(fut_pane_, 1);

    memory_->track(QStringLiteral("root"), root_); // applies the remembered root at once
    connect(root_, &QComboBox::currentIndexChanged, this, [this] { fut_pane_->set_view(root_->currentData().toString()); });
    fut_pane_->set_view(root_->currentData().toString());
    return w;
}

// ── Views: KEY-RATE DV01 ─────────────────────────────────────────────────────

QWidget* DerivCurvesPage::build_key_rates(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    Card card = make_card(w, QStringLiteral("KRD"), tr("KEY-RATE DV01 BY PAR TENOR"));
    book_.label(card.title, QT_TR_NOOP("KEY-RATE DV01 BY PAR TENOR"));
    QGridLayout* g = nullptr;
    QWidget* gw = dcp_grid_host(card.body->parentWidget(), &g);
    card.body->addWidget(gw);
    kr_instrument_ = make_combo(gw, {tr("The swap of the RATES calculator"), tr("A fixed-coupon bond")});
    book_.combo(kr_instrument_, {QT_TR_NOOP("The swap of the RATES calculator"), QT_TR_NOOP("A fixed-coupon bond")});
    QLabel* l = add_field(g, 0, 0, tr("INSTRUMENT"), kr_instrument_, 2);
    book_.label(l, QT_TR_NOOP("INSTRUMENT"));
    kr_swap_label_ = new QLabel(gw);
    kr_swap_label_->setObjectName(QStringLiteral("derivStatusLine"));
    kr_swap_label_->setWordWrap(true);
    g->addWidget(kr_swap_label_, 1, 2, 1, 4);
    compact_grid(g);
    kr_bond_row_ = dcp_grid_host(card.body->parentWidget(), &g);
    card.body->addWidget(kr_bond_row_);
    kr_coupon_ = make_spin(kr_bond_row_, 4.25, 0, 30, 3, QStringLiteral("%"));
    kr_maturity_ = make_spin(kr_bond_row_, 10.0, 0.1, 50, 2);
    kr_freq_ = make_combo(kr_bond_row_, {tr("Annual"), tr("Semi-annual"), tr("Quarterly")});
    book_.combo(kr_freq_, {QT_TR_NOOP("Annual"), QT_TR_NOOP("Semi-annual"), QT_TR_NOOP("Quarterly")});
    kr_freq_->setCurrentIndex(1);
    kr_face_ = make_spin(kr_bond_row_, 1e7, 1, 1e12, 0);
    struct F {
        int c;
        const char* src;
        QWidget* w;
    };
    for (const F& f : {F{0, QT_TR_NOOP("BOND COUPON"), kr_coupon_}, F{1, QT_TR_NOOP("MATURITY (YEARS)"), kr_maturity_},
                       F{2, QT_TR_NOOP("COUPON FREQUENCY"), kr_freq_}, F{3, QT_TR_NOOP("FACE AMOUNT HELD"), kr_face_}}) {
        QLabel* fl = add_field(g, 0, f.c, tr(f.src), f.w);
        book_.label(fl, f.src);
    }
    compact_grid(g);
    kr_kpis_ = new ui::KpiStrip(card.frame);
    card.body->addWidget(kr_kpis_);
    vl->addWidget(card.frame);
    auto* split = new QSplitter(Qt::Horizontal, w);
    kr_chart_ = new ui::BarChart(split);
    kr_chart_->set_signed_colors(true);
    kr_chart_->set_value_formatter([](double v) { return fmt::compact(v); });
    kr_chart_->setMinimumHeight(QFontMetrics(kr_chart_->font()).height() * 14);
    split->addWidget(kr_chart_);
    kr_table_ = dcp_table(split, &kr_model_);
    split->addWidget(kr_table_);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    vl->addWidget(split, 1);
    kr_note_ = make_note(w, QString());
    vl->addWidget(kr_note_);

    memory_->track(QStringLiteral("kr_instrument"), kr_instrument_);
    memory_->track(QStringLiteral("kr_coupon"), kr_coupon_);
    memory_->track(QStringLiteral("kr_maturity"), kr_maturity_);
    memory_->track(QStringLiteral("kr_freq"), kr_freq_);
    memory_->track(QStringLiteral("kr_face"), kr_face_);
    update_key_rate_rows();
    connect(kr_instrument_, &QComboBox::currentIndexChanged, this, [this] {
        update_key_rate_rows();
        schedule();
    });
    connect(kr_freq_, &QComboBox::currentIndexChanged, this, &DerivCurvesPage::schedule);
    for (QDoubleSpinBox* s : {kr_coupon_, kr_maturity_, kr_face_})
        connect(s, &QDoubleSpinBox::valueChanged, this, &DerivCurvesPage::schedule);
    return w;
}

void DerivCurvesPage::update_key_rate_rows() {
    if (!kr_instrument_)
        return;
    const bool bond = kr_instrument_->currentIndex() == 1;
    kr_bond_row_->setVisible(bond);
    kr_swap_label_->setVisible(!bond);
    kr_swap_label_->setText(tr("%1 %2-year swap starting in %3 years at %4, notional %5 (edit it on RATES).")
                                .arg(sw_side_->currentIndex() == 0 ? tr("Pay-fixed") : tr("Receive-fixed"),
                                     num(sw_tenor_->value(), 2), num(sw_start_->value(), 2),
                                     pct_points(sw_rate_->value(), 3), money(sw_notional_->value(), kDcpCcy)));
}

// ── Views: SWAPTION ──────────────────────────────────────────────────────────

QWidget* DerivCurvesPage::build_swaption(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    Card card = make_card(w, QStringLiteral("SWPT"), tr("EUROPEAN SWAPTION OFF THIS CURVE"));
    book_.label(card.title, QT_TR_NOOP("EUROPEAN SWAPTION OFF THIS CURVE"));
    QGridLayout* g = nullptr;
    QWidget* gw = dcp_grid_host(card.body->parentWidget(), &g);
    card.body->addWidget(gw);
    sv_expiry_ = make_spin(gw, 1.0, 0.01, 30, 2);
    sv_tenor_ = make_spin(gw, 5.0, 0.25, 30, 2);
    sv_strike_ = make_spin(gw, 4.0, -5, 30, 3, QStringLiteral("%"));
    sv_side_ = make_combo(gw, {tr("Payer (right to pay fixed)"), tr("Receiver (right to receive fixed)")});
    book_.combo(sv_side_, {QT_TR_NOOP("Payer (right to pay fixed)"), QT_TR_NOOP("Receiver (right to receive fixed)")});
    sv_model_ = make_combo(gw, {tr("Black-76 (lognormal vol %)"), tr("Bachelier (normal vol bp)")});
    book_.combo(sv_model_, {QT_TR_NOOP("Black-76 (lognormal vol %)"), QT_TR_NOOP("Bachelier (normal vol bp)")});
    sv_vol_ = make_spin(gw, 25.0, 0.01, 5000, 2);
    sv_freq_ = make_combo(gw, {tr("Annual"), tr("Semi-annual"), tr("Quarterly")});
    book_.combo(sv_freq_, {QT_TR_NOOP("Annual"), QT_TR_NOOP("Semi-annual"), QT_TR_NOOP("Quarterly")});
    sv_freq_->setCurrentIndex(1);
    sv_notional_ = make_spin(gw, 1e7, 1, 1e12, 0);
    struct F {
        int r, c;
        const char* src;
        QWidget* w;
    };
    for (const F& f : {F{0, 0, QT_TR_NOOP("EXPIRY (YEARS)"), sv_expiry_}, F{0, 1, QT_TR_NOOP("SWAP TENOR (YEARS)"), sv_tenor_},
                       F{0, 2, QT_TR_NOOP("STRIKE (FIXED RATE)"), sv_strike_}, F{0, 3, QT_TR_NOOP("TYPE"), sv_side_},
                       F{1, 0, QT_TR_NOOP("MODEL"), sv_model_}, F{1, 1, QT_TR_NOOP("VOLATILITY"), sv_vol_},
                       F{1, 2, QT_TR_NOOP("FIXED-LEG FREQUENCY"), sv_freq_}, F{1, 3, QT_TR_NOOP("NOTIONAL"), sv_notional_}}) {
        QLabel* l = add_field(g, f.r, f.c, tr(f.src), f.w);
        book_.label(l, f.src);
    }
    auto* atm = make_button(gw, tr("STRIKE AT THE FORWARD"), false);
    book_.button(atm, QT_TR_NOOP("STRIKE AT THE FORWARD"));
    atm->setToolTip(tr("Set the strike to the forward swap rate (at the money)"));
    connect(atm, &QPushButton::clicked, this, [this] { sv_strike_->setValue(sv_last_fwd_ * 100.0); });
    g->addWidget(atm, 1, 4); // beside the strike's row
    compact_grid(g);
    sv_kpis_ = new ui::KpiStrip(card.frame);
    card.body->addWidget(sv_kpis_);
    vl->addWidget(card.frame);
    sv_chart_ = make_xy_chart(w, tr("SWAPTION PREMIUM VS STRIKE"), QString(), {});
    vl->addWidget(sv_chart_, 1);
    sv_note_ = make_note(w, QString());
    vl->addWidget(sv_note_);

    memory_->track(QStringLiteral("sv_expiry"), sv_expiry_);
    memory_->track(QStringLiteral("sv_tenor"), sv_tenor_);
    memory_->track(QStringLiteral("sv_strike"), sv_strike_);
    memory_->track(QStringLiteral("sv_side"), sv_side_);
    memory_->track(QStringLiteral("sv_model"), sv_model_);
    memory_->track(QStringLiteral("sv_vol"), sv_vol_);
    memory_->track(QStringLiteral("sv_freq"), sv_freq_);
    memory_->track(QStringLiteral("sv_notional"), sv_notional_);
    update_swaption_units(false);
    connect(sv_model_, &QComboBox::currentIndexChanged, this, [this] {
        update_swaption_units(true);
        schedule();
    });
    for (QComboBox* c : {sv_side_, sv_freq_})
        connect(c, &QComboBox::currentIndexChanged, this, &DerivCurvesPage::schedule);
    for (QDoubleSpinBox* s : {sv_expiry_, sv_tenor_, sv_strike_, sv_vol_, sv_notional_})
        connect(s, &QDoubleSpinBox::valueChanged, this, &DerivCurvesPage::schedule);
    return w;
}

void DerivCurvesPage::update_swaption_units(bool convert) {
    if (!sv_vol_)
        return;
    const bool normal = sv_model_->currentIndex() == 1;
    QSignalBlocker block(sv_vol_);
    const double f = std::max(std::fabs(sv_last_fwd_), 1e-4);
    if (normal) {
        sv_vol_->setSuffix(QStringLiteral(" bp"));
        if (convert) // σn ≈ σ_Black · F
            sv_vol_->setValue(sv_vol_->value() / 100.0 * f * 1e4);
        sv_vol_->setToolTip(tr("Absolute (normal) volatility of the swap rate in basis points per year^0.5"));
    } else {
        sv_vol_->setSuffix(QStringLiteral(" %"));
        if (convert)
            sv_vol_->setValue(sv_vol_->value() / 1e4 / f * 100.0);
        sv_vol_->setToolTip(tr("Lognormal (Black) volatility of the swap rate, percent per year^0.5"));
    }
}

// ── Views: BOND ──────────────────────────────────────────────────────────────

QWidget* DerivCurvesPage::build_bond(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    Card card = make_card(w, QStringLiteral("BOND"), tr("FIXED-COUPON BOND OFF THIS CURVE"));
    book_.label(card.title, QT_TR_NOOP("FIXED-COUPON BOND OFF THIS CURVE"));
    QGridLayout* g = nullptr;
    QWidget* gw = dcp_grid_host(card.body->parentWidget(), &g);
    card.body->addWidget(gw);
    bd_coupon_ = make_spin(gw, 4.25, 0, 30, 3, QStringLiteral("%"));
    bd_freq_ = make_combo(gw, {tr("Annual"), tr("Semi-annual"), tr("Quarterly")});
    book_.combo(bd_freq_, {QT_TR_NOOP("Annual"), QT_TR_NOOP("Semi-annual"), QT_TR_NOOP("Quarterly")});
    bd_freq_->setCurrentIndex(1);
    bd_maturity_ = make_spin(gw, 9.5, 0.05, 50, 2);
    bd_price_ = make_spin(gw, 98.50, 0.01, 1000, 4);
    bd_kind_ = make_combo(gw, {tr("Clean price"), tr("Dirty (full) price")});
    book_.combo(bd_kind_, {QT_TR_NOOP("Clean price"), QT_TR_NOOP("Dirty (full) price")});
    bd_face_ = make_spin(gw, 1e7, 1, 1e12, 0);
    struct F {
        int c;
        const char* src;
        QWidget* w;
    };
    for (const F& f : {F{0, QT_TR_NOOP("COUPON (ANNUAL)"), bd_coupon_}, F{1, QT_TR_NOOP("FREQUENCY"), bd_freq_},
                       F{2, QT_TR_NOOP("MATURITY (YEARS)"), bd_maturity_}, F{3, QT_TR_NOOP("MARKET PRICE (PER 100)"), bd_price_},
                       F{4, QT_TR_NOOP("PRICE QUOTED AS"), bd_kind_}, F{5, QT_TR_NOOP("FACE AMOUNT HELD"), bd_face_}}) {
        QLabel* l = add_field(g, 0, f.c, tr(f.src), f.w);
        book_.label(l, f.src);
    }
    compact_grid(g);
    bd_kpis_ = new ui::KpiStrip(card.frame);
    card.body->addWidget(bd_kpis_);
    vl->addWidget(card.frame);
    bd_chart_ = make_xy_chart(w, tr("PRICE VS Z-SPREAD"), QString(), {});
    vl->addWidget(bd_chart_, 1);
    bd_note_ = make_note(w, QString());
    vl->addWidget(bd_note_);

    memory_->track(QStringLiteral("bd_coupon"), bd_coupon_);
    memory_->track(QStringLiteral("bd_freq"), bd_freq_);
    memory_->track(QStringLiteral("bd_maturity"), bd_maturity_);
    memory_->track(QStringLiteral("bd_kind"), bd_kind_);
    memory_->track(QStringLiteral("bd_price"), bd_price_);
    memory_->track(QStringLiteral("bd_face"), bd_face_);
    for (QComboBox* c : {bd_freq_, bd_kind_})
        connect(c, &QComboBox::currentIndexChanged, this, &DerivCurvesPage::schedule);
    for (QDoubleSpinBox* s : {bd_coupon_, bd_maturity_, bd_price_, bd_face_})
        connect(s, &QDoubleSpinBox::valueChanged, this, &DerivCurvesPage::schedule);
    return w;
}

// ── Views: CARRY ─────────────────────────────────────────────────────────────

QWidget* DerivCurvesPage::build_carry(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    Card card = make_card(w, QStringLiteral("CARRY"), tr("FUTURES FAIR VALUE · COST OF CARRY · IMPLIED REPO"));
    book_.label(card.title, QT_TR_NOOP("FUTURES FAIR VALUE · COST OF CARRY · IMPLIED REPO"));
    QGridLayout* g = nullptr;
    QWidget* gw = dcp_grid_host(card.body->parentWidget(), &g);
    card.body->addWidget(gw);
    cy_spot_ = make_spin(gw, 100.0, 0.0001, 1e9, 4);
    cy_days_ = make_int_spin(gw, 90, 1, 3650);
    cy_use_curve_ = new QCheckBox(tr("Financing rate from the curve"), gw);
    cy_use_curve_->setChecked(true);
    cy_use_curve_->setToolTip(tr("Use the zero rate of this page's curve at the delivery date"));
    cy_rate_ = make_spin(gw, 4.0, -20, 100, 3, QStringLiteral("%"));
    cy_carry_ = make_spin(gw, 0.0, -50, 100, 3, QStringLiteral("%"));
    cy_carry_->setToolTip(tr("Continuous storage / carry cost (e.g. warehousing, insurance) as a yield"));
    cy_yield_ = make_spin(gw, 1.5, -50, 100, 3, QStringLiteral("%"));
    cy_yield_->setToolTip(tr("Continuous income: dividend yield, foreign rate or convenience yield"));
    cy_d1_days_ = make_int_spin(gw, 0, 0, 3650);
    cy_d1_amt_ = make_spin(gw, 0.0, -1e9, 1e9, 4);
    cy_d2_days_ = make_int_spin(gw, 0, 0, 3650);
    cy_d2_amt_ = make_spin(gw, 0.0, -1e9, 1e9, 4);
    for (QWidget* x : {static_cast<QWidget*>(cy_d1_amt_), static_cast<QWidget*>(cy_d2_amt_)})
        x->setToolTip(tr("Cash income paid before delivery (dividend, coupon); a negative amount is a cost paid in cash "
                         "(e.g. storage)"));
    cy_market_ = make_spin(gw, 100.70, 0, 1e9, 4);
    cy_market_->setToolTip(tr("Market futures price; 0 = none (fair value only)"));
    struct F {
        int r, c;
        const char* src;
        QWidget* w;
    };
    for (const F& f : {F{0, 0, QT_TR_NOOP("SPOT PRICE"), cy_spot_}, F{0, 1, QT_TR_NOOP("DAYS TO DELIVERY"), cy_days_},
                       F{0, 2, QT_TR_NOOP("FINANCING RATE (CONT.)"), cy_rate_}, F{0, 3, QT_TR_NOOP("CARRY COST (CONT.)"), cy_carry_},
                       F{0, 4, QT_TR_NOOP("INCOME YIELD (CONT.)"), cy_yield_}, F{0, 5, QT_TR_NOOP("MARKET FUTURES PRICE"), cy_market_},
                       F{1, 0, QT_TR_NOOP("CASH INCOME 1: DAY"), cy_d1_days_}, F{1, 1, QT_TR_NOOP("CASH INCOME 1: AMOUNT"), cy_d1_amt_},
                       F{1, 2, QT_TR_NOOP("CASH INCOME 2: DAY"), cy_d2_days_}, F{1, 3, QT_TR_NOOP("CASH INCOME 2: AMOUNT"), cy_d2_amt_}}) {
        QLabel* l = add_field(g, f.r, f.c, tr(f.src), f.w);
        book_.label(l, f.src);
    }
    g->addWidget(cy_use_curve_, 3, 4, 1, 2);
    cy_use_future_ = make_button(gw, tr("USE SELECTED FUTURE"), false);
    book_.button(cy_use_future_, QT_TR_NOOP("USE SELECTED FUTURE"));
    cy_use_future_->setToolTip(tr("Fill the market price and the days to delivery from the contract month selected on "
                                  "FUTURES (the front month when none is selected)"));
    connect(cy_use_future_, &QPushButton::clicked, this, [this] { use_future_in_carry(selected_future()); });
    g->addWidget(cy_use_future_, 4, 4, 1, 2);
    cy_source_ = new QLabel(gw);
    cy_source_->setObjectName(QStringLiteral("derivStatusLine"));
    cy_source_->setWordWrap(true);
    g->addWidget(cy_source_, 4, 0, 1, 4);
    compact_grid(g);
    cy_kpis_ = new ui::KpiStrip(card.frame);
    card.body->addWidget(cy_kpis_);
    vl->addWidget(card.frame);
    cy_chart_ = make_xy_chart(w, tr("FAIR FUTURES PRICE VS DAYS TO DELIVERY"), QString(), {});
    vl->addWidget(cy_chart_, 1);
    cy_note_ = make_note(w, QString());
    vl->addWidget(cy_note_);

    memory_->track(QStringLiteral("cy_spot"), cy_spot_);
    memory_->track(QStringLiteral("cy_days"), cy_days_);
    memory_->track(QStringLiteral("cy_use_curve"), cy_use_curve_);
    memory_->track(QStringLiteral("cy_rate"), cy_rate_);
    memory_->track(QStringLiteral("cy_carry"), cy_carry_);
    memory_->track(QStringLiteral("cy_yield"), cy_yield_);
    memory_->track(QStringLiteral("cy_d1_days"), cy_d1_days_);
    memory_->track(QStringLiteral("cy_d1_amt"), cy_d1_amt_);
    memory_->track(QStringLiteral("cy_d2_days"), cy_d2_days_);
    memory_->track(QStringLiteral("cy_d2_amt"), cy_d2_amt_);
    memory_->track(QStringLiteral("cy_market"), cy_market_);
    connect(cy_use_curve_, &QCheckBox::toggled, this, &DerivCurvesPage::schedule);
    for (QDoubleSpinBox* s : {cy_spot_, cy_rate_, cy_carry_, cy_yield_, cy_d1_amt_, cy_d2_amt_, cy_market_})
        connect(s, &QDoubleSpinBox::valueChanged, this, &DerivCurvesPage::schedule);
    for (QSpinBox* s : {cy_days_, cy_d1_days_, cy_d2_days_})
        connect(s, &QSpinBox::valueChanged, this, &DerivCurvesPage::schedule);
    return w;
}

// ── View switching / refresh ─────────────────────────────────────────────────

void DerivCurvesPage::set_view(int index) {
    if (index < 0 || index >= kViewCount)
        return;
    if (view_ == kKeyRates && index != kKeyRates && kr_cancel_)
        kr_cancel_->store(true); // leaving: a running bump job is dropped
    ensure_view(index);
    view_ = index;
    views_->setCurrentIndex(index);
    for (int i = 0; i < tab_btns_.size(); ++i) {
        tab_btns_[i]->setProperty("active", tab_views_[i] == index);
        tab_btns_[i]->style()->unpolish(tab_btns_[i]);
        tab_btns_[i]->style()->polish(tab_btns_[i]);
    }
    if (index == kRates && treasury_)
        status_->setText(tr("%1 · curve of %2").arg(treasury_->source_label, treasury_->date.toString(Qt::ISODate)));
    else if (index != kFutures || !futures_)
        status_->clear();
    if (index >= kKeyRates) {
        ensure_curve();
        recompute_view(); // local maths: the view opens computed
    } else if (index == kFutures && futures_) {
        render_futures();
    }
}

void DerivCurvesPage::refresh() {
    switch (view_) {
        case kRates:
            ust_pane_->setProperty("derivForce", true);
            ust_pane_->run();
            break;
        case kFutures:
            if (fut_pane_) {
                fut_pane_->setProperty("derivForce", true);
                fut_pane_->run();
            }
            break;
        default: {
            // A calculator: reload the Treasury curve, then re-price.
            auto& svc = CurveDataService::instance();
            if (curve_req_)
                svc.cancel(curve_req_);
            status_->setText(tr("Reloading the U.S. Treasury curve…"));
            curve_req_ = svc.treasury_curve(true, this, [this](const CurveDataService::TreasuryReply& r) {
                curve_req_ = 0;
                if (!r.curve) {
                    status_->setText(tr("Treasury curve unavailable (%1) - the calculators keep the curve on screen.")
                                         .arg(r.message));
                    return;
                }
                treasury_ = r.curve;
                treasury_stale_ = r.stale;
                on_curve_changed();
            });
            recompute_view();
            break;
        }
    }
}

void DerivCurvesPage::schedule() {
    debounce_->start();
}

void DerivCurvesPage::recompute_view() {
    switch (view_) {
        case kRates:
            if (rates_dirty_) {
                rates_dirty_ = false;
                render_rates();
            }
            break;
        case kKeyRates:
            update_key_rate_rows();
            run_key_rates();
            break;
        case kSwaption:
            compute_swaption();
            break;
        case kBond:
            compute_bond();
            break;
        case kCarry:
            compute_carry();
            break;
        default:
            break;
    }
}

void DerivCurvesPage::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (view_ == kKeyRates && kr_chart_ && !kr_has_result_)
        run_key_rates(); // a job cancelled when the page was hidden
}

void DerivCurvesPage::hideEvent(QHideEvent* e) {
    if (kr_cancel_ && !kr_cancel_->load()) {
        kr_cancel_->store(true);
        kr_has_result_ = false;
    }
    QWidget::hideEvent(e);
}

// ── Curve: published Treasury + the user's overrides ─────────────────────────

void DerivCurvesPage::ensure_curve() {
    if (treasury_)
        return;
    auto& svc = CurveDataService::instance();
    if (auto saved = svc.treasury_now()) {
        treasury_ = saved;
        on_curve_changed();
        return;
    }
    if (curve_req_)
        return;
    curve_req_ = svc.treasury_curve(false, this, [this](const CurveDataService::TreasuryReply& r) {
        curve_req_ = 0;
        if (!r.curve) {
            status_->setText(tr("Treasury curve unavailable (%1). Open RATES and press REFRESH, or try again later.")
                                 .arg(r.message));
            return;
        }
        if (!treasury_) {
            treasury_ = r.curve;
            treasury_stale_ = r.stale;
            on_curve_changed();
        }
    });
}

void DerivCurvesPage::on_curve_changed() {
    rebuild_curve();
    render_rates();
    if (view_ >= kKeyRates)
        recompute_view();
}

void DerivCurvesPage::rebuild_curve() {
    eff_tenors_.clear();
    eff_par_.clear();
    eff_labels_.clear();
    applied_overrides_ = 0;
    zero_ = {};
    if (!treasury_ || !treasury_->ok())
        return;
    const mk::TreasuryCurve& c = *treasury_;
    for (int i = 0; i < c.tenors.size(); ++i) {
        const QString label = c.labels.value(i);
        double y = c.yields[i];
        const auto it = overrides_.constFind(label);
        if (it != overrides_.constEnd()) {
            y = it.value();
            ++applied_overrides_;
        }
        eff_tenors_.push_back(c.tenors[i]);
        eff_par_.push_back(y);
        eff_labels_ << label;
    }
    zero_ = rt::bootstrap_par_curve(eff_tenors_, eff_par_);
}

void DerivCurvesPage::load_overrides() {
    overrides_.clear();
    const QJsonObject o = QJsonDocument::fromJson(ovr_memory_->text().toUtf8()).object();
    for (auto it = o.constBegin(); it != o.constEnd(); ++it)
        if (it.value().isDouble() && std::isfinite(it.value().toDouble()))
            overrides_.insert(it.key(), it.value().toDouble());
}

void DerivCurvesPage::save_overrides() {
    QJsonObject o;
    for (auto it = overrides_.constBegin(); it != overrides_.constEnd(); ++it)
        o.insert(it.key(), it.value());
    ovr_memory_->setText(overrides_.isEmpty() ? QString()
                                              : QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
}

void DerivCurvesPage::apply_override_edit(QStandardItem* item) {
    if (rendering_rates_ || !item || item->column() != kDcpParCol || !treasury_ || !treasury_->ok())
        return;
    const int row = item->row();
    if (row < 0 || row >= treasury_->tenors.size())
        return;
    QString t = item->text();
    t.remove(QLatin1Char('%'));
    t = t.trimmed();
    bool ok = false;
    double v = QLocale().toDouble(t, &ok);
    if (!ok)
        v = t.toDouble(&ok);
    const QString label = treasury_->labels.value(row);
    if (ok && std::isfinite(v) && v > -5.0 && v < 30.0) {
        if (std::fabs(v - treasury_->yields[row]) < 1e-9)
            overrides_.remove(label);
        else
            overrides_.insert(label, v);
        save_overrides();
        LOG_DEBUG("Derivatives", QStringLiteral("Par curve override edited tenor=%1 overrides=%2").arg(label).arg(overrides_.size()));
    } else {
        status_->setText(tr("Type a par yield in percent between -5 and 30, for example 4.25."));
    }
    rebuild_curve();
    rates_dirty_ = true; // re-rendered outside the model's own signal
    kr_has_result_ = false;
    schedule();
}

void DerivCurvesPage::reset_overrides() {
    if (overrides_.isEmpty())
        return;
    overrides_.clear();
    save_overrides();
    rebuild_curve();
    kr_has_result_ = false;
    render_rates();
}

// ── Futures ──────────────────────────────────────────────────────────────────

void DerivCurvesPage::render_futures() {
    if (!fut_model_ || !futures_ || !futures_->ok())
        return;
    const mk::FuturesCurve& c = *futures_;
    const QDate today = QDate::currentDate();
    QVector<QPointF> pts;
    fut_model_->clear();
    fut_model_->setHorizontalHeaderLabels({tr("CONTRACT"), tr("MONTH"), tr("LAST"), tr("CHANGE"), tr("SPREAD TO NEXT"),
                                           tr("ROLL YIELD (ANN.)")});
    QVector<double> rolls;
    for (int i = 0; i < c.contracts.size(); ++i) {
        const mk::FuturesContract& k = c.contracts[i];
        const double months = today.daysTo(dcp_nominal(k.month)) / 30.4375;
        pts.append({months, k.last});
        QList<QStandardItem*> row;
        auto* name = new QStandardItem(k.symbol);
        name->setEditable(false);
        row << name << dcp_cell(k.month.toString(QStringLiteral("MMM yyyy"))) << dcp_cell(num(k.last, 4))
            << dcp_cell(std::isfinite(k.prev) ? signed_num(k.last - k.prev, 4) : QStringLiteral("—"));
        if (i + 1 < c.contracts.size()) {
            const mk::FuturesContract& n = c.contracts[i + 1];
            const double dt = dcp_nominal(k.month).daysTo(dcp_nominal(n.month)) / 365.0;
            const double roll = (dt > 0 && k.last > 0 && n.last > 0) ? std::log(k.last / n.last) / dt : std::nan("");
            rolls.append(roll);
            row << dcp_cell(signed_num(n.last - k.last, 4))
                << dcp_cell(std::isfinite(roll) ? fmt::fraction_pct(roll, 2) : QStringLiteral("—"));
        } else {
            row << dcp_cell(QStringLiteral("—")) << dcp_cell(QStringLiteral("—"));
        }
        fut_model_->appendRow(row);
    }
    fut_chart_->clear();
    fut_chart_->set_title(tr("%1 FUTURES CURVE").arg(c.name.toUpper()));
    fut_chart_->set_time_formatter([](double v) { return DerivCurvesPage::tr("%1 months").arg(num(v, 1)); });
    fut_chart_->set_tick_time_formatter([](double v) { return num(v, 0); });
    ui::TimeSeriesChart::SeriesSpec s;
    s.name = tr("Last price");
    s.points = pts;
    s.rebase = false;
    fut_chart_->add_series(s);
    fut_chart_->set_state(pts.size() >= 2 ? ui::ChartState::Ready : ui::ChartState::Empty);

    fut_kpis_->clear();
    auto tile = [this](const QString& l, const QString& v, const QString& sub) {
        ui::StatTile* t = fut_kpis_->add_tile(l);
        t->set_value(v);
        t->set_compact(true);
        t->set_sub_label(sub);
    };
    const mk::FuturesContract& front = c.contracts.first();
    const mk::FuturesContract& back = c.contracts.last();
    tile(tr("FRONT"), num(front.last, 4), front.symbol);
    tile(tr("LAST LISTED"), num(back.last, 4), back.symbol);
    const double slope = front.last > 0 ? (back.last / front.last - 1.0) : std::nan("");
    // A market structure, not a state: shown as plain text.
    tile(tr("STRUCTURE"), slope > 0 ? tr("Contango") : slope < 0 ? tr("Backwardation") : tr("Flat"),
         tr("back vs front %1").arg(std::isfinite(slope) ? fmt::fraction_pct(slope, 2) : QStringLiteral("—")));
    if (!rolls.isEmpty() && std::isfinite(rolls.first()))
        tile(tr("FRONT ROLL YIELD"), fmt::fraction_pct(rolls.first(), 2), tr("annualised, front → next"));
    if (std::isfinite(c.continuous))
        tile(tr("CONTINUOUS (=F)"), num(c.continuous, 4), tr("Yahoo front contract"));
    fut_note_->setText(tr("%1 · %2 · delayed. Roll yield = ln(F near / F next) / years between nominal maturities "
                          "(the middle of each contract month); positive in backwardation. Right-click a month to "
                          "open it in CARRY or add it to the book. %3")
                           .arg(c.unit, c.source_label,
                                c.missing.isEmpty() ? QString()
                                                    : tr("Months without a price: %1.").arg(c.missing.join(QStringLiteral(", ")))));
    if (view_ == kFutures)
        status_->setText(tr("%1 · fetched %2").arg(c.source_label, c.fetched_at.toLocalTime().toString(QStringLiteral("d MMM HH:mm"))));
}

int DerivCurvesPage::selected_future() const {
    if (!fut_table_ || !fut_table_->selectionModel())
        return 0;
    const QModelIndexList rows = fut_table_->selectionModel()->selectedRows();
    return rows.isEmpty() ? 0 : rows.first().row();
}

void DerivCurvesPage::futures_menu(const QPoint& pos) {
    const QModelIndex index = fut_table_->indexAt(pos);
    if (!index.isValid() || !futures_ || index.row() >= futures_->contracts.size())
        return;
    const int row = index.row();
    QMenu menu(this);
    QAction* carry = menu.addAction(tr("Fair value, basis and implied repo in CARRY"));
    QAction* add = menu.addAction(tr("Add %1 to the book").arg(futures_->contracts[row].symbol));
    QAction* picked = menu.exec(fut_table_->viewport()->mapToGlobal(pos));
    if (picked == carry)
        use_future_in_carry(row);
    else if (picked == add)
        add_selected_future_to_book(row);
}

void DerivCurvesPage::use_future_in_carry(int row) {
    if (!futures_ || !futures_->ok()) {
        status_->setText(tr("Load a futures curve on FUTURES first."));
        return;
    }
    row = std::clamp(row, 0, static_cast<int>(futures_->contracts.size()) - 1);
    const mk::FuturesContract& k = futures_->contracts[row];
    ensure_view(kCarry);
    const int days = std::max(1, static_cast<int>(QDate::currentDate().daysTo(dcp_nominal(k.month))));
    {
        QSignalBlocker b1(cy_market_), b2(cy_days_);
        if (k.last > 0)
            cy_market_->setValue(k.last);
        cy_days_->setValue(days);
    }
    cy_source_->setText(tr("Market price %1 of %2 (%3, delayed); delivery taken as the middle of %4. Type the spot "
                           "price of the underlying.")
                            .arg(num(k.last, 4), k.symbol, futures_->source_label, k.month.toString(QStringLiteral("MMM yyyy"))));
    set_view(kCarry);
}

void DerivCurvesPage::add_selected_future_to_book(int row) {
    if (!futures_ || !futures_->ok()) {
        status_->setText(tr("Load a futures curve on FUTURES first."));
        return;
    }
    row = std::clamp(row, 0, static_cast<int>(futures_->contracts.size()) - 1);
    const mk::FuturesContract& k = futures_->contracts[row];
    emit add_future_to_book(k.symbol, futures_->root, k.last);
    status_->setText(tr("%1 sent to the book at %2.").arg(k.symbol, num(k.last, 4)));
}

// ── Rates ────────────────────────────────────────────────────────────────────

void DerivCurvesPage::render_rates() {
    if (!treasury_ || !treasury_->ok() || !ust_model_)
        return;
    if (eff_tenors_.empty())
        rebuild_curve();
    if (zero_.empty())
        return;
    const mk::TreasuryCurve& c = *treasury_;
    const rt::ZeroCurve& z = zero_;
    QVector<QPointF> par, zero, fwd, published;
    rendering_rates_ = true;
    ust_model_->clear();
    ust_model_->setHorizontalHeaderLabels({tr("TENOR"), tr("PAR YIELD"), tr("CHANGE"), tr("ZERO (CONT.)"),
                                           tr("DISCOUNT FACTOR"), tr("6M FORWARD"), tr("SOURCE")});
    QFont bold;
    bold.setBold(true);
    for (int i = 0; i < c.tenors.size() && i < static_cast<int>(eff_par_.size()); ++i) {
        const double t = c.tenors[i];
        const double y = eff_par_[static_cast<size_t>(i)];
        const bool edited = overrides_.contains(c.labels.value(i));
        const double zr = z.rate_at(t), f6 = rt::forward_rate(z, t, t + 0.5);
        par.append({t, y});
        published.append({t, c.yields[i]});
        zero.append({t, zr * 100.0});
        fwd.append({t, f6 * 100.0});
        const double prev = c.prev_yields.value(i, std::nan(""));
        QList<QStandardItem*> row;
        auto* tenor = new QStandardItem(c.labels.value(i));
        tenor->setEditable(false);
        auto* py = new QStandardItem(pct_points(y, 3));
        py->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        py->setEditable(true);
        py->setToolTip(tr("Double-click to override this par yield (percent)"));
        auto* src = new QStandardItem(edited ? tr("Edited (Treasury %1)").arg(pct_points(c.yields[i], 3)) : tr("Treasury"));
        src->setEditable(false);
        if (edited) {
            py->setFont(bold);
            src->setFont(bold);
            tenor->setFont(bold);
        }
        row << tenor << py
            << dcp_cell(std::isfinite(prev) ? fmt::bp((c.yields[i] - prev) * 100.0) : QStringLiteral("—"))
            << dcp_cell(vol_pct(zr, 3)) << dcp_cell(num(z.df(t), 6)) << dcp_cell(vol_pct(f6, 3)) << src;
        ust_model_->appendRow(row);
    }
    rendering_rates_ = false;
    ust_chart_->clear();
    ust_chart_->set_title(applied_overrides_ > 0 ? tr("U.S. TREASURY CURVE · %1 · EDITED").arg(c.date.toString(Qt::ISODate))
                                                 : tr("U.S. TREASURY CURVE · %1").arg(c.date.toString(Qt::ISODate)));
    ust_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return num(v, 2) + QLatin1Char('%'); });
    ust_chart_->set_time_formatter([](double v) { return DerivCurvesPage::tr("%1 years").arg(num(v, 2)); });
    ust_chart_->set_tick_time_formatter([](double v) { return num(v, 0); });
    struct Line {
        QString name;
        QVector<QPointF> pts;
        bool dashed;
        bool muted;
    };
    QVector<Line> lines = {{applied_overrides_ > 0 ? tr("Par yield (edited)") : tr("Par yield"), par, false, false},
                           {tr("Zero rate (cont.)"), zero, false, false},
                           {tr("6-month forward"), fwd, true, false}};
    if (applied_overrides_ > 0)
        lines.append({tr("Published Treasury par"), published, true, true});
    for (const Line& l : lines) {
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = l.name;
        s.points = l.pts;
        s.dashed = l.dashed;
        s.rebase = false;
        if (l.muted)
            s.color = ui::TimeSeriesChart::ColorRole::Muted;
        ust_chart_->add_series(s);
    }
    ust_kpis_->clear();
    auto tile = [this](const QString& l, const QString& v, const QString& sub) {
        ui::StatTile* t = ust_kpis_->add_tile(l);
        t->set_value(v);
        t->set_compact(true);
        t->set_sub_label(sub);
    };
    auto at = [&](double tenor) {
        for (int i = 0; i < c.tenors.size() && i < static_cast<int>(eff_par_.size()); ++i)
            if (std::fabs(c.tenors[i] - tenor) < 1e-9)
                return i;
        return -1;
    };
    const int i2 = at(2.0), i10 = at(10.0), i3m = at(0.25);
    auto par_at = [&](int i) { return eff_par_[static_cast<size_t>(i)]; };
    if (i3m >= 0)
        tile(tr("3M BILL"), pct_points(par_at(i3m)), tr("par"));
    if (i2 >= 0)
        tile(tr("2Y"), pct_points(par_at(i2)), tr("par"));
    if (i10 >= 0)
        tile(tr("10Y"), pct_points(par_at(i10)), tr("par"));
    if (i2 >= 0 && i10 >= 0)
        tile(tr("2S10S"), fmt::bp((par_at(i10) - par_at(i2)) * 100.0), tr("10Y − 2Y"));
    tile(tr("CURVE DATE"), c.date.toString(QStringLiteral("d MMM yyyy")), treasury_stale_ ? tr("saved copy") : tr("latest"));
    if (applied_overrides_ > 0)
        tile(tr("YOUR OVERRIDES"), QString::number(applied_overrides_), tr("tenors edited"));
    ust_note_->setText(tr("%1 (public domain). Par yields are bootstrapped to continuously compounded zero rates: bills "
                          "as zero-coupon yields, longer tenors as semi-annual par bonds with linearly interpolated par "
                          "yields. The Treasury curve stands in for a risk-free curve (no free OIS swap curve exists); "
                          "swap, FRA, cap, swaption, bond and carry values on this page use it to project and discount "
                          "(single curve).")
                           .arg(c.source_label));
    ovr_note_->setText(applied_overrides_ > 0
                           ? tr("Your overrides on %n tenor(s) are kept when a new Treasury curve arrives. They apply to "
                                "this page's calculators only; the option pages keep the published curve.",
                                nullptr, applied_overrides_)
                           : tr("Double-click a par yield to override it. Overrides apply to this page's calculators only."));
    ovr_reset_->setEnabled(!overrides_.isEmpty());
    if (view_ == kRates)
        status_->setText(tr("%1 · curve of %2").arg(c.source_label, c.date.toString(Qt::ISODate)));
    recompute_calculators();
}

void DerivCurvesPage::recompute_calculators() {
    calc_kpis_->clear();
    auto& rows = calc_rows_[kRates];
    rows.clear();
    if (!has_curve())
        return;
    const rt::ZeroCurve& z = zero_;
    const int freq = dcp_freq(sw_freq_);
    const double N = sw_notional_->value();
    const bool payer = sw_side_->currentIndex() == 0;
    const rt::SwapResult sw = rt::swap_on_curve(z, sw_start_->value(), sw_tenor_->value(), sw_rate_->value() / 100.0,
                                                freq, N, payer);
    double fwd = 0;
    const double fra = rt::fra_value(z, fra_start_->value(), std::max(fra_end_->value(), fra_start_->value() + 0.01),
                                     fra_rate_->value() / 100.0, N, payer, &fwd);
    const bool normal = cap_model_->currentIndex() == 1;
    const double vol = normal ? cap_vol_->value() / 10000.0 : cap_vol_->value() / 100.0;
    const rt::CapResult cap = rt::cap_floor(z, sw_start_->value(), sw_tenor_->value(), cap_strike_->value() / 100.0, vol,
                                            freq, N, cap_kind_->currentIndex() == 0, normal);
    auto tile = [this, &rows](const QString& l, const QString& v, const QString& sub, double sign = 0) {
        ui::StatTile* t = calc_kpis_->add_tile(l);
        t->set_value(v);
        t->set_compact(true);
        t->set_sub_label(sub);
        if (sign != 0)
            t->set_tone(sign > 0 ? ui::StatTile::Tone::Positive : ui::StatTile::Tone::Negative);
        rows.append({l, v, sub});
    };
    tile(tr("SWAP PAR RATE"), vol_pct(sw.par_rate, 3), tr("%1 payments").arg(sw.payments));
    tile(tr("SWAP PV"), signed_money(sw.pv, kDcpCcy), payer ? tr("to the fixed payer") : tr("to the fixed receiver"), sw.pv);
    tile(tr("SWAP DV01"), signed_money(sw.dv01, kDcpCcy), tr("PV change, +1 bp parallel"));
    tile(tr("FRA FORWARD"), vol_pct(fwd, 3), tr("simple, %1–%2 y").arg(num(fra_start_->value(), 2), num(fra_end_->value(), 2)));
    tile(tr("FRA VALUE"), signed_money(fra, kDcpCcy), payer ? tr("pay fixed") : tr("receive fixed"), fra);
    tile(cap_kind_->currentIndex() == 0 ? tr("CAP VALUE") : tr("FLOOR VALUE"), money(cap.value, kDcpCcy),
         tr("%1 caplets, %2").arg(cap.caplet_values.size()).arg(normal ? tr("Bachelier") : tr("Black-76")));
    tile(tr("CAP DV01"), signed_money(cap.dv01, kDcpCcy), tr("+1 bp, vol unchanged"));
}

// ── Key-rate DV01 ────────────────────────────────────────────────────────────

void DerivCurvesPage::run_key_rates() {
    if (!kr_chart_)
        return;
    if (!has_curve() || eff_tenors_.size() < 2) {
        kr_chart_->set_state(ui::PaintedChart::Loading, tr("Waiting for the U.S. Treasury curve…"));
        return;
    }
    if (kr_cancel_)
        kr_cancel_->store(true);
    kr_cancel_ = std::make_shared<std::atomic<bool>>(false);
    const quint64 id = ++kr_req_;
    const std::vector<double> tenors = eff_tenors_, par = eff_par_;
    const bool bond = kr_instrument_->currentIndex() == 1;
    const double s0 = sw_start_->value(), sT = sw_tenor_->value(), sK = sw_rate_->value() / 100.0;
    const int sF = dcp_freq(sw_freq_);
    const double sN = sw_notional_->value();
    const bool payer = sw_side_->currentIndex() == 0;
    rt::BondSpec b;
    b.coupon_pct = kr_coupon_->value();
    b.freq = dcp_freq(kr_freq_);
    b.maturity = kr_maturity_->value();
    const double face = kr_face_->value();
    const QString what = bond ? tr("%1 %2-year bond, face %3").arg(pct_points(b.coupon_pct, 3), num(b.maturity, 2),
                                                                     money(face, kDcpCcy))
                              : tr("%1 %2-year swap, notional %3")
                                    .arg(payer ? tr("pay-fixed") : tr("receive-fixed"), num(sT, 2), money(sN, kDcpCcy));
    if (!kr_has_result_)
        kr_chart_->set_state(ui::PaintedChart::Loading, tr("Calculating key-rate DV01…"));
    auto cancel = kr_cancel_;
    QPointer<DerivCurvesPage> self(this);
    (void)QtConcurrent::run([self, id, cancel, tenors, par, bond, s0, sT, sK, sF, sN, payer, b, face, what]() {
        KeyRateOut out;
        out.what = what;
        rt::CurvePricer pv;
        if (bond)
            pv = [b, face](const rt::ZeroCurve& c) { return rt::bond_on_curve(c, b).dirty * face / 100.0; };
        else
            pv = [s0, sT, sK, sF, sN, payer](const rt::ZeroCurve& c) { return rt::swap_pv(c, s0, sT, sK, sF, sN, payer); };
        out.kr = rt::key_rate_dv01(tenors, par, pv, cancel.get());
        if (cancel->load() || out.kr.cancelled)
            return;
        const rt::ZeroCurve z = rt::bootstrap_par_curve(tenors, par);
        out.zero_dv01 = 0.5 * (pv(rt::shifted(z, 1.0)) - pv(rt::shifted(z, -1.0)));
        // Delivered through the application object: the QPointer is only read on the GUI thread.
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, id, out]() {
                if (self && id == self->kr_req_)
                    self->render_key_rates(out);
            },
            Qt::QueuedConnection);
    });
}

void DerivCurvesPage::render_key_rates(const KeyRateOut& out) {
    kr_has_result_ = true;
    const rt::KeyRateResult& kr = out.kr;
    QStringList cats;
    QVector<double> vals;
    kr_model_->clear();
    kr_model_->setHorizontalHeaderLabels({tr("PAR TENOR"), tr("KEY-RATE DV01"), tr("SHARE OF TOTAL"), tr("PAR YIELD USED")});
    double largest = 0;
    QString largest_label;
    for (size_t i = 0; i < kr.tenors.size(); ++i) {
        const QString label = eff_labels_.value(static_cast<int>(i), num(kr.tenors[i], 2));
        cats << label;
        vals << kr.dv01[i];
        if (std::fabs(kr.dv01[i]) > std::fabs(largest)) {
            largest = kr.dv01[i];
            largest_label = label;
        }
        QList<QStandardItem*> row;
        auto* l = new QStandardItem(label);
        l->setEditable(false);
        row << l << dcp_cell(signed_money(kr.dv01[i], kDcpCcy))
            << dcp_cell(std::fabs(kr.total) > 1e-12 ? fmt::fraction_pct(kr.dv01[i] / kr.total, 1, false) : QStringLiteral("—"))
            << dcp_cell(i < eff_par_.size() ? pct_points(eff_par_[i], 3) : QStringLiteral("—"));
        kr_model_->appendRow(row);
    }
    kr_chart_->set_data(cats, vals, tr("Key-rate DV01"));
    kr_chart_->set_state(ui::PaintedChart::Ready);
    kr_kpis_->clear();
    auto& rows = calc_rows_[kKeyRates];
    rows.clear();
    auto tile = [this, &rows](const QString& l, const QString& v, const QString& sub) {
        ui::StatTile* t = kr_kpis_->add_tile(l);
        t->set_value(v);
        t->set_compact(true);
        t->set_sub_label(sub);
        rows.append({l, v, sub});
    };
    tile(tr("PV"), signed_money(kr.base_pv, kDcpCcy), out.what);
    tile(tr("SUM OF KEY RATES"), signed_money(kr.total, kDcpCcy), tr("per +1 bp on every bucket"));
    tile(tr("PARALLEL (PAR CURVE)"), signed_money(kr.parallel, kDcpCcy), tr("every par yield +1 bp"));
    tile(tr("PARALLEL (ZERO CURVE)"), signed_money(out.zero_dv01, kDcpCcy), tr("every zero rate +1 bp"));
    if (!largest_label.isEmpty())
        tile(tr("LARGEST BUCKET"), largest_label, signed_money(largest, kDcpCcy));
    kr_note_->setText(tr("Each bucket bumps ONE par yield of the curve on RATES (your overrides included) by 1 bp, "
                         "re-bootstraps the zero curve and re-prices (central difference). The bootstrap interpolates par "
                         "yields linearly, so a bucket is a tent-shaped bump between its neighbouring tenors and the "
                         "buckets add up to the parallel par-curve move. Positive = gains when that rate rises."));
}

// ── Swaption ─────────────────────────────────────────────────────────────────

void DerivCurvesPage::compute_swaption() {
    if (!sv_kpis_)
        return;
    sv_kpis_->clear();
    auto& rows = calc_rows_[kSwaption];
    rows.clear();
    if (!has_curve()) {
        sv_chart_->set_state(ui::ChartState::Loading, tr("Waiting for the U.S. Treasury curve…"));
        return;
    }
    const bool normal = sv_model_->currentIndex() == 1;
    const bool payer = sv_side_->currentIndex() == 0;
    const double T = sv_expiry_->value(), tenor = sv_tenor_->value();
    const int freq = dcp_freq(sv_freq_);
    const double N = sv_notional_->value();
    const double K = sv_strike_->value() / 100.0;
    const double vol = normal ? sv_vol_->value() / 1e4 : sv_vol_->value() / 100.0;
    const rt::SwaptionResult r = rt::swaption_on_curve(zero_, T, tenor, K, vol, freq, N, payer, normal);
    sv_last_fwd_ = r.forward_rate;
    auto tile = [this, &rows](const QString& l, const QString& v, const QString& sub) {
        ui::StatTile* t = sv_kpis_->add_tile(l);
        t->set_value(v);
        t->set_compact(true);
        t->set_sub_label(sub);
        rows.append({l, v, sub});
    };
    tile(tr("FORWARD SWAP RATE"), vol_pct(r.forward_rate, 3), tr("%1 into %2 y").arg(num(T, 2), num(tenor, 2)));
    tile(tr("ANNUITY (PV01)"), money(N * r.annuity * 1e-4, kDcpCcy), tr("per bp of the swap rate, %1 payments").arg(r.payments));
    sv_chart_->clear();
    if (!r.ok) {
        sv_note_->setText(QString::fromStdString(r.message));
        sv_chart_->set_state(ui::ChartState::Empty, QString::fromStdString(r.message));
        return;
    }
    tile(tr("PREMIUM"), money(r.premium, kDcpCcy), tr("%1 of notional").arg(fmt::fraction_pct(r.premium / N, 3, false)));
    tile(tr("PREMIUM (RUNNING)"), fmt::bp(r.premium / (N * r.annuity) * 1e4, 1, false), tr("per year over the swap"));
    tile(tr("INTRINSIC"), money(r.intrinsic, kDcpCcy), tr("time value %1").arg(money(r.premium - r.intrinsic, kDcpCcy)));
    tile(tr("DELTA"), signed_money(r.delta_bp, kDcpCcy), tr("per +1 bp of the forward rate"));
    tile(tr("VEGA"), money(r.vega, kDcpCcy), normal ? tr("per 1 bp normal vol") : tr("per 1 vol point"));
    tile(tr("DV01"), signed_money(r.dv01, kDcpCcy), tr("+1 bp parallel curve, vol fixed"));

    // Premium against the strike, ±200 bp around the forward.
    QVector<QPointF> pts, intr;
    for (int k = -20; k <= 20; ++k) {
        const double kk = r.forward_rate + k * 0.001;
        if (!normal && kk <= 0)
            continue;
        const rt::SwaptionResult p = rt::swaption_on_curve(zero_, T, tenor, kk, vol, freq, N, payer, normal);
        if (!p.ok)
            continue;
        pts.append({kk * 100.0, p.premium});
        intr.append({kk * 100.0, p.intrinsic});
    }
    sv_chart_->set_title(tr("%1 PREMIUM VS STRIKE").arg(payer ? tr("PAYER") : tr("RECEIVER")));
    sv_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return fmt::compact(v); });
    sv_chart_->set_time_formatter([](double v) { return DerivCurvesPage::tr("Strike %1").arg(pct_points(v, 3)); });
    sv_chart_->set_tick_time_formatter([](double v) { return pct_points(v, 2); });
    ui::TimeSeriesChart::SeriesSpec s;
    s.name = normal ? tr("Bachelier premium") : tr("Black-76 premium");
    s.points = pts;
    s.rebase = false;
    sv_chart_->add_series(s);
    ui::TimeSeriesChart::SeriesSpec i;
    i.name = tr("Intrinsic (forward)");
    i.points = intr;
    i.dashed = true;
    i.rebase = false;
    i.color = ui::TimeSeriesChart::ColorRole::Muted;
    sv_chart_->add_series(i);
    sv_chart_->add_price_marker(K * 100.0, r.premium, ui::ChartMarkerKind::Custom, tr("K"),
                                tr("Strike %1 · premium %2").arg(pct_points(K * 100.0, 3), money(r.premium, kDcpCcy)));
    sv_chart_->set_state(pts.size() >= 2 ? ui::ChartState::Ready : ui::ChartState::Empty);
    sv_note_->setText(tr("European swaption exercised at %1 years into a %2-year swap; forward swap rate and annuity "
                         "from this page's curve (single curve, ACT/365 accruals). %3 Delta and vega hold the annuity "
                         "fixed; DV01 re-prices on the curve shifted ±1 bp with the volatility unchanged.")
                          .arg(num(T, 2), num(tenor, 2),
                               normal ? tr("Bachelier: premium = N·A·[(F−K)·N(d) + σn·√T·n(d)], normal vol in bp.")
                                      : tr("Black-76: premium = N·A·[F·N(d1) − K·N(d2)] (payer), lognormal vol.")));
    if (view_ == kSwaption)
        status_->setText(tr("Swaption off the curve of %1%2").arg(treasury_ ? treasury_->date.toString(Qt::ISODate) : QString(),
                                                                  applied_overrides_ > 0 ? tr(" (edited)") : QString()));
}

// ── Bond ─────────────────────────────────────────────────────────────────────

void DerivCurvesPage::compute_bond() {
    if (!bd_kpis_)
        return;
    bd_kpis_->clear();
    auto& rows = calc_rows_[kBond];
    rows.clear();
    if (!has_curve()) {
        bd_chart_->set_state(ui::ChartState::Loading, tr("Waiting for the U.S. Treasury curve…"));
        return;
    }
    rt::BondSpec b;
    b.coupon_pct = bd_coupon_->value();
    b.freq = dcp_freq(bd_freq_);
    b.maturity = bd_maturity_->value();
    const double face = bd_face_->value();
    const rt::BondCurveResult c0 = rt::bond_on_curve(zero_, b, 0.0);
    auto tile = [this, &rows](const QString& l, const QString& v, const QString& sub, ui::StatTile::Tone tone = ui::StatTile::Tone::Neutral) {
        ui::StatTile* t = bd_kpis_->add_tile(l);
        t->set_value(v);
        t->set_compact(true);
        t->set_sub_label(sub);
        if (tone != ui::StatTile::Tone::Neutral)
            t->set_tone(tone);
        rows.append({l, v, sub});
    };
    bd_chart_->clear();
    if (!c0.ok) {
        bd_note_->setText(tr("The bond has no cash flows left: check the maturity."));
        bd_chart_->set_state(ui::ChartState::Empty, bd_note_->text());
        return;
    }
    const bool clean_quote = bd_kind_->currentIndex() == 0;
    const double market_dirty = clean_quote ? bd_price_->value() + c0.accrued : bd_price_->value();
    const double market_clean = market_dirty - c0.accrued;
    const rt::SpreadSolve z = rt::z_spread(zero_, b, market_dirty);
    const double asw = rt::asset_swap_spread(zero_, b, market_dirty);
    tile(tr("CURVE PRICE"), num(c0.clean, 4), tr("clean, dirty %1").arg(num(c0.dirty, 4)));
    tile(tr("MARKET PRICE"), num(market_clean, 4), tr("clean, dirty %1").arg(num(market_dirty, 4)));
    tile(tr("ACCRUED"), num(c0.accrued, 4), tr("per 100 face"));
    if (z.ok) {
        tile(tr("Z-SPREAD"), fmt::bp(z.spread * 1e4, 1), tr("continuous, over the zero curve"));
        tile(tr("ASSET-SWAP SPREAD"), fmt::bp(asw * 1e4, 1), tr("par-par; + = cheap to the curve"));
        const rt::BondCurveResult cz = rt::bond_on_curve(zero_, b, z.spread);
        tile(tr("EFFECTIVE DURATION"), num(cz.duration, 3), tr("years, at the market price"));
        tile(tr("CONVEXITY"), num(cz.convexity, 2), tr("years²"));
        tile(tr("POSITION DV01"), signed_money(-cz.dirty * cz.duration * 1e-4 * face / 100.0, kDcpCcy),
             tr("per +1 bp of yield or spread"));
    } else {
        tile(tr("Z-SPREAD"), QStringLiteral("—"), QString::fromStdString(z.message), ui::StatTile::Tone::Warning);
    }
    // Clean price against the Z-spread, ±150 bp around the solved one.
    const double z_mid = z.ok ? z.spread : 0.0;
    QVector<QPointF> pts;
    for (int k = -30; k <= 30; ++k) {
        const double zz = z_mid + k * 0.0005;
        const rt::BondCurveResult r = rt::bond_on_curve(zero_, b, zz);
        if (r.ok)
            pts.append({zz * 1e4, r.clean});
    }
    bd_chart_->set_title(tr("CLEAN PRICE VS Z-SPREAD"));
    bd_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return num(v, 2); });
    bd_chart_->set_time_formatter([](double v) { return DerivCurvesPage::tr("Z-spread %1").arg(fmt::bp(v, 1)); });
    bd_chart_->set_tick_time_formatter([](double v) { return fmt::bp(v, 0); });
    ui::TimeSeriesChart::SeriesSpec s;
    s.name = tr("Clean price");
    s.points = pts;
    s.rebase = false;
    bd_chart_->add_series(s);
    bd_chart_->add_hline(market_clean, tr("Market %1").arg(num(market_clean, 3)), ui::ChartColorRole::Muted);
    if (z.ok)
        bd_chart_->add_price_marker(z.spread * 1e4, market_clean, ui::ChartMarkerKind::Custom, tr("Z"),
                                    tr("Z-spread %1").arg(fmt::bp(z.spread * 1e4, 1)));
    bd_chart_->set_state(pts.size() >= 2 ? ui::ChartState::Ready : ui::ChartState::Empty);
    bd_note_->setText(tr("Cash flows on a schedule rolled back from maturity (the first coupon period may be short), "
                         "discounted on this page's curve. Z-spread: the constant continuously compounded spread over "
                         "the zero curve that reprices the market dirty price. Asset-swap spread (par-par): (curve "
                         "price − market price) / (100 × annuity of the bond's coupon schedule). Accrued interest is "
                         "linear since the previous coupon date. The Treasury curve is a proxy: spreads are over "
                         "Treasuries, not over swaps."));
    if (view_ == kBond)
        status_->setText(tr("Bond off the curve of %1%2").arg(treasury_ ? treasury_->date.toString(Qt::ISODate) : QString(),
                                                              applied_overrides_ > 0 ? tr(" (edited)") : QString()));
}

// ── Carry ────────────────────────────────────────────────────────────────────

void DerivCurvesPage::compute_carry() {
    if (!cy_kpis_)
        return;
    cy_kpis_->clear();
    auto& rows = calc_rows_[kCarry];
    rows.clear();
    const double T = cy_days_->value() / 365.0;
    const bool curve_rate = cy_use_curve_->isChecked() && has_curve();
    if (curve_rate) {
        QSignalBlocker block(cy_rate_);
        cy_rate_->setValue(zero_.rate_at(T) * 100.0);
    }
    cy_rate_->setEnabled(!cy_use_curve_->isChecked());
    cy_use_future_->setEnabled(futures_ && futures_->ok());
    rt::CarryInputs in;
    in.spot = cy_spot_->value();
    in.years = T;
    in.rate = cy_rate_->value() / 100.0;
    in.carry_cost = cy_carry_->value() / 100.0;
    in.income_yield = cy_yield_->value() / 100.0;
    if (cy_d1_days_->value() > 0 && cy_d1_amt_->value() != 0)
        in.income.push_back({cy_d1_days_->value() / 365.0, cy_d1_amt_->value()});
    if (cy_d2_days_->value() > 0 && cy_d2_amt_->value() != 0)
        in.income.push_back({cy_d2_days_->value() / 365.0, cy_d2_amt_->value()});
    in.market_future = cy_market_->value();
    const rt::CarryResult r = rt::futures_carry(in);
    auto tile = [this, &rows](const QString& l, const QString& v, const QString& sub) {
        ui::StatTile* t = cy_kpis_->add_tile(l);
        t->set_value(v);
        t->set_compact(true);
        t->set_sub_label(sub);
        rows.append({l, v, sub});
    };
    cy_chart_->clear();
    if (!r.ok) {
        cy_note_->setText(QString::fromStdString(r.message));
        cy_chart_->set_state(ui::ChartState::Empty, cy_note_->text());
        return;
    }
    tile(tr("FAIR FUTURES PRICE"), num(r.fair, 4), tr("%1 days, rate %2").arg(cy_days_->value()).arg(pct_points(in.rate * 100.0, 3)));
    if (!in.income.empty())
        tile(tr("PV OF CASH INCOME"), num(r.pv_income, 4), tr("discounted at the financing rate"));
    if (r.has_market) {
        tile(tr("MARKET"), num(in.market_future, 4), tr("spot basis %1").arg(signed_num(r.spot_basis, 4)));
        tile(tr("BASIS TO FAIR"), signed_num(r.basis, 4), tr("%1 of fair").arg(fmt::fraction_pct(r.basis / r.fair, 3)));
        tile(tr("IMPLIED REPO"), vol_pct(r.implied_repo, 3), tr("continuous; simple %1").arg(vol_pct(r.implied_repo_simple, 3)));
        tile(tr("IMPLIED INCOME YIELD"), vol_pct(r.implied_yield, 3), tr("continuous, at the financing rate"));
    }
    QVector<QPointF> pts;
    const int horizon = std::max(30, cy_days_->value() * 2);
    for (int k = 0; k <= 40; ++k) {
        rt::CarryInputs x = in;
        x.years = std::max(1.0, horizon * k / 40.0) / 365.0;
        if (curve_rate)
            x.rate = zero_.rate_at(x.years);
        x.market_future = 0;
        const rt::CarryResult rr = rt::futures_carry(x);
        if (rr.ok)
            pts.append({x.years * 365.0, rr.fair});
    }
    cy_chart_->set_title(tr("FAIR FUTURES PRICE VS DAYS TO DELIVERY"));
    cy_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return num(v, 2); });
    cy_chart_->set_time_formatter([](double v) { return DerivCurvesPage::tr("%1 days").arg(num(v, 0)); });
    cy_chart_->set_tick_time_formatter([](double v) { return num(v, 0); });
    ui::TimeSeriesChart::SeriesSpec s;
    s.name = tr("Fair value");
    s.points = pts;
    s.rebase = false;
    cy_chart_->add_series(s);
    cy_chart_->add_hline(in.spot, tr("Spot %1").arg(num(in.spot, 2)), ui::ChartColorRole::Muted);
    if (r.has_market)
        cy_chart_->add_price_marker(cy_days_->value(), in.market_future, ui::ChartMarkerKind::Custom, tr("Market"),
                                    tr("Market %1").arg(num(in.market_future, 4)));
    cy_chart_->set_state(pts.size() >= 2 ? ui::ChartState::Ready : ui::ChartState::Empty);
    cy_note_->setText(tr("Cost of carry: F = (S − PV(cash income)) · e^((r + c − y)·T), r the financing rate%1, c the "
                         "carry cost, y the income / dividend / convenience yield, cash income discounted at r. Implied "
                         "repo: the financing rate that makes the market price fair (continuous; simple-rate version "
                         "beside it). Implied income yield: the y that makes it fair at r. Not an exchange's or broker's "
                         "settlement or delivery-option-adjusted figure.")
                          .arg(curve_rate ? tr(" (zero rate of this page's curve at delivery)") : QString()));
    if (view_ == kCarry)
        status_->setText(curve_rate ? tr("Financing rate from the curve of %1").arg(treasury_ ? treasury_->date.toString(Qt::ISODate)
                                                                                                : QString())
                                    : tr("Financing rate typed by you"));
}

// ── Export / language ────────────────────────────────────────────────────────

QVector<QPair<QString, QString>> DerivCurvesPage::view_inputs() const {
    QVector<QPair<QString, QString>> in;
    switch (view_) {
        case kKeyRates:
            if (kr_instrument_)
                in = {{tr("Instrument"), kr_instrument_->currentText()},
                      {tr("Bond coupon"), pct_points(kr_coupon_->value(), 3)},
                      {tr("Bond maturity"), tr("%1 years").arg(num(kr_maturity_->value(), 2))}};
            break;
        case kSwaption:
            if (sv_expiry_)
                in = {{tr("Expiry"), tr("%1 years").arg(num(sv_expiry_->value(), 2))},
                      {tr("Swap tenor"), tr("%1 years").arg(num(sv_tenor_->value(), 2))},
                      {tr("Strike"), pct_points(sv_strike_->value(), 3)},
                      {tr("Type"), sv_side_->currentText()},
                      {tr("Model"), sv_model_->currentText()},
                      {tr("Volatility"), num(sv_vol_->value(), 2) + sv_vol_->suffix()},
                      {tr("Notional"), money(sv_notional_->value(), kDcpCcy)}};
            break;
        case kBond:
            if (bd_coupon_)
                in = {{tr("Coupon"), pct_points(bd_coupon_->value(), 3)},
                      {tr("Frequency"), bd_freq_->currentText()},
                      {tr("Maturity"), tr("%1 years").arg(num(bd_maturity_->value(), 2))},
                      {bd_kind_->currentText(), num(bd_price_->value(), 4)},
                      {tr("Face held"), money(bd_face_->value(), kDcpCcy)}};
            break;
        case kCarry:
            if (cy_spot_)
                in = {{tr("Spot"), num(cy_spot_->value(), 4)},
                      {tr("Days to delivery"), QString::number(cy_days_->value())},
                      {tr("Financing rate"), pct_points(cy_rate_->value(), 3)},
                      {tr("Carry cost"), pct_points(cy_carry_->value(), 3)},
                      {tr("Income yield"), pct_points(cy_yield_->value(), 3)},
                      {tr("Market futures price"), num(cy_market_->value(), 4)}};
            break;
        default:
            break;
    }
    if (treasury_ && view_ != kFutures)
        in.append({tr("Curve"), tr("U.S. Treasury par curve of %1%2")
                                    .arg(treasury_->date.toString(Qt::ISODate),
                                         applied_overrides_ > 0 ? tr(", %n tenor(s) edited", nullptr, applied_overrides_)
                                                                : QString())});
    return in;
}

ui::ChartExport::Table DerivCurvesPage::export_table() const {
    switch (view_) {
        case kRates:
            return ui::ChartExport::table_from_model(ust_model_);
        case kFutures:
            return fut_model_ ? ui::ChartExport::table_from_model(fut_model_) : ui::ChartExport::Table{};
        case kKeyRates:
            return kr_model_ ? ui::ChartExport::table_from_model(kr_model_) : ui::ChartExport::Table{};
        default: {
            ui::ChartExport::Table t;
            t.headers = {tr("Figure"), tr("Value"), tr("Note")};
            for (const QStringList& r : calc_rows_.value(view_))
                t.rows.append(r);
            const auto in = view_inputs();
            if (!in.isEmpty()) {
                t.rows.append(QStringList());
                for (const auto& [k, v] : in)
                    t.rows.append({k, v, tr("input")});
            }
            return t;
        }
    }
}

QPixmap DerivCurvesPage::export_image() const {
    switch (view_) {
        case kRates: return ust_chart_->render_image();
        case kFutures: return fut_chart_ ? fut_chart_->render_image() : QPixmap();
        case kKeyRates: return kr_chart_ ? ui::ChartExport::grab(kr_chart_) : QPixmap();
        case kSwaption: return sv_chart_ ? sv_chart_->render_image() : QPixmap();
        case kBond: return bd_chart_ ? bd_chart_->render_image() : QPixmap();
        case kCarry: return cy_chart_ ? cy_chart_->render_image() : QPixmap();
        default: return {};
    }
}

report::ReportPayload DerivCurvesPage::report_payload() const {
    report::ReportPayload p;
    p.source = QStringLiteral("Derivatives");
    p.as_of = QDateTime::currentDateTime();
    const ui::KpiStrip* strip = nullptr;
    QString table_title;
    const QLabel* note = nullptr;
    switch (view_) {
        case kRates:
            p.title = applied_overrides_ > 0 ? tr("U.S. Treasury curve (edited)") : tr("U.S. Treasury curve");
            strip = ust_kpis_;
            table_title = tr("Par, zero and forward rates");
            note = ust_note_;
            break;
        case kFutures:
            p.title = tr("Futures curve: %1").arg(root_ ? root_->currentText() : QString());
            strip = fut_kpis_;
            table_title = tr("Contract months");
            note = fut_note_;
            break;
        case kKeyRates:
            p.title = tr("Key-rate DV01");
            strip = kr_kpis_;
            table_title = tr("Key-rate buckets");
            note = kr_note_;
            break;
        case kSwaption:
            p.title = tr("Swaption off the Treasury curve");
            strip = sv_kpis_;
            table_title = tr("Swaption figures");
            note = sv_note_;
            break;
        case kBond:
            p.title = tr("Bond off the Treasury curve: Z-spread and asset-swap spread");
            strip = bd_kpis_;
            table_title = tr("Bond figures");
            note = bd_note_;
            break;
        case kCarry:
            p.title = tr("Futures fair value and implied repo");
            strip = cy_kpis_;
            table_title = tr("Cost-of-carry figures");
            note = cy_note_;
            break;
        default:
            break;
    }
    if (strip) {
        for (int i = 0; i < strip->count(); ++i) {
            report::PayloadKpi k;
            k.label = strip->tile(i)->label();
            k.value = strip->tile(i)->value_text();
            p.kpis.append(k);
        }
    }
    if (view_ == kRates || view_ == kFutures || view_ == kKeyRates) {
        const ui::ChartExport::Table t = export_table();
        report::PayloadTable table;
        table.title = table_title;
        table.columns = t.headers;
        table.rows = t.rows;
        p.tables.append(table);
    }
    report::PayloadChart ch;
    ch.title = p.title;
    ch.image = png_bytes(export_image());
    if (!ch.image.isEmpty())
        p.charts.append(ch);
    p.inputs = view_inputs();
    p.notes = note ? note->text() : QString();
    return p;
}

void DerivCurvesPage::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void DerivCurvesPage::retranslate() {
    book_.apply();
    if (root_)
        for (int i = 0; i < root_->count() && i < static_cast<int>(std::size(kDcpRoots)); ++i)
            root_->setItemText(i, QStringLiteral("%1 · %2").arg(QLatin1String(kDcpRoots[i].code), tr(kDcpRoots[i].name)));
    update_swaption_units(false);
    render_futures();
    render_rates();
    if (view_ >= kKeyRates)
        recompute_view();
}

} // namespace fincept::screens::deriv
