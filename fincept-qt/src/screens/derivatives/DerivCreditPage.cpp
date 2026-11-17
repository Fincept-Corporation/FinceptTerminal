#include "screens/derivatives/DerivCreditPage.h"

#include "core/logging/Logger.h"
#include "services/options/CreditMath.h"
#include "services/options/CurveDataService.h"
#include "services/options/RatesMath.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/StatTile.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

namespace fincept::screens::deriv {

namespace cr = services::options::credit;
namespace rt = services::options::rates;
namespace fmt = fincept::ui::formatting;
using services::options::CurveDataService;

namespace {

constexpr double kDcrNaN = std::numeric_limits<double>::quiet_NaN();
// The sample curve every first visit opens with (an investment-grade-like upward slope).
const double kDcrTenors[] = {0.5, 1, 2, 3, 4, 5, 7, 10};
const double kDcrSpreadsBp[] = {60, 75, 90, 105, 120, 130, 140, 150};

const char* const kDcrFeedNote = QT_TRANSLATE_NOOP(
    "fincept::screens::deriv::DerivCreditPage",
    "No free single-name CDS quote feed exists - type the par spreads by tenor (from a dealer run, your broker or a "
    "data terminal). Sovereign and index levels are not prefilled either: the terminal has no free source for them.");
const char* const kDcrConvNote = QT_TRANSLATE_NOOP(
    "fincept::screens::deriv::DerivCreditPage",
    "Quoted-spread convention: a flat hazard curve fitted to the quoted spread at the position's maturity, coupon and "
    "recovery; upfront = (spread − coupon) × risky annuity, paid by the protection buyer when positive.");

/// "6M", "5Y", "18m", "2.5" → years; NaN when it is not a tenor.
double dcr_parse_tenor(const QString& text) {
    QString s = text.trimmed().toUpper();
    if (s.isEmpty())
        return kDcrNaN;
    double scale = 1.0;
    if (s.endsWith(QLatin1Char('Y'))) {
        s.chop(1);
    } else if (s.endsWith(QLatin1Char('M'))) {
        s.chop(1);
        scale = 1.0 / 12.0;
    } else if (s.endsWith(QLatin1Char('W'))) {
        s.chop(1);
        scale = 7.0 / 365.0;
    }
    s = s.trimmed();
    bool ok = false;
    double v = QLocale::c().toDouble(s, &ok);
    if (!ok)
        v = QLocale().toDouble(s, &ok);
    return ok && std::isfinite(v) ? v * scale : kDcrNaN;
}

double dcr_parse_number(const QString& text) {
    const QString s = text.trimmed();
    bool ok = false;
    double v = QLocale::c().toDouble(s, &ok);
    if (!ok)
        v = QLocale().toDouble(s, &ok);
    return ok && std::isfinite(v) ? v : kDcrNaN;
}

/// 0.5 → "6M", 5 → "5Y", 2.5 → "2.5Y".
QString dcr_tenor_text(double years) {
    const double months = years * 12.0;
    if (years < 1.0 && std::fabs(months - std::round(months)) < 1e-9)
        return QStringLiteral("%1M").arg(static_cast<int>(std::round(months)));
    if (std::fabs(years - std::round(years)) < 1e-9)
        return QStringLiteral("%1Y").arg(static_cast<int>(std::round(years)));
    return QString::number(years, 'g', 6) + QLatin1Char('Y');
}

QJsonValue dcr_num(double v) {
    return std::isfinite(v) ? QJsonValue(v) : QJsonValue();
}

double dcr_get(const QJsonObject& o, const char* key) {
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isDouble() ? v.toDouble() : kDcrNaN;
}

QStandardItem* dcr_cell(const QString& t) {
    auto* it = new QStandardItem(t);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    it->setEditable(false);
    return it;
}

/// Everything a fit needs, copied off the widgets on the GUI thread.
struct DcrIn {
    std::vector<double> tenors;  ///< ascending
    std::vector<double> spreads; ///< decimals
    double recovery = 0.4;
    rt::ZeroCurve curve;
    cr::Conventions cv;
    cr::Position pos;
    double conv_spread = 0.01;  ///< decimal
    double conv_upfront = 0.0;  ///< fraction of notional
};

/// The fit, the position's risks and the converter, off the GUI thread. Empty when cancelled.
QJsonObject dcr_compute(const DcrIn& in, const std::shared_ptr<std::atomic<bool>>& cancel) {
    QJsonObject out;
    const cr::Risk r = cr::position_risk(in.tenors, in.spreads, in.recovery, in.curve, in.cv, in.pos,
                                         [cancel]() { return cancel->load(); });
    if (cancel->load() || r.error == cr::ErrorKind::Cancelled)
        return {};
    if (!r.ok) {
        // The quote the error is about: input order for validation errors, fitted (sorted) order
        // otherwise — the page passes the quotes sorted, so both are the same rows.
        double t = kDcrNaN, s = kDcrNaN;
        if (r.index >= 0 && r.index < static_cast<int>(in.tenors.size())) {
            t = in.tenors[static_cast<size_t>(r.index)];
            s = in.spreads[static_cast<size_t>(r.index)];
        }
        out["ok"] = false;
        out["error"] = static_cast<int>(r.error);
        out["error_tenor"] = dcr_num(t);
        out["error_spread"] = dcr_num(s);
        out["message"] = QString::fromStdString(r.message);
        return out;
    }
    const cr::HazardCurve& hc = r.fit.curve;
    out["ok"] = true;
    out["par_spread"] = dcr_num(r.par_spread);
    out["rpv01"] = dcr_num(r.rpv01);
    out["protection_leg"] = dcr_num(r.protection_leg);
    out["premium_leg"] = dcr_num(r.premium_leg);
    out["pv"] = dcr_num(r.pv);
    out["upfront"] = dcr_num(r.upfront);
    out["upfront_amount"] = dcr_num(r.upfront_amount);
    out["default_prob"] = dcr_num(r.default_probability);
    out["dp5"] = dcr_num(hc.default_probability(5.0));
    out["cs01"] = dcr_num(r.cs01);
    out["recovery01"] = dcr_num(r.recovery01);
    out["ir_dv01"] = dcr_num(r.ir_dv01);
    out["jtd"] = dcr_num(r.jtd);
    out["maturity"] = in.pos.maturity;
    out["coupon"] = in.pos.coupon;
    out["notional"] = in.pos.notional;
    out["buy"] = in.pos.buy_protection;
    out["recovery"] = in.recovery;

    QJsonArray rows;
    for (size_t i = 0; i < r.fit.tenors.size(); ++i) {
        const double T = r.fit.tenors[i];
        rows.append(QJsonArray{T, r.fit.spreads[i], hc.h[i], hc.survival(T), 1.0 - hc.survival(T), r.fit.rpv01[i],
                               r.fit.repriced[i], dcr_num(r.bucket_cs01.size() > i ? r.bucket_cs01[i] : kDcrNaN)});
    }
    out["rows"] = rows;

    // Chart: survival on a fine grid, hazard as steps (each point starts its segment).
    const double t_max = std::max(r.fit.tenors.back(), in.pos.maturity);
    QJsonArray surv, haz;
    for (int k = 0; k <= 120; ++k) {
        const double t = t_max * k / 120.0;
        surv.append(QJsonArray{t, hc.survival(t)});
    }
    double prev = 0.0;
    for (size_t k = 0; k < hc.t.size(); ++k) {
        haz.append(QJsonArray{prev, hc.h[k]});
        prev = hc.t[k];
    }
    haz.append(QJsonArray{std::max(prev, t_max), hc.h.back()});
    out["surv"] = surv;
    out["haz"] = haz;

    // Upfront <-> running spread (quoted flat-spread convention at the position's terms).
    const cr::Conversion u = cr::upfront_from_spread(in.conv_spread, in.pos.coupon, in.pos.maturity, in.recovery,
                                                     in.curve, in.cv);
    QJsonObject cu;
    cu["ok"] = u.ok;
    cu["spread"] = in.conv_spread;
    cu["upfront"] = dcr_num(u.upfront);
    cu["hazard"] = dcr_num(u.hazard);
    cu["rpv01"] = dcr_num(u.rpv01);
    out["to_upfront"] = cu;
    const cr::Conversion s = cr::spread_from_upfront(in.conv_upfront, in.pos.coupon, in.pos.maturity, in.recovery,
                                                     in.curve, in.cv);
    QJsonObject cs;
    cs["ok"] = s.ok;
    cs["upfront"] = in.conv_upfront;
    cs["spread"] = dcr_num(s.spread);
    cs["hazard"] = dcr_num(s.hazard);
    out["to_spread"] = cs;
    return out;
}

} // namespace

DerivCreditPage::DerivCreditPage(QWidget* parent) : QWidget(parent), book_("fincept::screens::deriv::DerivCreditPage") {
    build();
}

DerivCreditPage::~DerivCreditPage() {
    if (cancel_)
        cancel_->store(true);
    if (curve_req_)
        CurveDataService::instance().cancel(curve_req_);
}

void DerivCreditPage::build() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(8);
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);
    root->addWidget(split);
    split->addWidget(build_inputs(split));
    split->addWidget(build_results(split));
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);

    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(200);
    connect(debounce_, &QTimer::timeout, this, &DerivCreditPage::refresh);

    memory_ = new ui::InputMemory(QStringLiteral("derivatives"), QStringLiteral("credit"), this);
    memory_->track(QStringLiteral("quotes"), quotes_memory_);
    memory_->track(QStringLiteral("recovery"), recovery_);
    memory_->track(QStringLiteral("freq"), freq_);
    memory_->track(QStringLiteral("daycount"), daycount_);
    memory_->track(QStringLiteral("discount"), discount_);
    memory_->track(QStringLiteral("flat_rate"), flat_rate_);
    memory_->track(QStringLiteral("side"), side_);
    memory_->track(QStringLiteral("notional"), notional_);
    memory_->track(QStringLiteral("ccy"), ccy_);
    memory_->track(QStringLiteral("maturity"), maturity_);
    memory_->track(QStringLiteral("coupon"), coupon_);
    memory_->track(QStringLiteral("conv_spread"), conv_spread_);
    memory_->track(QStringLiteral("conv_upfront"), conv_upfront_);
    memory_->restore();
    restore_quotes();

    // Connected after the restore: restoring must not start anything.
    for (QDoubleSpinBox* s : {recovery_, flat_rate_, notional_, maturity_, coupon_, conv_spread_, conv_upfront_})
        connect(s, &QDoubleSpinBox::valueChanged, this, &DerivCreditPage::schedule);
    for (QComboBox* c : {freq_, daycount_, side_, ccy_})
        connect(c, &QComboBox::currentIndexChanged, this, &DerivCreditPage::schedule);
    connect(discount_, &QComboBox::currentIndexChanged, this, [this] {
        update_discount_label();
        schedule();
    });
    connect(quotes_, &QStandardItemModel::itemChanged, this, [this] {
        if (filling_)
            return;
        save_quotes();
        schedule();
    });
    connect(&CurveDataService::instance(), &CurveDataService::treasury_updated, this, [this] {
        if (discount_->currentIndex() == 0) {
            update_discount_label();
            schedule();
        }
    });
    update_discount_label();
    retranslate();
}

