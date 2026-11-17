#include "screens/derivatives/DerivScenarioPage.h"

#include "services/options/OptionModels.h"
#include "services/options/ScenarioMath.h"
#include "services/options/StrategyMath.h"
#include "ui/charts/FanChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/ScenarioGrid.h"
#include "ui/widgets/SensitivityTable.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cmath>

namespace fincept::screens::deriv {

namespace st = services::options::strategy;
namespace sc = services::options::scenario;
namespace fmt = fincept::ui::formatting;

namespace {

struct DspIn {
    st::Spec spec;
    st::Market m;
    double atm_iv = 0.2;
    std::vector<double> spot_pcts, vol_pts;
    double days = 0;
    double rate_bp = 0;
    std::vector<sc::Shock> stress;
    double cone_days = 30;
};

QJsonArray dsp_arr(const std::vector<double>& v) {
    QJsonArray a;
    for (double d : v)
        a.append(std::isfinite(d) ? d : 0.0);
    return a;
}

QVector<double> dsp_vec(const QJsonValue& v) {
    QVector<double> out;
    for (const QJsonValue& x : v.toArray())
        out.append(x.toDouble());
    return out;
}

QVector<QVector<double>> dsp_mat(const QJsonValue& v) {
    QVector<QVector<double>> out;
    for (const QJsonValue& r : v.toArray())
        out.append(dsp_vec(r));
    return out;
}

/// Everything the page shows, computed off the GUI thread. Empty when cancelled.
QJsonObject dsp_compute(const DspIn& in, const std::shared_ptr<std::atomic<bool>>& cancel) {
    QJsonArray pnl, delta, vega;
    for (double sp : in.spot_pcts) {
        if (cancel->load())
            return {};
        QJsonArray pr, dr, vr;
        for (double vp : in.vol_pts) {
            const sc::ShockResult r = sc::evaluate(in.spec, in.m, {sp, vp, in.rate_bp, in.days});
            pr.append(r.pnl);
            dr.append(r.delta);
            vr.append(r.vega_pt);
        }
        pnl.append(pr);
        delta.append(dr);
        vega.append(vr);
    }
    QJsonArray stress;
    for (const sc::Shock& s : in.stress) {
        const sc::ShockResult r = sc::evaluate(in.spec, in.m, s);
        stress.append(QJsonArray{r.pnl, r.value, r.delta, r.gamma, r.vega_pt, r.theta_day});
    }
    const sc::Cone cone = sc::probability_cone(in.m.spot, in.m.r, in.m.q, in.atm_iv, std::max(in.cone_days, 5.0), 61);
    QJsonObject c;
    c["days"] = dsp_arr(cone.days);
    c["p2"] = dsp_arr(cone.p2);
    c["p16"] = dsp_arr(cone.p16);
    c["p50"] = dsp_arr(cone.p50);
    c["p84"] = dsp_arr(cone.p84);
    c["p98"] = dsp_arr(cone.p98);
    const std::vector<double> bes = st::breakevens(in.spec, in.m, in.m.spot * 0.3, in.m.spot * 3.0);
    QJsonObject out;
    out["spot_pcts"] = dsp_arr(in.spot_pcts);
    out["vol_pts"] = dsp_arr(in.vol_pts);
    out["pnl"] = pnl;
    out["delta"] = delta;
    out["vega"] = vega;
    out["stress"] = stress;
    out["cone"] = c;
    out["breakevens"] = dsp_arr(bes);
    out["value"] = st::position_value(in.spec, in.m, in.m.spot);
    out["spot"] = in.m.spot;
    out["atm_iv"] = in.atm_iv;
    out["days"] = in.days;
    out["rate_bp"] = in.rate_bp;
    return out;
}

} // namespace

DerivScenarioPage::DerivScenarioPage(DerivContext* ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx), book_("fincept::screens::deriv::DerivScenarioPage") {
    build();
}

DerivScenarioPage::~DerivScenarioPage() {
    if (cancel_)
        cancel_->store(true);
}

void DerivScenarioPage::build() {
    // A one-line toolbar strip across the page, then the shock inputs (natural-width columns),
    // the view tabs and the result with a narrow margin.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto* root = new QVBoxLayout;
    root->setContentsMargins(10, 8, 10, 6);
    root->setSpacing(6);

    QHBoxLayout* tl = nullptr;
    QWidget* top = make_strip(this, &tl);
    tl->setContentsMargins(10, 3, 10, 3);
    tl->setSpacing(8);
    outer->addWidget(top);
    outer->addLayout(root, 1);
    source_ = make_combo(top, {tr("Position from the STRATEGY page"), tr("Single option (inputs below)"),
                               tr("Position from the BOOK")});
    book_.combo(source_, {QT_TR_NOOP("Position from the STRATEGY page"), QT_TR_NOOP("Single option (inputs below)"),
                          QT_TR_NOOP("Position from the BOOK")});
    tl->addWidget(source_);
    summary_ = new QLabel(top);
    summary_->setObjectName(QStringLiteral("derivPanelTitle"));
    summary_->setWordWrap(true); // both wrap rather than widening the strip past a narrow window
    tl->addWidget(summary_);
    status_ = new QLabel(top);
    status_->setObjectName(QStringLiteral("derivStatusLine"));
    status_->setWordWrap(true);
    tl->addWidget(status_, 1);
    auto* run = make_button(top, tr("RUN"), true);
    book_.button(run, QT_TR_NOOP("RUN"));
    run->setToolTip(tr("Recompute (F5)"));
    connect(run, &QPushButton::clicked, this, &DerivScenarioPage::refresh);
    tl->addWidget(run);
    result_bar_ = new DerivResultBar(QStringLiteral("option_scenarios"), top);
    result_bar_->set_table([this] { return export_table(); });
    result_bar_->set_image([this] { return export_image(); });
    result_bar_->set_payload([this] { return report_payload(); });
    tl->addWidget(result_bar_);

    // Inputs.
    auto* inputs = new QWidget(this);
    auto* g = new QGridLayout(inputs);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(10);
    g->setVerticalSpacing(4);
    spot_range_ = make_spin(inputs, 20, 1, 90, 1, QStringLiteral("%"));
    spot_steps_ = make_int_spin(inputs, 8, 1, 50);
    vol_range_ = make_spin(inputs, 10, 0.5, 100, 1);
    vol_steps_ = make_int_spin(inputs, 4, 1, 20);
    rate_bp_ = make_spin(inputs, 0, -1000, 1000, 0);
    days_ = new QSlider(Qt::Horizontal, inputs);
    days_->setRange(0, 30);
    days_label_ = new QLabel(inputs);
    days_label_->setObjectName(QStringLiteral("derivStatusLine"));
    struct F {
        int col;
        const char* src;
        QWidget* w;
    };
    for (const F& f : {F{0, QT_TR_NOOP("SPOT SHOCK ± (%)"), spot_range_}, F{1, QT_TR_NOOP("SPOT STEPS EACH SIDE"), spot_steps_},
                       F{2, QT_TR_NOOP("VOL SHOCK ± (POINTS)"), vol_range_}, F{3, QT_TR_NOOP("VOL STEPS EACH SIDE"), vol_steps_},
                       F{4, QT_TR_NOOP("RATE SHIFT (BP)"), rate_bp_}}) {
        QLabel* l = add_field(g, 0, f.col, tr(f.src), f.w);
        book_.label(l, f.src);
    }
    QLabel* dl = add_field(g, 0, 5, tr("DAYS FORWARD (TIME DECAY)"), days_);
    book_.label(dl, QT_TR_NOOP("DAYS FORWARD (TIME DECAY)"));
    g->addWidget(days_label_, 1, 6);
    compact_grid(g); // natural-width columns; the spare width stays at the right
    root->addWidget(inputs);

    single_ = new QWidget(this);
    auto* sg = new QGridLayout(single_);
    sg->setContentsMargins(0, 0, 0, 0);
    sg->setHorizontalSpacing(10);
    sg->setVerticalSpacing(4);
    s_spot_ = make_spin(single_, 100, 0.01, 1e8, 2);
    s_strike_ = make_spin(single_, 100, 0.01, 1e8, 2);
    s_days_ = make_int_spin(single_, 30, 1, 3650);
    s_iv_ = make_spin(single_, 25, 0.1, 500, 2, QStringLiteral("%"));
    s_rate_ = make_spin(single_, 4, -20, 100, 2, QStringLiteral("%"));
    s_type_ = make_combo(single_, {tr("Call"), tr("Put")});
    book_.combo(s_type_, {QT_TR_NOOP("Call"), QT_TR_NOOP("Put")});
    s_qty_ = make_spin(single_, 1, -1e6, 1e6, 0);
    for (const F& f : {F{0, QT_TR_NOOP("SPOT"), s_spot_}, F{1, QT_TR_NOOP("STRIKE"), s_strike_},
                       F{2, QT_TR_NOOP("DAYS TO EXPIRY"), s_days_}, F{3, QT_TR_NOOP("IMPLIED VOL"), s_iv_},
                       F{4, QT_TR_NOOP("RATE (CONT.)"), s_rate_}, F{5, QT_TR_NOOP("TYPE"), s_type_},
                       F{6, QT_TR_NOOP("CONTRACTS (− = SHORT)"), s_qty_}}) {
        QLabel* l = add_field(sg, 0, f.col, tr(f.src), f.w);
        book_.label(l, f.src);
    }
    compact_grid(sg);
    root->addWidget(single_);

    // Views.
    QHBoxLayout* tb = nullptr;
    QWidget* tabs = make_strip(this, &tb);
    tb->setContentsMargins(0, 0, 4, 0);
    tb->setSpacing(2);
    static const char* const kTabs[] = {QT_TR_NOOP("SPOT × VOL GRID"), QT_TR_NOOP("PROBABILITY CONE"),
                                        QT_TR_NOOP("STRESS PRESETS")};
    for (int i = 0; i < 3; ++i) {
        auto* b = new QPushButton(tr(kTabs[i]), tabs);
        b->setObjectName(QStringLiteral("derivSubBtn"));
        b->setCursor(Qt::PointingHandCursor);
        book_.button(b, kTabs[i]);
        connect(b, &QPushButton::clicked, this, [this, i] { set_view(i); });
        tb->addWidget(b);
        tab_btns_.append(b);
    }
    tb->addStretch(1);
    root->addWidget(tabs);

    pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, this);
    views_ = new QStackedWidget(pane_->content_parent());
    grid_ = new ui::SensitivityTable(views_);
    grid_->set_export_buttons_visible(false); // the page's COPY / CSV / PNG / ADD TO REPORT row
    grid_->set_export_name(QStringLiteral("option_scenario_grid"));
    views_->addWidget(grid_);
    auto* cone_host = new QWidget(views_);
    auto* cl = new QVBoxLayout(cone_host);
    cl->setContentsMargins(0, 0, 0, 0);
    cone_ = new ui::FanChart(cone_host);
    cone_->set_value_formatter([](double v) { return fmt::number(v, 2); });
    cone_->set_x_formatter([](double v) { return DerivScenarioPage::tr("%1 d").arg(fmt::number(v, 0)); });
    cl->addWidget(cone_, 1);
    cone_note_ = make_note(cone_host, QString());
    cl->addWidget(cone_note_);
    views_->addWidget(cone_host);
    stress_ = new ui::ScenarioGrid(views_);
    stress_->set_columns({{tr("Spot %"), 1, tr("Spot change in percent")},
                          {tr("Vol pts"), 1, tr("Change of every leg's IV in vol points")},
                          {tr("Rate bp"), 0, tr("Change of the risk-free rate in basis points")},
                          {tr("Days"), 0, tr("Calendar days forward")}},
                         {{tr("P&L"), [this](double v) { return signed_money(v, currency_); }, true,
                           tr("Against the position's current model value")},
                          {tr("Value"), [this](double v) { return money(v, currency_); }, false, {}},
                          {tr("Delta"), [](double v) { return num(v, 1); }, false, tr("Shares")},
                          {tr("Gamma"), [](double v) { return num(v, 3); }, false, {}},
                          {tr("Vega / pt"), [this](double v) { return money(v, currency_); }, false, {}},
                          {tr("Theta / day"), [this](double v) { return money(v, currency_); }, false, {}}});
    views_->addWidget(stress_);
    pane_->set_content(views_);
    pane_->set_persist_limit(0);
    pane_->set_max_age(ui::autoload::kNeverRefresh);
    pane_->set_loading_text(tr("Calculating scenarios…"));
    pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Table);
    pane_->set_status_label(status_);
    pane_->set_start([this](quint64 id) {
        DspIn in;
        QString label;
        if (!position(&in.spec, &in.m, &in.atm_iv, &label)) {
            pane_->fail(id, tr("Build a position on the STRATEGY page, or choose \"Single option\" above."),
                        tr("No position to analyse"));
            return;
        }
        summary_->setText(label);
        const auto sp = sc::symmetric_axis(spot_range_->value(), spot_steps_->value());
        const auto vp = sc::symmetric_axis(vol_range_->value(), vol_steps_->value());
        in.spot_pcts = sp;
        in.vol_pts = vp;
        in.days = days_->value();
        in.rate_bp = rate_bp_->value();
        for (const ui::ScenarioRow& r : stress_->scenarios())
            in.stress.push_back({r.shocks.value(0), r.shocks.value(1), r.shocks.value(2), r.shocks.value(3)});
        in.cone_days = std::max(30.0, st::first_expiry(in.spec) * 365.0);
        if (cancel_)
            cancel_->store(true);
        cancel_ = std::make_shared<std::atomic<bool>>(false);
        auto cancel = cancel_;
        QPointer<DerivScenarioPage> self(this);
        (void)QtConcurrent::run([self, id, in, cancel]() {
            QJsonObject out = dsp_compute(in, cancel);
            if (cancel->load())
                return;
            QMetaObject::invokeMethod(
                self.data(),
                [self, id, out]() {
                    if (self && self->pane_->is_current(id))
                        self->pane_->deliver(id, out);
                },
                Qt::QueuedConnection);
        });
    });
    pane_->set_cancel([this](quint64) {
        if (cancel_)
            cancel_->store(true);
    });
    pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render(p.toObject()); });
    root->addWidget(pane_, 1);
    note_ = make_note(this, QString());
    root->addWidget(note_);

    stress_memory_ = new QLineEdit(this);
    stress_memory_->hide();
    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(200);
    connect(debounce_, &QTimer::timeout, this, &DerivScenarioPage::refresh);

    memory_ = new ui::InputMemory(QStringLiteral("derivatives"), QStringLiteral("scenarios"), this);
    memory_->track(QStringLiteral("source"), source_);
    memory_->track(QStringLiteral("spot_range"), spot_range_);
    memory_->track(QStringLiteral("spot_steps"), spot_steps_);
    memory_->track(QStringLiteral("vol_range"), vol_range_);
    memory_->track(QStringLiteral("vol_steps"), vol_steps_);
    memory_->track(QStringLiteral("rate_bp"), rate_bp_);
    memory_->track(QStringLiteral("s_spot"), s_spot_);
    memory_->track(QStringLiteral("s_strike"), s_strike_);
    memory_->track(QStringLiteral("s_days"), s_days_);
    memory_->track(QStringLiteral("s_iv"), s_iv_);
    memory_->track(QStringLiteral("s_rate"), s_rate_);
    memory_->track(QStringLiteral("s_type"), s_type_);
    memory_->track(QStringLiteral("s_qty"), s_qty_);
    memory_->track(QStringLiteral("stress"), stress_memory_);
    memory_->restore();
    restore_stress();
    if ((!ctx_ || !ctx_->position().valid) && source_->currentIndex() == 0)
        source_->setCurrentIndex(1); // nothing on the STRATEGY page yet: start on a single option
    if ((!ctx_ || !ctx_->book_position().valid) && source_->currentIndex() == 2)
        source_->setCurrentIndex(ctx_ && ctx_->position().valid ? 0 : 1); // the book hand-off is per session
    update_source_rows();

    for (QDoubleSpinBox* s : {spot_range_, vol_range_, rate_bp_, s_spot_, s_strike_, s_iv_, s_rate_, s_qty_})
        connect(s, &QDoubleSpinBox::valueChanged, this, &DerivScenarioPage::schedule);
    for (QSpinBox* s : {spot_steps_, vol_steps_, s_days_})
        connect(s, &QSpinBox::valueChanged, this, &DerivScenarioPage::schedule);
    connect(s_type_, &QComboBox::currentIndexChanged, this, &DerivScenarioPage::schedule);
    connect(source_, &QComboBox::currentIndexChanged, this, [this] {
        update_source_rows();
        schedule();
    });
    connect(days_, &QSlider::valueChanged, this, [this](int d) {
        days_label_->setText(tr("%1 days").arg(d));
        schedule();
    });
    connect(stress_, &ui::ScenarioGrid::scenarios_edited, this, [this] {
        save_stress();
        schedule();
    });
    if (ctx_)
        connect(ctx_, &DerivContext::position_changed, this, [this] {
            if (source_->currentIndex() == 0)
                schedule();
        });
    set_view(0);
    retranslate();
}

