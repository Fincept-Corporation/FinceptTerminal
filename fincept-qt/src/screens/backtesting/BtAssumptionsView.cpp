// src/screens/backtesting/BtAssumptionsView.cpp
//
// ASSUMPTIONS — everything a reader needs to trust a number of the active run: warnings, data provenance
// per symbol, execution model, cost model, metric conventions, what is not modelled, ignored settings,
// derived-data disclosures, metrics left blank and why, engine identity, the provider log and RAW JSON.
#include "screens/backtesting/BtWorkbenchTabs.h"

#include "ui/formatting/FormatWidgets.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/PanelHeader.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens::bt {

using namespace fincept::services::backtest;

namespace {

} // namespace

/// The panels of the view.
struct BtAssumptionsView::Impl {
    QScrollArea* scroll = nullptr;
    QGridLayout* grid = nullptr;
    QLabel* headline = nullptr;
    QWidget* warnings_box = nullptr;
    QVBoxLayout* warnings_layout = nullptr;
    BtKvGrid* data_kv = nullptr;
    ui::LiveTableModel<BtProvSymbol>* data_model = nullptr;
    QTableView* data_table = nullptr;
    QLabel* data_note = nullptr;
    BtKvGrid* exec_kv = nullptr;
    BtKvGrid* cost_kv = nullptr;
    BtKvGrid* conv_kv = nullptr;
    QLabel* not_modelled = nullptr;
    QLabel* ignored = nullptr;
    QLabel* disclosures = nullptr;
    QFrame* disclosures_panel = nullptr;
    BtKvGrid* blank_kv = nullptr;
    QFrame* blank_panel = nullptr;
    BtKvGrid* engine_kv = nullptr;
    QPlainTextEdit* log = nullptr;
    QVector<QFrame*> panels;
    int columns = 0;
};

namespace {

QString bt_asm_text(const QString& s) {
    return s.trimmed().isEmpty() ? dash() : s.trimmed();
}

QString bt_asm_bullets(const QStringList& items, const QString& none) {
    if (items.isEmpty())
        return none;
    QStringList out;
    for (const QString& i : items)
        out << QStringLiteral("•  ") + i;
    return out.join(QLatin1Char('\n'));
}

QString bt_asm_bp(double fraction) {
    return std::isfinite(fraction) ? QCoreApplication::translate("fincept::screens::bt::BtAssumptionsView", "%1 bp")
                                         .arg(num(fraction * 10000.0, 1))
                                   : dash();
}

} // namespace

BtAssumptionsView::~BtAssumptionsView() = default;