QWidget* DerivCreditPage::build_inputs(QWidget* parent) {
    auto* scroll = new QScrollArea(parent);
    scroll->setObjectName(QStringLiteral("derivScroll"));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget;
    scroll->setWidget(content); // still empty: moving it is free
    const QFontMetrics fm(font());
    scroll->setMinimumWidth(fm.horizontalAdvance(QLatin1Char('0')) * 46);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(0, 0, 6, 0);
    vl->setSpacing(10);

    // ── The spread curve ──
    Card curve = make_card(content, QStringLiteral("CDS"), tr("PAR SPREAD CURVE (TYPED)"));
    book_.label(curve.title, QT_TR_NOOP("PAR SPREAD CURVE (TYPED)"));
    QLabel* feed = make_note(curve.frame, tr(kDcrFeedNote));
    book_.label(feed, kDcrFeedNote);
    curve.body->addWidget(feed);
    quotes_view_ = new QTableView(curve.frame);
    quotes_ = new QStandardItemModel(0, 2, quotes_view_);
    quotes_view_->setModel(quotes_);
    dense_table(quotes_view_);
    quotes_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    quotes_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    quotes_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    quotes_view_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                                  QAbstractItemView::AnyKeyPressed | QAbstractItemView::SelectedClicked);
    quotes_view_->setToolTip(tr("Tenor (6M, 1Y, 5 …) and par spread in basis points. Edit a cell to refit."));
    quotes_view_->setMinimumHeight(quotes_view_->verticalHeader()->defaultSectionSize() * 9 +
                                   quotes_view_->horizontalHeader()->sizeHint().height());
    curve.body->addWidget(quotes_view_);
    auto* rb = new QWidget(curve.frame);
    auto* rbl = new QHBoxLayout(rb);
    rbl->setContentsMargins(0, 0, 0, 0);
    rbl->setSpacing(6);
    auto* add = make_button(rb, tr("+ ROW"), false);
    book_.button(add, QT_TR_NOOP("+ ROW"));
    auto* del = make_button(rb, tr("REMOVE ROW"), false);
    book_.button(del, QT_TR_NOOP("REMOVE ROW"));
    auto* sample = make_button(rb, tr("SAMPLE CURVE"), false);
    book_.button(sample, QT_TR_NOOP("SAMPLE CURVE"));
    sample->setToolTip(tr("Replace the quotes with the sample curve (60 to 150 bp from 6 months to 10 years)"));
    rbl->addWidget(add);
    rbl->addWidget(del);
    rbl->addWidget(sample);
    rbl->addStretch(1);
    curve.body->addWidget(rb);
    connect(add, &QPushButton::clicked, this, [this] {
        filling_ = true;
        quotes_->appendRow({new QStandardItem(), new QStandardItem()});
        filling_ = false;
        quotes_view_->setCurrentIndex(quotes_->index(quotes_->rowCount() - 1, 0));
        quotes_view_->edit(quotes_->index(quotes_->rowCount() - 1, 0));
    });
    connect(del, &QPushButton::clicked, this, [this] {
        const QModelIndex i = quotes_view_->currentIndex();
        const int row = i.isValid() ? i.row() : quotes_->rowCount() - 1;
        if (row < 0 || quotes_->rowCount() <= 1)
            return;
        quotes_->removeRow(row);
        save_quotes();
        schedule();
    });
    connect(sample, &QPushButton::clicked, this, [this] {
        set_quote_rows({});
        save_quotes();
        schedule();
    });

    auto* gw = new QWidget(curve.frame);
    auto* g = new QGridLayout(gw);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(8);
    g->setVerticalSpacing(4);
    curve.body->addWidget(gw);
    recovery_ = make_spin(gw, 40.0, 0.0, 98.0, 1, QStringLiteral("%"));
    recovery_->setToolTip(tr("Expected recovery on default (40% is the market standard for senior unsecured debt)"));
    freq_ = make_combo(gw, {tr("Quarterly"), tr("Monthly"), tr("Semi-annual"), tr("Annual")});
    book_.combo(freq_, {QT_TR_NOOP("Quarterly"), QT_TR_NOOP("Monthly"), QT_TR_NOOP("Semi-annual"), QT_TR_NOOP("Annual")});
    daycount_ = make_combo(gw, {tr("ACT/360"), tr("ACT/365F")});
    book_.combo(daycount_, {QT_TR_NOOP("ACT/360"), QT_TR_NOOP("ACT/365F")});
    discount_ = make_combo(gw, {tr("U.S. Treasury curve"), tr("Flat rate")});
    book_.combo(discount_, {QT_TR_NOOP("U.S. Treasury curve"), QT_TR_NOOP("Flat rate")});
    flat_rate_ = make_spin(gw, 4.0, -5.0, 50.0, 2, QStringLiteral("%"));
    flat_rate_->setToolTip(tr("Continuously compounded; used for \"Flat rate\" and while the Treasury curve is not loaded"));
    struct F {
        int r, c;
        const char* src;
        QWidget* w;
    };
    for (const F& f : {F{0, 0, QT_TR_NOOP("RECOVERY"), recovery_}, F{0, 1, QT_TR_NOOP("PREMIUM PAYMENTS"), freq_},
                       F{1, 0, QT_TR_NOOP("ACCRUAL DAY COUNT"), daycount_}, F{1, 1, QT_TR_NOOP("DISCOUNTING"), discount_},
                       F{2, 0, QT_TR_NOOP("FLAT RATE (CONT.)"), flat_rate_}}) {
        QLabel* l = add_field(g, f.r, f.c, tr(f.src), f.w);
        book_.label(l, f.src);
    }
    discount_label_ = new QLabel(gw);
    discount_label_->setObjectName(QStringLiteral("derivStatusLine"));
    discount_label_->setWordWrap(true);
    g->addWidget(discount_label_, 6, 0, 1, 2);
    compact_grid(g); // natural-width fields, not two boxes stretched across the column
    vl->addWidget(curve.frame);

    // ── The position ──
    Card pos = make_card(content, QStringLiteral("POS"), tr("POSITION"));
    book_.label(pos.title, QT_TR_NOOP("POSITION"));
    auto* pw = new QWidget(pos.frame);
    auto* pg = new QGridLayout(pw);
    pg->setContentsMargins(0, 0, 0, 0);
    pg->setHorizontalSpacing(8);
    pg->setVerticalSpacing(4);
    pos.body->addWidget(pw);
    side_ = make_combo(pw, {tr("Buy protection"), tr("Sell protection")});
    book_.combo(side_, {QT_TR_NOOP("Buy protection"), QT_TR_NOOP("Sell protection")});
    notional_ = make_spin(pw, 10e6, 1.0, 1e12, 0);
    ccy_ = make_combo(pw, {QStringLiteral("USD"), QStringLiteral("EUR"), QStringLiteral("GBP"), QStringLiteral("JPY"),
                           QStringLiteral("CHF"), QStringLiteral("CAD"), QStringLiteral("AUD"), QStringLiteral("INR")});
    maturity_ = make_spin(pw, 5.0, 0.1, 30.0, 2);
    coupon_ = make_spin(pw, 100.0, 0.0, 10000.0, 1);
    coupon_->setToolTip(tr("Running coupon in basis points (standard contracts pay 100 or 500 bp)"));
    for (const F& f : {F{0, 0, QT_TR_NOOP("SIDE"), side_}, F{0, 1, QT_TR_NOOP("NOTIONAL"), notional_},
                       F{1, 0, QT_TR_NOOP("CURRENCY"), ccy_}, F{1, 1, QT_TR_NOOP("MATURITY (YEARS)"), maturity_},
                       F{2, 0, QT_TR_NOOP("RUNNING COUPON (BP)"), coupon_}}) {
        QLabel* l = add_field(pg, f.r, f.c, tr(f.src), f.w);
        book_.label(l, f.src);
    }
    auto* cb = new QWidget(pw);
    auto* cbl = new QHBoxLayout(cb);
    cbl->setContentsMargins(0, 0, 0, 0);
    cbl->setSpacing(6);
    for (int bp : {100, 500}) {
        auto* b = make_button(cb, tr("%1 BP").arg(bp), false);
        b->setToolTip(tr("Standard coupon of %1 bp").arg(bp));
        connect(b, &QPushButton::clicked, this, [this, bp] { coupon_->setValue(bp); });
        cbl->addWidget(b);
    }
    cbl->addStretch(1);
    pg->addWidget(cb, 5, 1);
    compact_grid(pg);
    vl->addWidget(pos.frame);

    // ── Upfront <-> running spread ──
    Card conv = make_card(content, QStringLiteral("UPF"), tr("UPFRONT ↔ RUNNING SPREAD"));
    book_.label(conv.title, QT_TR_NOOP("UPFRONT ↔ RUNNING SPREAD"));
    QLabel* cn = make_note(conv.frame, tr(kDcrConvNote));
    book_.label(cn, kDcrConvNote);
    conv.body->addWidget(cn);
    auto* uw = new QWidget(conv.frame);
    auto* ug = new QGridLayout(uw);
    ug->setContentsMargins(0, 0, 0, 0);
    ug->setHorizontalSpacing(8);
    ug->setVerticalSpacing(4);
    conv.body->addWidget(uw);
    conv_spread_ = make_spin(uw, 300.0, 0.01, 50000.0, 2);
    conv_upfront_ = make_spin(uw, 5.0, -100.0, 100.0, 4, QStringLiteral("%"));
    QLabel* l1 = add_field(ug, 0, 0, tr("QUOTED SPREAD (BP)"), conv_spread_);
    book_.label(l1, QT_TR_NOOP("QUOTED SPREAD (BP)"));
    QLabel* l2 = add_field(ug, 0, 1, tr("UPFRONT (% OF NOTIONAL)"), conv_upfront_);
    book_.label(l2, QT_TR_NOOP("UPFRONT (% OF NOTIONAL)"));
    conv_spread_out_ = new QLabel(uw);
    conv_spread_out_->setObjectName(QStringLiteral("derivStatusLine"));
    conv_spread_out_->setWordWrap(true);
    conv_spread_out_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    conv_upfront_out_ = new QLabel(uw);
    conv_upfront_out_->setObjectName(QStringLiteral("derivStatusLine"));
    conv_upfront_out_->setWordWrap(true);
    conv_upfront_out_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ug->addWidget(conv_upfront_out_, 2, 0);
    ug->addWidget(conv_spread_out_, 2, 1);
    vl->addWidget(conv.frame);
    vl->addStretch(1);

    // Hidden carrier of the quote rows (InputMemory remembers it).
    quotes_memory_ = new QLineEdit(content);
    quotes_memory_->hide();
    return scroll;
}