// ── Inputs ───────────────────────────────────────────────────────────────────

void DerivScenarioPage::update_source_rows() {
    const bool single = source_->currentIndex() == 1;
    single_->setVisible(single);
    st::Spec spec;
    st::Market m;
    double iv = 0;
    QString label;
    int max_days = 30;
    if (position(&spec, &m, &iv, &label))
        max_days = std::max(1, static_cast<int>(std::floor(st::first_expiry(spec) * 365.0)));
    QSignalBlocker block(days_);
    days_->setRange(0, max_days);
    days_label_->setText(tr("%1 days").arg(days_->value()));
}

bool DerivScenarioPage::position(st::Spec* spec, st::Market* m, double* atm_iv, QString* label) const {
    spec->multiplier = 100;
    spec->legs.clear();
    if (source_->currentIndex() == 0 && ctx_ && ctx_->position().valid) {
        const DerivContext::Position& p = ctx_->position();
        for (const LegView& v : p.legs)
            spec->legs.push_back(v.leg);
        spec->multiplier = p.multiplier;
        *m = p.market;
        *atm_iv = p.atm_iv;
        *label = tr("%1 on %2 · %3 legs").arg(p.name, p.symbol).arg(p.legs.size());
        return !spec->legs.empty();
    }
    if (source_->currentIndex() == 2 && ctx_ && ctx_->book_position().valid) {
        const DerivContext::Position& p = ctx_->book_position();
        for (const LegView& v : p.legs)
            spec->legs.push_back(v.leg);
        spec->multiplier = p.multiplier;
        *m = p.market;
        *atm_iv = p.atm_iv;
        *label = tr("%1 · %2 legs").arg(p.name).arg(p.legs.size());
        return !spec->legs.empty();
    }
    if (source_->currentIndex() != 1)
        return false;
    st::Leg l;
    l.kind = s_type_->currentIndex() == 0 ? st::LegKind::Call : st::LegKind::Put;
    l.quantity = s_qty_->value() == 0 ? 1 : s_qty_->value();
    l.strike = s_strike_->value();
    l.expiry_years = s_days_->value() / 365.0;
    l.iv = s_iv_->value() / 100.0;
    m->spot = s_spot_->value();
    m->r = s_rate_->value() / 100.0;
    m->q = 0;
    l.entry_price = services::options::models::bsm_price(l.kind == st::LegKind::Call, m->spot, l.strike, l.expiry_years,
                                                        m->r, 0.0, l.iv);
    spec->legs.push_back(l);
    *atm_iv = l.iv;
    *label = tr("%1 × %2 %3, %4 days").arg(num(l.quantity, 0), num(l.strike, 2),
                                           l.kind == st::LegKind::Call ? tr("call") : tr("put"))
                 .arg(s_days_->value());
    return true;
}