BtAssumptionsView::BtAssumptionsView(BtSelection* selection, QWidget* parent)
    : BtRunView(selection, parent), d(std::make_unique<Impl>()) {
    Impl& p = *d;
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    p.scroll = new QScrollArea(this);
    p.scroll->setObjectName(QStringLiteral("btScroll"));
    p.scroll->setWidgetResizable(true);
    p.scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget(p.scroll);
    auto* bv = new QVBoxLayout(body);
    bv->setContentsMargins(6, 6, 6, 6);
    bv->setSpacing(6);
    p.headline = make_note(QString(), body, Tone::None);
    bv->addWidget(p.headline);
    auto* gridw = new QWidget(body);
    p.grid = new QGridLayout(gridw);
    p.grid->setContentsMargins(0, 0, 0, 0);
    p.grid->setSpacing(6);
    bv->addWidget(gridw);
    bv->addStretch(1);
    p.scroll->setWidget(body);
    root->addWidget(p.scroll);

    auto panel = [&p, gridw](const QString& title) {
        BtPanel pn = make_panel(title, gridw);
        p.panels.append(pn.frame);
        return pn;
    };

    BtPanel warn = panel(tr("WARNINGS"));
    p.warnings_box = new QWidget(warn.body_widget);
    p.warnings_layout = new QVBoxLayout(p.warnings_box);
    p.warnings_layout->setContentsMargins(0, 0, 0, 0);
    p.warnings_layout->setSpacing(4);
    warn.body->addWidget(p.warnings_box);

    BtPanel data = panel(tr("DATA"));
    p.data_kv = new BtKvGrid(data.body_widget, 2);
    data.body->addWidget(p.data_kv);
    p.data_model = new ui::LiveTableModel<BtProvSymbol>(this);
    p.data_model->set_headers({tr("SYMBOL"), tr("SOURCE"), tr("CCY"), tr("BARS"), tr("FIRST BAR"), tr("LAST BAR"),
                               tr("REQUESTED FROM"), tr("MISSING"), tr("CACHE AS OF")});
    p.data_model->set_key([](const BtProvSymbol& s) { return s.symbol; });
    p.data_model->set_cell([](const BtProvSymbol& s, int col, int role) -> QVariant {
        if (role == Qt::DisplayRole) {
            switch (col) {
                case 0:
                    return s.symbol;
                case 1:
                    return bt_asm_text(s.source);
                case 2:
                    return bt_asm_text(s.currency);
                case 3:
                    return count(s.bars);
                case 4:
                    return date_text(s.first_bar);
                case 5:
                    return date_text(s.last_bar);
                case 6:
                    return date_text(s.requested_start);
                case 7:
                    return count(s.missing_bars);
                case 8:
                    return bt_asm_text(s.cache_as_of);
                default:
                    break;
            }
        } else if (role == ui::LiveTableModel<BtProvSymbol>::SortRole) {
            if (col == 3)
                return s.bars;
            if (col == 7)
                return s.missing_bars;
            return s.symbol;
        } else if (role == Qt::ForegroundRole && col == 7 && s.missing_bars > 0) {
            return tone_color(Tone::Warn);
        } else if (role == Qt::TextAlignmentRole && (col == 3 || col == 7)) {
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }
        return {};
    });
    p.data_table = new QTableView(data.body_widget);
    setup_table(p.data_table, QStringLiteral("backtest_data_provenance"));
    p.data_table->setModel(make_sort_proxy(p.data_model, this));
    set_column_chars(p.data_table, {10, 10, 5, 7, 11, 11, 13, 8, 18});
    p.data_table->setMinimumHeight(p.data_table->verticalHeader()->defaultSectionSize() * 4);
    data.body->addWidget(p.data_table);
    p.data_note = make_note(QString(), data.body_widget, Tone::Warn);
    data.body->addWidget(p.data_note);
    fincept::ui::formatting::refresh_model_on_format_change(p.data_model);

    BtPanel exec = panel(tr("EXECUTION MODEL"));
    p.exec_kv = new BtKvGrid(exec.body_widget);
    exec.body->addWidget(p.exec_kv);

    BtPanel cost = panel(tr("COST MODEL"));
    p.cost_kv = new BtKvGrid(cost.body_widget);
    cost.body->addWidget(p.cost_kv);

    BtPanel conv = panel(tr("METRIC CONVENTIONS"));
    p.conv_kv = new BtKvGrid(conv.body_widget);
    conv.body->addWidget(p.conv_kv);

    BtPanel nm = panel(tr("NOT MODELLED"));
    p.not_modelled = make_note(QString(), nm.body_widget, Tone::None);
    nm.body->addWidget(p.not_modelled);
    auto* ig_title = new QLabel(tr("IGNORED SETTINGS"), nm.body_widget);
    ig_title->setObjectName(QStringLiteral("btSectionTitle"));
    nm.body->addWidget(ig_title);
    p.ignored = make_note(QString(), nm.body_widget, Tone::None);
    nm.body->addWidget(p.ignored);

    BtPanel disc = panel(tr("DERIVED DATA"));
    p.disclosures_panel = disc.frame;
    p.disclosures = make_note(QString(), disc.body_widget, Tone::Warn);
    disc.body->addWidget(p.disclosures);

    BtPanel blank = panel(tr("BLANK METRICS — WHY"));
    p.blank_panel = blank.frame;
    p.blank_kv = new BtKvGrid(blank.body_widget);
    blank.body->addWidget(p.blank_kv);

    BtPanel eng = panel(tr("ENGINE"));
    p.engine_kv = new BtKvGrid(eng.body_widget);
    eng.body->addWidget(p.engine_kv);

    BtPanel log = panel(tr("LOG"));
    auto* raw = make_tool_button(tr("RAW JSON"), tr("The complete result as the engine returned it"), log.header);
    log.header->add_control(raw);
    connect(raw, &QPushButton::clicked, this, &BtAssumptionsView::raw_json_requested);
    p.log = new QPlainTextEdit(log.body_widget);
    p.log->setReadOnly(true);
    p.log->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    p.log->setMinimumHeight(QFontMetrics(font()).height() * 6);
    log.body->addWidget(p.log);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        d->columns = 0; // re-flow with the new font
        QResizeEvent ev(size(), size());
        resizeEvent(&ev);
    });
    fincept::ui::formatting::on_format_changed(this, [this]() { show_run(run_); });
}