QWidget* DerivCreditPage::build_results(QWidget* parent) {
    // One panel: a one-line header (title, the pane's status, RUN and the exports), then the
    // KPIs, the survival / hazard chart over the term table, and the note.
    auto* w = new QWidget(parent);
    w->setObjectName(QStringLiteral("derivPanel"));
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(1, 1, 1, 1);
    vl->setSpacing(0);
    auto* top = new ui::PanelHeader(tr("HAZARD CURVE"), w);
    book_.label(top->title_label(), QT_TR_NOOP("HAZARD CURVE"));
    status_ = new QLabel(top->controls());
    status_->setObjectName(QStringLiteral("derivStatusLine"));
    status_->setWordWrap(true); // wraps rather than widening the panel past a narrow window
    top->add_control(status_, 1);
    auto* run = make_button(top->controls(), tr("RUN"), true);
    book_.button(run, QT_TR_NOOP("RUN"));
    run->setToolTip(tr("Refit the curve and recompute (F5)"));
    connect(run, &QPushButton::clicked, this, &DerivCreditPage::refresh);
    top->add_control(run);
    result_bar_ = new DerivResultBar(QStringLiteral("cds_curve"), top->controls());
    result_bar_->set_table([this] { return export_table(); });
    result_bar_->set_image([this] { return export_image(); });
    result_bar_->set_payload([this] { return report_payload(); });
    top->add_control(result_bar_);
    vl->addWidget(top);

    pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, w);
    auto* content = new QWidget(pane_->content_parent());
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(10, 8, 10, 8);
    cl->setSpacing(6);
    kpis_ = new ui::KpiStrip(content);
    cl->addWidget(kpis_);
    risk_kpis_ = new ui::KpiStrip(content);
    cl->addWidget(risk_kpis_);
    auto* split = new QSplitter(Qt::Vertical, content);
    split->setChildrenCollapsible(false);
    chart_ = make_xy_chart(split, tr("SURVIVAL PROBABILITY AND HAZARD RATE"), QString(), {});
    split->addWidget(chart_);
    table_ = new QTableView(split);
    table_model_ = new QStandardItemModel(table_);
    table_->setModel(table_model_);
    dense_table(table_);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    split->addWidget(table_);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    cl->addWidget(split, 1);
    note_ = make_note(content, QString());
    cl->addWidget(note_);
    pane_->set_content(content);
    pane_->set_persist_limit(0);
    pane_->set_max_age(ui::autoload::kNeverRefresh);
    pane_->set_loading_text(tr("Fitting the hazard curve…"));
    pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Chart);
    pane_->set_status_label(status_);
    pane_->set_start([this](quint64 id) {
        DcrIn in;
        // Quotes: blank rows are skipped; anything else must parse.
        std::vector<std::pair<double, double>> q;
        for (int r = 0; r < quotes_->rowCount(); ++r) {
            const QString ts = quotes_->item(r, 0) ? quotes_->item(r, 0)->text().trimmed() : QString();
            const QString ss = quotes_->item(r, 1) ? quotes_->item(r, 1)->text().trimmed() : QString();
            if (ts.isEmpty() && ss.isEmpty())
                continue;
            const double t = dcr_parse_tenor(ts), s = dcr_parse_number(ss);
            if (!std::isfinite(t)) {
                pane_->fail(id, tr("Row %1: \"%2\" is not a tenor. Use years (5, 2.5) or 6M / 1Y.").arg(r + 1).arg(ts),
                            tr("Check the spreads"));
                return;
            }
            if (!std::isfinite(s)) {
                pane_->fail(id, tr("Row %1: the spread must be a number of basis points.").arg(r + 1),
                            tr("Check the spreads"));
                return;
            }
            q.emplace_back(t, s / 1e4);
        }
        std::stable_sort(q.begin(), q.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& [t, s] : q) {
            in.tenors.push_back(t);
            in.spreads.push_back(s);
        }
        in.recovery = recovery_->value() / 100.0;
        static const int kFreq[] = {4, 12, 2, 1};
        in.cv.frequency = kFreq[std::clamp(freq_->currentIndex(), 0, 3)];
        in.cv.day_count = daycount_->currentIndex() == 1 ? cr::DayCount::Act365F : cr::DayCount::Act360;
        in.cv.accrual_on_default = true;
        rt::ZeroCurve z;
        if (discount_->currentIndex() == 0)
            z = CurveDataService::instance().zero_curve_now();
        in.curve = z.empty() ? cr::flat_curve(flat_rate_->value() / 100.0) : z;
        in.pos.notional = notional_->value();
        in.pos.coupon = coupon_->value() / 1e4;
        in.pos.maturity = maturity_->value();
        in.pos.buy_protection = side_->currentIndex() == 0;
        in.conv_spread = conv_spread_->value() / 1e4;
        in.conv_upfront = conv_upfront_->value() / 100.0;
        dirty_ = false;

        if (cancel_)
            cancel_->store(true);
        cancel_ = std::make_shared<std::atomic<bool>>(false);
        auto cancel = cancel_;
        QPointer<DerivCreditPage> self(this);
        (void)QtConcurrent::run([self, id, in, cancel]() {
            const QJsonObject out = dcr_compute(in, cancel);
            if (cancel->load() || out.isEmpty())
                return;
            QMetaObject::invokeMethod(
                self.data(),
                [self, id, out]() {
                    if (!self || !self->pane_->is_current(id))
                        return;
                    if (out.value(QStringLiteral("ok")).toBool()) {
                        self->pane_->deliver(id, out);
                        return;
                    }
                    // Plain words for the fit failure, in the user's language.
                    const auto kind = static_cast<cr::ErrorKind>(out.value(QStringLiteral("error")).toInt());
                    const double t = dcr_get(out, "error_tenor"), s = dcr_get(out, "error_spread");
                    const QString ten = std::isfinite(t) ? dcr_tenor_text(t) : QString();
                    const QString bp = std::isfinite(s) ? num(s * 1e4, 1) : QString();
                    QString text;
                    switch (kind) {
                        case cr::ErrorKind::NoQuotes:
                            text = DerivCreditPage::tr("Type at least one tenor and par spread.");
                            break;
                        case cr::ErrorKind::BadTenor:
                            text = DerivCreditPage::tr("Every tenor must be a positive number of years (100 at most).");
                            break;
                        case cr::ErrorKind::DuplicateTenor:
                            text = DerivCreditPage::tr("The %1 tenor is listed twice - keep one.").arg(ten);
                            break;
                        case cr::ErrorKind::BadSpread:
                            text = DerivCreditPage::tr("The %1 spread must be a positive number of basis points.").arg(ten);
                            break;
                        case cr::ErrorKind::BadRecovery:
                            text = DerivCreditPage::tr("Recovery must be between 0% and 98%.");
                            break;
                        case cr::ErrorKind::BadMaturity:
                            text = DerivCreditPage::tr("The maturity must be a positive number of years.");
                            break;
                        case cr::ErrorKind::NegativeHazard:
                            text = DerivCreditPage::tr("The %1 spread (%2 bp) is too low after the shorter tenors: no "
                                                       "positive default intensity reproduces it. Raise it or lower the "
                                                       "shorter spreads.")
                                       .arg(ten, bp);
                            break;
                        default:
                            text = DerivCreditPage::tr("No default intensity reproduces the %1 spread (%2 bp). Check it.")
                                       .arg(ten, bp);
                            break;
                    }
                    LOG_DEBUG("Derivatives", QStringLiteral("Credit curve fit refused kind=%1").arg(static_cast<int>(kind)));
                    self->pane_->fail(id, text, DerivCreditPage::tr("Check the spreads"));
                },
                Qt::QueuedConnection);
        });
    });
    pane_->set_cancel([this](quint64) {
        if (cancel_)
            cancel_->store(true);
    });
    pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render(p.toObject()); });
    vl->addWidget(pane_, 1);
    return w;
}