void DerivScenarioPage::schedule() {
    debounce_->start();
}

void DerivScenarioPage::refresh() {
    update_source_rows();
    pane_->run();
}

void DerivScenarioPage::use_strategy_position() {
    if (ctx_ && ctx_->position().valid) {
        currency_ = ctx_->position().currency;
        source_->setCurrentIndex(0);
    }
    refresh();
}

void DerivScenarioPage::use_book_position() {
    if (ctx_ && ctx_->book_position().valid) {
        currency_ = ctx_->book_position().currency;
        QSignalBlocker block(source_);
        source_->setCurrentIndex(2);
        update_source_rows();
    }
    refresh();
}

void DerivScenarioPage::load_contract(const ContractRef& c) {
    // Not blocked: the inputs are remembered like typed ones; the edits' debounced re-run is
    // superseded by the run below.
    source_->setCurrentIndex(1);
    s_spot_->setValue(c.spot);
    s_strike_->setValue(c.strike);
    s_days_->setValue(std::max(1, static_cast<int>(std::lround(c.years * 365.0))));
    if (c.iv > 0)
        s_iv_->setValue(c.iv * 100.0);
    s_rate_->setValue(c.rate * 100.0);
    s_type_->setCurrentIndex(c.is_call ? 0 : 1);
    s_qty_->setValue(1);
    currency_ = QStringLiteral("USD");
    update_source_rows();
    status_->setText(tr("%1 %2 %3 %4 from the chain (carry not included in the single-option inputs).")
                         .arg(c.symbol, c.expiry_label, num(c.strike, 2), c.is_call ? tr("call") : tr("put")));
    refresh();
}