void BtAssumptionsView::show_run(const BtRunPtr& run) {
    run_ = run;
    Impl& p = *d;
    if (p.columns == 0) {
        QResizeEvent ev(size(), size());
        resizeEvent(&ev);
    }
    // Warnings (rebuilt: at most a few dozen rows).
    while (QLayoutItem* item = p.warnings_layout->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        if (QLayout* l = item->layout()) {
            while (QLayoutItem* sub = l->takeAt(0)) {
                if (QWidget* sw = sub->widget())
                    sw->deleteLater();
                delete sub;
            }
        }
        delete item;
    }
    if (!run) {
        p.headline->setText(tr("No backtest yet — run one to see its assumptions."));
        p.data_model->upsert({});
        p.log->clear();
        return;
    }
    const BtRun& r = *run;
    const BtProvenance& pv = r.provenance;
    const BtAssumptions& a = r.assumptions;
    QString engine = r.engine_implementation.isEmpty() ? r.engine : r.engine_implementation;
    p.headline->setText(tr("Produced by %1%2 · fill: %3 · costs: %4 · risk-free: %5 · data: %6")
                            .arg(engine, r.engine_version.isEmpty() ? QString() : QStringLiteral(" ") + r.engine_version,
                                 a.fill_at.isEmpty() ? dash() : fill_timing_text(a.fill_at),
                                 r.costs.model.preset.isEmpty() ? bt_asm_text(a.costs).left(80) : r.costs.model.preset,
                                 a.risk_free_source.isEmpty() ? dash() : a.risk_free_source,
                                 pv.present ? QStringLiteral("%1 %2 → %3").arg(pv.data_source, pv.first_bar, pv.last_bar)
                                            : tr("not reported")));
    set_tone(p.headline, r.synthetic ? Tone::Danger : Tone::None);

    if (r.warnings.isEmpty()) {
        p.warnings_layout->addWidget(make_note(tr("No warnings: the engine reported nothing that limits this result."),
                                               p.warnings_box, Tone::Ok));
    }
    for (const BtWarning& w : r.warnings) {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        const int rank = bt_warning_rank(w.severity);
        const Tone tone = rank == 0 ? Tone::Danger : (rank == 1 ? Tone::Warn : Tone::Dim);
        auto* chip = make_chip(rank == 0 ? tr("ERROR") : (rank == 1 ? tr("WARN") : tr("INFO")), tone, p.warnings_box);
        chip->setToolTip(w.code);
        auto* msg = make_note(w.symbol.isEmpty() ? w.message : QStringLiteral("%1 — %2").arg(w.symbol, w.message),
                              p.warnings_box, Tone::None);
        row->addWidget(chip, 0, Qt::AlignTop);
        row->addWidget(msg, 1);
        p.warnings_layout->addLayout(row);
    }

    p.data_kv->set_rows({{tr("Source"), bt_asm_text(pv.data_source.isEmpty() ? a.data_source : pv.data_source)},
                         {tr("Adjustment"), bt_asm_text(pv.adjustment)},
                         {tr("Interval"), bt_asm_text(pv.interval)},
                         {tr("Bars"), count(pv.bars >= 0 ? pv.bars : r.t.size())},
                         {tr("First bar"), date_text(pv.first_bar.isEmpty() ? r.dates.value(0) : pv.first_bar)},
                         {tr("Last bar"), date_text(pv.last_bar.isEmpty() ? (r.dates.isEmpty() ? QString() : r.dates.last()) : pv.last_bar)},
                         {tr("Requested"), QStringLiteral("%1 → %2").arg(date_text(pv.requested_start), date_text(pv.requested_end))},
                         {tr("Warm-up"), pv.warmup_bars >= 0 ? bars(pv.warmup_bars) : bt_asm_text(a.warm_up)},
                         {tr("Currency"), bt_asm_text(r.currency)},
                         {tr("Synthetic"), r.synthetic ? tr("YES — no market meaning") : tr("no"), r.synthetic ? Tone::Danger : Tone::None}});
    p.data_model->upsert(pv.symbols);
    p.data_table->setVisible(!pv.symbols.isEmpty());
    p.data_note->setText(pv.present ? QString() : tr("This engine does not report data provenance yet; the dates come from "
                                                     "its equity curve."));
    p.data_note->setVisible(!pv.present);

    p.exec_kv->set_rows({{tr("Signal at"), bt_asm_text(a.fill_signal_at)},
                         {tr("Fill at"), a.fill_at.isEmpty() ? tr("not reported") : fill_timing_text(a.fill_at),
                          a.fill_at == QLatin1String("same_close") ? Tone::Warn : Tone::None, a.fill_description},
                         {tr("Execution"), bt_asm_text(a.execution)},
                         {tr("Sizing"), bt_asm_text(a.position_sizing)},
                         {tr("Survivorship"), bt_asm_text(a.survivorship)},
                         {tr("Engine"), bt_asm_text(a.engine)}});

    const BtCostModel& m = r.costs.model;
    QVector<BtKv> cost_rows;
    cost_rows.append({tr("Preset"), bt_asm_text(m.preset), Tone::None, m.source});
    cost_rows.append({tr("Source"), bt_asm_text(m.source)});
    if (m.present) {
        cost_rows.append({tr("Commission"), m.commission_type == QLatin1String("per_share")
                                                ? tr("%1 per share").arg(num(m.commission_value, 4))
                                                : bt_asm_bp(m.commission_value) + QStringLiteral(" (") + bt_asm_text(m.commission_type) + QLatin1Char(')')});
        cost_rows.append({tr("Minimum"), std::isfinite(m.min_commission) ? num(m.min_commission, 2) : dash()});
        cost_rows.append({tr("Slippage"), bt_asm_bp(m.slippage_value) + QStringLiteral(" (") + bt_asm_text(m.slippage_type) + QLatin1Char(')')});
        cost_rows.append({tr("Spread"), std::isfinite(m.spread_bps) ? tr("%1 bp quoted").arg(num(m.spread_bps, 1)) : dash()});
        cost_rows.append({tr("Borrow"), pct(m.borrow_rate, 2) + tr(" a year")});
        cost_rows.append({tr("Financing"), std::isfinite(m.financing_rate) ? pct(m.financing_rate, 2) : tr("not charged")});
        cost_rows.append({tr("Itemised"), QStringLiteral("%1 · %2 · %3")
                                              .arg(m.itemised_commission ? tr("commission") : tr("commission inside prices"),
                                                   m.itemised_slippage ? tr("slippage") : tr("slippage inside prices"),
                                                   m.itemised_borrow ? tr("borrow") : tr("borrow not itemised"))});
    } else {
        cost_rows.append({tr("Applied"), bt_asm_text(a.costs)});
    }
    p.cost_kv->set_rows(cost_rows);

    p.conv_kv->set_rows({{tr("Annualisation"), bt_asm_text(a.annualization)},
                         {tr("Bars / year"), num(r.periods_per_year, 0)},
                         {tr("Risk-free"), a.risk_free_source.isEmpty() ? dash()
                                                                         : QStringLiteral("%1 · %2").arg(a.risk_free_source, pct(a.risk_free_rate, 2)),
                          Tone::None, a.risk_free_description},
                         {tr("Returns"), tr("simple bar returns; CAGR on calendar days")},
                         {tr("VaR / CVaR"), tr("1-bar historical; not a forecast")},
                         {tr("Drawdown"), tr("from the running peak of equity")}});

    p.not_modelled->setText(bt_asm_bullets(a.not_modelled, tr("Not reported by this engine")));
    QStringList ignored = a.ignored_settings;
    for (const BtWarning& w : r.warnings)
        if (w.code == QLatin1String("IGNORED_SETTING") || w.code == QLatin1String("IGNORED_SYMBOLS"))
            if (!ignored.contains(w.message))
                ignored << w.message;
    p.ignored->setText(bt_asm_bullets(ignored, tr("None — every setting was applied")));
    p.disclosures->setText(bt_asm_bullets(r.data_disclosures + a.notes, QString()));
    p.disclosures_panel->setVisible(!r.data_disclosures.isEmpty() || !a.notes.isEmpty());

    QVector<BtKv> blank_rows;
    for (auto it = r.undefined.constBegin(); it != r.undefined.constEnd(); ++it)
        blank_rows.append({it.key(), it.value(), Tone::Dim});
    p.blank_kv->set_rows(blank_rows);
    p.blank_panel->setVisible(!blank_rows.isEmpty());

    p.engine_kv->set_rows({{tr("Engine"), bt_asm_text(r.engine)},
                           {tr("Implementation"), bt_asm_text(r.engine_implementation)},
                           {tr("Engine version"), bt_asm_text(r.engine_version)},
                           {tr("Library"), pv.library.isEmpty() ? tr("not used") : QStringLiteral("%1 %2").arg(pv.library, pv.library_version)},
                           {tr("Python"), bt_asm_text(pv.python_version)},
                           {tr("Result schema"), QString::number(r.schema_version)},
                           {tr("Strategy"), pv.strategy_name.isEmpty() ? dash() : QStringLiteral("%1 (%2)").arg(pv.strategy_name, pv.strategy_type)},
                           {tr("Run id"), bt_asm_text(pv.run_id.isEmpty() ? r.id : pv.run_id)},
                           {tr("Started"), bt_asm_text(pv.started_at)},
                           {tr("Duration"), pv.duration_ms >= 0 ? tr("%1 s").arg(num(pv.duration_ms / 1000.0, 2)) : dash()},
                           {tr("Seed"), std::isfinite(pv.seed) ? num(pv.seed, 0) : tr("none (deterministic)")}});

    p.log->setPlainText(r.logs.isEmpty() ? tr("The engine wrote no log for this run.") : r.logs.join(QLatin1Char('\n')));
}

void BtAssumptionsView::resizeEvent(QResizeEvent* e) {
    BtRunView::resizeEvent(e);
    Impl& p = *d;
    const int avg = std::max(5, QFontMetrics(font()).averageCharWidth());
    const int cols = e->size().width() >= avg * 150 ? 2 : 1;
    if (cols == p.columns)
        return;
    p.columns = cols;
    for (QFrame* f : p.panels)
        p.grid->removeWidget(f);
    for (int i = 0; i < p.panels.size(); ++i)
        p.grid->addWidget(p.panels[i], cols == 2 ? i / 2 : i, cols == 2 ? i % 2 : 0);
}

} // namespace fincept::screens::bt