// ── Inputs ───────────────────────────────────────────────────────────────────

void DerivCreditPage::set_quote_rows(const QVector<QPair<QString, QString>>& rows_in) {
    QVector<QPair<QString, QString>> rows = rows_in;
    if (rows.isEmpty())
        for (size_t i = 0; i < std::size(kDcrTenors); ++i)
            rows.append({dcr_tenor_text(kDcrTenors[i]), QString::number(kDcrSpreadsBp[i])});
    filling_ = true;
    quotes_->removeRows(0, quotes_->rowCount());
    for (const auto& [t, s] : rows) {
        auto* ti = new QStandardItem(t);
        auto* si = new QStandardItem(s);
        si->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        quotes_->appendRow({ti, si});
    }
    filling_ = false;
}

void DerivCreditPage::save_quotes() {
    QJsonArray a;
    for (int r = 0; r < quotes_->rowCount(); ++r) {
        const QString t = quotes_->item(r, 0) ? quotes_->item(r, 0)->text().trimmed() : QString();
        const QString s = quotes_->item(r, 1) ? quotes_->item(r, 1)->text().trimmed() : QString();
        if (t.isEmpty() && s.isEmpty())
            continue;
        a.append(QJsonArray{t, s});
    }
    quotes_memory_->setText(QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact)));
}