void DerivScenarioPage::save_stress() {
    QJsonArray a;
    for (const ui::ScenarioRow& r : stress_->scenarios()) {
        if (r.base)
            continue;
        QJsonArray s;
        for (double d : r.shocks)
            s.append(d);
        a.append(QJsonObject{{QStringLiteral("n"), r.name}, {QStringLiteral("s"), s}});
    }
    stress_memory_->setText(QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact)));
}

void DerivScenarioPage::restore_stress() {
    QVector<ui::ScenarioRow> rows;
    rows.append({tr("Base (no shock)"), {0, 0, 0, 0}, {}, true});
    const QJsonArray saved = QJsonDocument::fromJson(stress_memory_->text().toUtf8()).array();
    if (!saved.isEmpty()) {
        for (const QJsonValue& v : saved) {
            const QJsonObject o = v.toObject();
            ui::ScenarioRow r;
            r.name = o.value(QStringLiteral("n")).toString();
            for (const QJsonValue& s : o.value(QStringLiteral("s")).toArray())
                r.shocks.append(s.toDouble());
            if (!r.name.isEmpty())
                rows.append(r);
        }
    } else {
        for (const sc::Preset& p : sc::stress_presets())
            rows.append({tr(p.name.c_str()), {p.shock.spot_pct, p.shock.vol_pts, p.shock.rate_bp, p.shock.days}, {}, false});
    }
    stress_->set_rows(rows);
}

// ── Views ────────────────────────────────────────────────────────────────────

void DerivScenarioPage::set_view(int index) {
    views_->setCurrentIndex(index);
    for (int i = 0; i < tab_btns_.size(); ++i) {
        tab_btns_[i]->setProperty("active", i == index);
        tab_btns_[i]->style()->unpolish(tab_btns_[i]);
        tab_btns_[i]->style()->polish(tab_btns_[i]);
    }
}

void DerivScenarioPage::render(const QJsonObject& p) {
    last_ = p;
    if (ctx_ && source_->currentIndex() == 0 && ctx_->position().valid)
        currency_ = ctx_->position().currency;
    else if (ctx_ && source_->currentIndex() == 2 && ctx_->book_position().valid)
        currency_ = ctx_->book_position().currency;
    const QString c = currency_;
    const QVector<double> sp = dsp_vec(p.value(QStringLiteral("spot_pcts")));
    const QVector<double> vp = dsp_vec(p.value(QStringLiteral("vol_pts")));
    QStringList rows, cols;
    for (double s : sp)
        rows << fmt::pct(s, 1);
    for (double v : vp)
        cols << tr("%1 pts").arg(fmt::signed_number(v, 1));
    grid_->clear();
    grid_->set_title(tr("P&L against today's value"),
                     tr("%1 days forward · rate %2").arg(fmt::number(p.value(QStringLiteral("days")).toDouble(), 0),
                                                          fmt::bp(p.value(QStringLiteral("rate_bp")).toDouble())));
    grid_->set_axes(tr("Spot shock"), rows, tr("Vol shock"), cols);
    grid_->set_matrix(dsp_mat(p.value(QStringLiteral("pnl"))), [c](double v) { return signed_money(v, c); });
    grid_->add_metric(tr("Delta (shares)"), dsp_mat(p.value(QStringLiteral("delta"))), [](double v) { return num(v, 1); });
    grid_->add_metric(tr("Vega per vol point"), dsp_mat(p.value(QStringLiteral("vega"))),
                      [c](double v) { return money(v, c); });
    grid_->set_base_cell(sp.size() / 2, vp.size() / 2);
    grid_->set_note(tr("Each leg revalued with Black-Scholes-Merton at its IV plus the vol shock; spot and rate "
                       "shocked as shown. The outlined cell is today's state."));

    const QJsonObject cone = p.value(QStringLiteral("cone")).toObject();
    const QVector<double> x = dsp_vec(cone.value(QStringLiteral("days")));
    ui::FanBand outer{2.3, 97.7, dsp_vec(cone.value(QStringLiteral("p2"))), dsp_vec(cone.value(QStringLiteral("p98")))};
    ui::FanBand inner{15.9, 84.1, dsp_vec(cone.value(QStringLiteral("p16"))), dsp_vec(cone.value(QStringLiteral("p84")))};
    cone_->clear();
    cone_->set_fan(x, {outer, inner}, dsp_vec(cone.value(QStringLiteral("p50"))));
    cone_->clear_reference_lines();
    for (double b : dsp_vec(p.value(QStringLiteral("breakevens"))))
        cone_->add_reference_line(b, tr("Breakeven %1").arg(num(b, 2)));
    cone_note_->setText(tr("Lognormal cone of the underlying from %1 at %2 implied volatility: the inner band holds "
                           "about 68 % of outcomes (±1σ), the outer about 95 % (±2σ). Lines: the position's "
                           "breakevens at its first expiry.")
                            .arg(num(p.value(QStringLiteral("spot")).toDouble(), 2),
                                 vol_pct(p.value(QStringLiteral("atm_iv")).toDouble())));

    const QJsonArray stress = p.value(QStringLiteral("stress")).toArray();
    for (int i = 0; i < stress.size(); ++i)
        stress_->set_metric_values(i, dsp_vec(stress[i]));
    note_->setText(tr("Current model value of the position: %1. P&L is measured against it, so the unshocked cell is "
                      "zero. European Black-Scholes-Merton revaluation; American early exercise is not modelled here.")
                       .arg(money(p.value(QStringLiteral("value")).toDouble(), c)));
}