void DerivCreditPage::restore_quotes() {
    QVector<QPair<QString, QString>> rows;
    for (const QJsonValue& v : QJsonDocument::fromJson(quotes_memory_->text().toUtf8()).array()) {
        const QJsonArray r = v.toArray();
        if (r.size() == 2)
            rows.append({r[0].toString(), r[1].toString()});
    }
    set_quote_rows(rows); // empty: the sample curve
}

void DerivCreditPage::update_discount_label() {
    if (!discount_label_)
        return;
    auto& svc = CurveDataService::instance();
    if (discount_->currentIndex() == 1) {
        discount_label_->setText(tr("Discounting at a flat %1 (continuous).").arg(pct_points(flat_rate_->value())));
        return;
    }
    if (auto c = svc.treasury_now(); c && c->ok()) {
        discount_label_->setText(tr("Discounting: U.S. Treasury curve of %1, bootstrapped to zero rates (a proxy for the "
                                    "risk-free curve; %2).")
                                     .arg(c->date.toString(Qt::ISODate), c->source_label));
        return;
    }
    discount_label_->setText(tr("The Treasury curve is not loaded yet - discounting at the flat %1 until it arrives.")
                                 .arg(pct_points(flat_rate_->value())));
    if (!curve_requested_) {
        // Free and keyless (U.S. Treasury, public domain); one request per page, answered from the
        // service's cache when another page loaded it already.
        curve_requested_ = true;
        curve_req_ = svc.treasury_curve(false, this, [this](const CurveDataService::TreasuryReply& r) {
            curve_req_ = 0;
            if (!r.curve)
                return;
            update_discount_label();
            if (discount_->currentIndex() == 0)
                schedule();
        });
    }
}