// ── Export ───────────────────────────────────────────────────────────────────

ui::ChartExport::Table DerivScenarioPage::export_table() const {
    ui::ChartExport::Table t;
    switch (views_->currentIndex()) {
        case 2:
            t.headers = stress_->headers();
            t.rows = stress_->rows();
            break;
        case 1: {
            t.headers = {tr("Days"), tr("2.3%"), tr("15.9%"), tr("Median"), tr("84.1%"), tr("97.7%")};
            const QJsonObject c = last_.value(QStringLiteral("cone")).toObject();
            const QVector<double> d = dsp_vec(c.value(QStringLiteral("days")));
            const QVector<double> a = dsp_vec(c.value(QStringLiteral("p2"))), b = dsp_vec(c.value(QStringLiteral("p16"))),
                                  m = dsp_vec(c.value(QStringLiteral("p50"))), e = dsp_vec(c.value(QStringLiteral("p84"))),
                                  f = dsp_vec(c.value(QStringLiteral("p98")));
            for (int i = 0; i < d.size(); ++i)
                t.rows.append({num(d[i], 0), num(a.value(i), 2), num(b.value(i), 2), num(m.value(i), 2),
                               num(e.value(i), 2), num(f.value(i), 2)});
            break;
        }
        default:
            t.headers = grid_->headers();
            t.rows = grid_->rows();
            break;
    }
    return t;
}

QPixmap DerivScenarioPage::export_image() const {
    switch (views_->currentIndex()) {
        case 1:
            return ui::ChartExport::grab(cone_);
        case 2:
            return ui::ChartExport::grab(stress_);
        default:
            return grid_->grab_image();
    }
}

report::ReportPayload DerivScenarioPage::report_payload() const {
    report::ReportPayload p;
    p.source = QStringLiteral("Derivatives");
    p.title = tr("Scenario analysis: %1").arg(summary_->text());
    p.as_of = QDateTime::currentDateTime();
    report::PayloadKpi v;
    v.label = tr("Current model value");
    v.value = money(last_.value(QStringLiteral("value")).toDouble(), currency_);
    p.kpis.append(v);
    double worst = 0, best = 0;
    for (const auto& row : dsp_mat(last_.value(QStringLiteral("pnl"))))
        for (double d : row) {
            worst = std::min(worst, d);
            best = std::max(best, d);
        }
    report::PayloadKpi w;
    w.label = tr("Worst grid P&L");
    w.value = signed_money(worst, currency_);
    w.tone = QStringLiteral("negative");
    p.kpis.append(w);
    report::PayloadKpi b;
    b.label = tr("Best grid P&L");
    b.value = signed_money(best, currency_);
    b.tone = QStringLiteral("positive");
    p.kpis.append(b);
    report::PayloadTable grid;
    grid.title = tr("P&L over spot × vol shocks");
    grid.columns = grid_->headers();
    grid.rows = grid_->rows();
    p.tables.append(grid);
    report::PayloadTable stress;
    stress.title = tr("Stress scenarios");
    stress.columns = stress_->headers();
    stress.rows = stress_->rows();
    p.tables.append(stress);
    report::PayloadChart cone;
    cone.title = tr("Probability cone");
    cone.image = png_bytes(ui::ChartExport::grab(cone_));
    if (!cone.image.isEmpty())
        p.charts.append(cone);
    p.inputs = {{tr("Spot shock"), QStringLiteral("± %1").arg(pct_points(spot_range_->value(), 1))},
                {tr("Vol shock"), tr("± %1 pts").arg(num(vol_range_->value(), 1))},
                {tr("Days forward"), QString::number(days_->value())},
                {tr("Rate shift"), fmt::bp(rate_bp_->value())}};
    p.notes = note_->text();
    return p;
}

// ── Language ─────────────────────────────────────────────────────────────────

void DerivScenarioPage::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void DerivScenarioPage::retranslate() {
    book_.apply();
    days_label_->setText(tr("%1 days").arg(days_->value()));
    if (!last_.isEmpty())
        render(last_);
}

} // namespace fincept::screens::deriv