void DerivCreditPage::schedule() {
    dirty_ = true;
    if (discount_->currentIndex() == 1)
        update_discount_label(); // the flat rate shown follows the spin box
    debounce_->start();
}

void DerivCreditPage::refresh() {
    debounce_->stop();
    pane_->run();
}

QString DerivCreditPage::currency() const {
    return ccy_ ? ccy_->currentText() : QStringLiteral("USD");
}

void DerivCreditPage::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (dirty_ && pane_->has_result())
        schedule(); // an edit whose run was cancelled when the page was left
}

void DerivCreditPage::hideEvent(QHideEvent* e) {
    if (debounce_->isActive()) {
        debounce_->stop();
        dirty_ = true;
    }
    if (pane_->is_running()) {
        pane_->cancel();
        dirty_ = true;
    }
    QWidget::hideEvent(e);
}

// ── Results ──────────────────────────────────────────────────────────────────

void DerivCreditPage::render(const QJsonObject& p) {
    last_ = p;
    const QString c = currency();
    const double notional = dcr_get(p, "notional");
    const bool buyer = p.value(QStringLiteral("buy")).toBool(true);
    const double maturity = dcr_get(p, "maturity");
    const double coupon = dcr_get(p, "coupon");
    auto text_or_dash = [](double v, const std::function<QString(double)>& f) {
        return std::isfinite(v) ? f(v) : QStringLiteral("—");
    };
    auto tile = [](ui::KpiStrip* strip, const QString& label, const QString& value, const QString& sub, double sign = 0) {
        ui::StatTile* t = strip->add_tile(label);
        t->set_value(value);
        t->set_compact(true);
        t->set_sub_label(sub);
        if (std::isfinite(sign) && sign != 0)
            t->set_tone(sign > 0 ? ui::StatTile::Tone::Positive : ui::StatTile::Tone::Negative);
        return t;
    };
    auto smoney = [c](double v) { return signed_money(v, c); };

    kpis_->clear();
    risk_kpis_->clear();
    const double par = dcr_get(p, "par_spread");
    tile(kpis_, tr("PAR SPREAD"), text_or_dash(par, [](double v) { return tr("%1 bp").arg(num(v * 1e4, 2)); }),
         tr("%1-year, off the fitted curve").arg(num(maturity, 2)));
    const double upf = dcr_get(p, "upfront"), upf_amt = dcr_get(p, "upfront_amount");
    tile(kpis_, tr("UPFRONT"), text_or_dash(upf, [](double v) { return fmt::fraction_pct(v, 3, true); }),
         std::isfinite(upf_amt) ? (upf_amt >= 0 ? tr("buyer pays %1 at %2 bp").arg(money(upf_amt, c), num(coupon * 1e4, 0))
                                                : tr("buyer receives %1 at %2 bp")
                                                      .arg(money(-upf_amt, c), num(coupon * 1e4, 0)))
                                : QString());
    const double pv = dcr_get(p, "pv");
    tile(kpis_, tr("POSITION VALUE"), text_or_dash(pv, smoney),
         buyer ? tr("to the protection buyer") : tr("to the protection seller"), pv);
    tile(kpis_, tr("RISKY ANNUITY"), text_or_dash(dcr_get(p, "rpv01"), [](double v) { return num(v, 4); }),
         tr("RPV01 per 1 of notional"));
    tile(kpis_, tr("DEFAULT PROBABILITY"),
         text_or_dash(dcr_get(p, "default_prob"), [](double v) { return fmt::fraction_pct(v, 2, false); }),
         tr("to the maturity, risk-neutral"));
    tile(kpis_, tr("5Y DEFAULT PROBABILITY"),
         text_or_dash(dcr_get(p, "dp5"), [](double v) { return fmt::fraction_pct(v, 2, false); }), tr("risk-neutral"));
    const double cs01 = dcr_get(p, "cs01"), rec01 = dcr_get(p, "recovery01"), ir01 = dcr_get(p, "ir_dv01"),
                 jtd = dcr_get(p, "jtd");
    tile(risk_kpis_, tr("CS01"), text_or_dash(cs01, smoney), tr("+1 bp on every quote, refitted"), cs01);
    tile(risk_kpis_, tr("RECOVERY 01"), text_or_dash(rec01, smoney), tr("+1% recovery, refitted"), rec01);
    tile(risk_kpis_, tr("RATES DV01"), text_or_dash(ir01, smoney), tr("+1 bp on the discount curve"), ir01);
    tile(risk_kpis_, tr("JUMP TO DEFAULT"), text_or_dash(jtd, smoney), tr("P&L if the name defaulted today"), jtd);
    tile(risk_kpis_, tr("PROTECTION LEG"),
         text_or_dash(dcr_get(p, "protection_leg"), [c](double v) { return money(v, c); }), tr("present value"));
    tile(risk_kpis_, tr("PREMIUM LEG"), text_or_dash(dcr_get(p, "premium_leg"), [c](double v) { return money(v, c); }),
         tr("present value at the coupon"));

    // Chart.
    QVector<QPointF> surv, haz;
    for (const QJsonValue& v : p.value(QStringLiteral("surv")).toArray())
        surv.append({v.toArray().at(0).toDouble(), v.toArray().at(1).toDouble()});
    for (const QJsonValue& v : p.value(QStringLiteral("haz")).toArray())
        haz.append({v.toArray().at(0).toDouble(), v.toArray().at(1).toDouble()});
    chart_->clear();
    chart_->set_title(tr("SURVIVAL PROBABILITY AND HAZARD RATE"));
    chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return fmt::fraction_pct(v, 1, false); });
    chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Right, [](double v) { return fmt::fraction_pct(v, 2, false); });
    chart_->set_time_formatter([](double v) { return DerivCreditPage::tr("%1 years").arg(num(v, 2)); });
    chart_->set_tick_time_formatter([](double v) { return num(v, 0); });
    ui::TimeSeriesChart::SeriesSpec s;
    s.name = tr("Survival probability");
    s.points = surv;
    s.rebase = false;
    chart_->add_series(s);
    ui::TimeSeriesChart::SeriesSpec h;
    h.name = tr("Hazard rate (right axis)");
    h.points = haz;
    h.style = ui::TimeSeriesChart::Style::Step;
    h.axis = ui::TimeSeriesChart::Axis::Right;
    h.dashed = true;
    h.rebase = false;
    chart_->add_series(h);
    chart_->set_state(surv.size() >= 2 ? ui::ChartState::Ready : ui::ChartState::Empty);

    // Table.
    table_model_->clear();
    table_model_->setHorizontalHeaderLabels({tr("TENOR"), tr("QUOTE"), tr("HAZARD / YEAR"), tr("SURVIVAL"),
                                             tr("DEFAULT PROB."), tr("RISKY ANNUITY"), tr("REPRICED"),
                                             tr("CS01 BUCKET")});
    for (const QJsonValue& v : p.value(QStringLiteral("rows")).toArray()) {
        const QJsonArray r = v.toArray();
        const double bucket = r.at(7).isDouble() ? r.at(7).toDouble() : kDcrNaN;
        QList<QStandardItem*> row;
        auto* t = new QStandardItem(dcr_tenor_text(r.at(0).toDouble()));
        t->setEditable(false);
        row << t << dcr_cell(tr("%1 bp").arg(num(r.at(1).toDouble() * 1e4, 2)))
            << dcr_cell(fmt::fraction_pct(r.at(2).toDouble(), 3, false))
            << dcr_cell(fmt::fraction_pct(r.at(3).toDouble(), 3, false))
            << dcr_cell(fmt::fraction_pct(r.at(4).toDouble(), 3, false)) << dcr_cell(num(r.at(5).toDouble(), 4))
            << dcr_cell(tr("%1 bp").arg(num(r.at(6).toDouble() * 1e4, 4)))
            << dcr_cell(std::isfinite(bucket) ? signed_money(bucket, c) : QStringLiteral("—"));
        table_model_->appendRow(row);
    }

    // Converter.
    const QJsonObject tu = p.value(QStringLiteral("to_upfront")).toObject();
    const QJsonObject ts = p.value(QStringLiteral("to_spread")).toObject();
    if (tu.value(QStringLiteral("ok")).toBool()) {
        const double u = dcr_get(tu, "upfront");
        conv_upfront_out_->setText(tr("%1 bp quoted → upfront %2 of notional (%3) at a %4 bp coupon; flat hazard %5.")
                                       .arg(num(dcr_get(tu, "spread") * 1e4, 2), fmt::fraction_pct(u, 4, true),
                                            signed_money(u * notional, c), num(coupon * 1e4, 0),
                                            fmt::fraction_pct(dcr_get(tu, "hazard"), 3, false)));
    } else {
        conv_upfront_out_->setText(tr("No upfront for this spread: check the spread and the maturity."));
    }
    if (ts.value(QStringLiteral("ok")).toBool()) {
        conv_spread_out_->setText(tr("Upfront %1 → quoted spread %2 bp at a %3 bp coupon; flat hazard %4.")
                                      .arg(fmt::fraction_pct(dcr_get(ts, "upfront"), 4, true),
                                           num(dcr_get(ts, "spread") * 1e4, 2), num(coupon * 1e4, 0),
                                           fmt::fraction_pct(dcr_get(ts, "hazard"), 3, false)));
    } else {
        conv_spread_out_->setText(tr("No positive spread gives this upfront at this coupon: it must lie above "
                                     "−coupon × risky annuity and below 1 − recovery."));
    }

    note_->setText(tr("Method: piecewise-constant hazard rates bootstrapped tenor by tenor so every quote prices to par "
                      "(ISDA CDS Standard Model, simplified: premiums in arrears on a schedule rolled back from the "
                      "maturity with a short first period starting today, accrual on default, no IMM-date roll, no "
                      "accrued premium at entry). Protection pays 1 − recovery at default; both legs are integrated "
                      "exactly under flat hazard and flat forward rates. Sensitivities refit the curve. Jump to "
                      "default ignores accrued premium. References: O'Kane (2008) Modelling Single-name and Multi-name "
                      "Credit Derivatives, ch. 5-6; Hull, Options, Futures and Other Derivatives, ch. 25."));
}

// ── Export ───────────────────────────────────────────────────────────────────

ui::ChartExport::Table DerivCreditPage::export_table() const {
    ui::ChartExport::Table t = ui::ChartExport::table_from_model(table_model_);
    t.rows.append(QStringList());
    for (const ui::KpiStrip* strip : {kpis_, risk_kpis_})
        for (int i = 0; i < strip->count(); ++i)
            t.rows.append({strip->tile(i)->label(), strip->tile(i)->value_text()});
    return t;
}

QPixmap DerivCreditPage::export_image() const {
    return chart_ ? chart_->render_image() : QPixmap();
}

report::ReportPayload DerivCreditPage::report_payload() const {
    report::ReportPayload p;
    p.source = QStringLiteral("Derivatives");
    const bool buyer = last_.value(QStringLiteral("buy")).toBool(side_->currentIndex() == 0);
    p.title = tr("Credit default swap: %1, %2 years, %3 bp coupon")
                  .arg(buyer ? tr("protection bought") : tr("protection sold"), num(maturity_->value(), 2),
                       num(coupon_->value(), 0));
    p.as_of = pane_ && pane_->as_of().isValid() ? pane_->as_of() : QDateTime::currentDateTime();
    p.kpi_title = tr("Position and risks");
    for (const ui::KpiStrip* strip : {kpis_, risk_kpis_})
        for (int i = 0; i < strip->count(); ++i) {
            report::PayloadKpi k;
            k.label = strip->tile(i)->label();
            k.value = strip->tile(i)->value_text();
            p.kpis.append(k);
        }
    const ui::ChartExport::Table t = ui::ChartExport::table_from_model(table_model_);
    report::PayloadTable curve;
    curve.title = tr("Fitted hazard curve");
    curve.columns = t.headers;
    curve.rows = t.rows;
    curve.note = tr("Par spreads typed by the user; hazards bootstrapped so every quote reprices to par.");
    p.tables.append(curve);
    report::PayloadChart ch;
    ch.title = tr("Survival probability and hazard rate");
    ch.image = png_bytes(export_image());
    if (!ch.image.isEmpty())
        p.charts.append(ch);
    p.inputs = {{tr("Side"), side_->currentText()},
                {tr("Notional"), money(notional_->value(), currency())},
                {tr("Maturity"), tr("%1 years").arg(num(maturity_->value(), 2))},
                {tr("Running coupon"), tr("%1 bp").arg(num(coupon_->value(), 1))},
                {tr("Recovery"), pct_points(recovery_->value(), 1)},
                {tr("Premium payments"), freq_->currentText()},
                {tr("Accrual day count"), daycount_->currentText()},
                {tr("Discounting"), discount_label_->text()}};
    p.notes = note_->text();
    return p;
}

// ── Language ─────────────────────────────────────────────────────────────────

void DerivCreditPage::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void DerivCreditPage::retranslate() {
    book_.apply();
    filling_ = true;
    quotes_->setHorizontalHeaderLabels({tr("TENOR"), tr("PAR SPREAD (BP)")});
    filling_ = false;
    update_discount_label();
    if (!last_.isEmpty())
        render(last_);
}

} // namespace fincept::screens::deriv
