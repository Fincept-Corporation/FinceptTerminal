// src/screens/algo_trading/ScannerPanel.cpp
#include "screens/algo_trading/ScannerPanel.h"

#include "algo_engine/AlgoMarket.h"
#include "algo_engine/AlgoScanner.h"
#include "algo_engine/CandleDataFetcher.h"
#include "core/logging/Logger.h"
#include "screens/algo_trading/AlgoTabKit.h"
#include "screens/algo_trading/ScannerView.h"
#include "services/algo_trading/AlgoRuleLogic.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "services/algo_trading/AlgoUniverses.h"
#include "trading/AccountManager.h"
#include "ui/charts/ChartExport.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/algo/AlgoCurrency.h"
#include "ui/widgets/algo/RuleTableEditor.h"
#include "ui/widgets/algo/SymbolChipInput.h"

#include <QComboBox>
#include <QHideEvent>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>

namespace fincept::screens {

using namespace fincept::services::algo;
namespace kit = fincept::screens::algo_kit;

namespace {

/// Most symbols the DEFAULT scan reads from a region list (the user's own runs are not capped).
constexpr int kScanDefaultSymbolCap = 30;
/// Price-based result: re-entering the tab after this refreshes it quietly.
constexpr int kScanMaxAgeSec = 15 * 60;
/// The selected result's chart is fetched once the selection rests this long.
constexpr int kScanDetailDebounceMs = 250;

QVector<QPair<QString, int>> scan_range_options_for(const QString& tf) {
    if (tf == QStringLiteral("live"))
        return {};
    const int cap = fincept::services::algo::algo_default_lookback_days(tf);
    const QVector<QPair<QString, int>> ladder = {{QStringLiteral("Today"), 1},
                                                 {QStringLiteral("5D"), 5},
                                                 {QStringLiteral("1M"), 30},
                                                 {QStringLiteral("6M"), 180},
                                                 {QStringLiteral("1Y"), 365}};
    QVector<QPair<QString, int>> out;
    for (const auto& o : ladder)
        if (o.second < cap)
            out.append(o);
    out.append({QStringLiteral("Max"), cap});
    return out;
}

} // namespace

// ── Constructor ─────────────────────────────────────────────────────────────

ScannerPanel::ScannerPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect_service();
    // Currency / number format changed: re-render the last results (no re-scan).
    fincept::ui::formatting::on_format_changed(this, [this]() {
        if (!last_scan_.isEmpty())
            view_->set_scan(last_scan_, [this](const QString& s) { return result_currency(s); });
    });
    LOG_DEBUG("AlgoTrading", "ScannerPanel constructed");
}

void ScannerPanel::connect_service() {
    auto& scanner = fincept::algo::AlgoScanner::instance();
    connect(&scanner, &fincept::algo::AlgoScanner::scan_complete, this, &ScannerPanel::on_scan_result);
    connect(&scanner, &fincept::algo::AlgoScanner::scan_error, this, &ScannerPanel::on_scan_error);
}

// ── Build UI ────────────────────────────────────────────────────────────────

void ScannerPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    view_ = new ScannerView(this);
    root->addWidget(view_);

    // Preset rule sets ("Custom" = whatever is in RULES).
    QComboBox* preset = view_->preset_combo();
    preset->addItem(tr("Custom"));
    for (const auto& p : scanner_presets())
        preset->addItem(p.name);
    connect(preset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScannerPanel::apply_preset);

    // "live" is a per-tick deployment mode, not a bar size: a scan fetches history,
    // and "live" fell through to DAILY bars while the result row said LIVE.
    QStringList scan_tfs = algo_timeframes();
    scan_tfs.removeAll(QStringLiteral("live"));
    view_->timeframe_combo()->addItems(scan_tfs);
    view_->timeframe_combo()->setCurrentIndex(std::max(0, static_cast<int>(scan_tfs.indexOf("1d"))));
    connect(view_->timeframe_combo(), QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { rebuild_range_options(); });
    rebuild_range_options();

    // Visible labels translatable; the userData keys ("Auto"/"Broker"/"YFinance") drive logic.
    view_->source_combo()->addItem(QString(), "Auto");
    view_->source_combo()->addItem(QString(), "Broker");
    view_->source_combo()->addItem(QString(), "YFinance");
    connect(view_->source_combo(), QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) { view_->set_account_visible(idx != 2); }); // none for "YFinance only"
    populate_accounts();
    fill_universes();
    retranslate_inputs();

    connect(view_->load_universe_button(), &QPushButton::clicked, this, &ScannerPanel::load_universe);
    connect(view_->save_universe_button(), &QPushButton::clicked, this, &ScannerPanel::save_universe);
    connect(view_->symbols(), &fincept::ui::algo::SymbolChipInput::price_resolved, this,
            [this](const QString& s, double px) { prefill_close_from_price(s, px); });
    connect(view_, &ScannerView::action_requested, this, &ScannerPanel::on_action);
    connect(view_, &ScannerView::activated, this, &ScannerPanel::symbol_activated);
    connect(view_, &ScannerView::selection_changed, this, &ScannerPanel::schedule_detail_fetch);
    connect(view_, &ScannerView::no_match_action, this, [this]() {
        view_->preset_combo()->setFocus();
        view_->preset_combo()->showPopup();
    });

    // The results area is a ui::AutoLoadPane: on first show the default scan runs
    // (skeleton while it does, the remembered scan with its "as of" on later visits,
    // a quiet re-scan when that is older than 15 minutes), a failure is a page with
    // RETRY, and "no symbol matched" is a designed state — never a bare header.
    results_pane_ = new fincept::ui::AutoLoadPane(fincept::ui::AutoLoadPane::Policy::AutoRun, view_->results_host());
    results_pane_->set_content(view_->build_results_content(results_pane_->content_parent()));
    view_->results_host()->layout()->addWidget(results_pane_);
    results_pane_->skeleton()->set_shape(fincept::ui::LoadingSkeleton::Shape::Table);
    results_pane_->skeleton()->set_row_count(8);
    results_pane_->set_loading_text(tr("Scanning the market…"));
    results_pane_->set_cache_key(QStringLiteral("algo_trading/scanner"));
    results_pane_->set_max_age(kScanMaxAgeSec);
    results_pane_->set_timeout(180);
    results_pane_->set_auto_delay(250);
    results_pane_->set_status_label(view_->asof_label());
    results_pane_->set_start([this](quint64 id) { start_scan(id); });
    results_pane_->set_cancel([this](quint64 id) {
        // AlgoScanner cannot stop a scan: its answer is dropped instead.
        if (inflight_id_ == id)
            inflight_id_ = 0;
        if (queued_id_ == id)
            queued_id_ = 0;
    });
    results_pane_->set_render(
        [this](const QJsonValue& payload, const QDateTime&, bool) { apply_scan_payload(payload.toObject()); });
    connect(results_pane_, &fincept::ui::AutoLoadPane::running_changed, this,
            [this](bool running) { view_->set_running(running); });
    connect(results_pane_, &fincept::ui::AutoLoadPane::result_shown, this, [this](const QDateTime& as_of, bool) {
        view_->status_bar()->set_text(QStringLiteral("asof"), fincept::ui::AutoLoadPane::as_of_text(as_of));
    });

    detail_timer_ = new QTimer(this);
    detail_timer_->setSingleShot(true);
    detail_timer_->setInterval(kScanDetailDebounceMs);
    connect(detail_timer_, &QTimer::timeout, this, [this]() { fetch_detail(detail_pending_); });
}

void ScannerPanel::retranslate_inputs() {
    // Index 0 of the preset combo is the fixed "Custom" entry; the rest are preset names.
    if (view_->preset_combo()->count() > 0)
        view_->preset_combo()->setItemText(0, tr("Custom"));
    QComboBox* src = view_->source_combo();
    if (src->count() >= 3) {
        src->setItemText(0, tr("Auto (broker → Yahoo)"));
        src->setItemText(1, tr("Broker only"));
        src->setItemText(2, tr("Yahoo Finance only"));
    }
    // account combo item 0 is the fixed "None" fallback; the rest are account data.
    if (view_->account_combo()->count() > 0)
        view_->account_combo()->setItemText(0, tr("None (Yahoo fallback)"));
}

// ── Inputs ──────────────────────────────────────────────────────────────────

void ScannerPanel::apply_preset(int index) {
    const auto presets = scanner_presets();
    if (index <= 0 || index > presets.size())
        return; // index 0 = "Custom": the rules stay as they are
    const auto& preset = presets[index - 1];
    view_->rules()->set_conditions(preset.conditions, QStringLiteral("AND"));
    LOG_INFO("AlgoTrading", QString("Applied scanner preset: %1").arg(preset.name));
}

void ScannerPanel::rebuild_range_options() {
    const QString tf = view_->timeframe_combo()->currentText();
    const auto opts = scan_range_options_for(tf);
    const bool live = opts.isEmpty();
    view_->set_range_visible(!live);
    if (live)
        return;
    QComboBox* range = view_->range_combo();
    const QString prev = range->currentText();
    QSignalBlocker block(range);
    range->clear();
    for (const auto& o : opts)
        range->addItem(o.first, o.second);
    const int idx = range->findText(prev);
    range->setCurrentIndex(idx >= 0 ? idx : 0);
}

void ScannerPanel::fill_universes() {
    QComboBox* combo = view_->universe_combo();
    const QString keep = combo->currentData().toString();
    QSignalBlocker blocker(combo);
    combo->clear();
    for (const auto& u : algo_universes()) {
        combo->addItem(u.name, u.id);
        combo->setItemData(combo->count() - 1, tr("%1 symbols. %2").arg(u.symbols.size()).arg(u.source), Qt::ToolTipRole);
    }
    const int idx = combo->findData(keep);
    if (idx >= 0)
        combo->setCurrentIndex(idx);
}

void ScannerPanel::load_universe() {
    const QString id = view_->universe_combo()->currentData().toString();
    for (const auto& u : algo_universes()) {
        if (u.id != id)
            continue;
        exchange_hint_ = u.exchange_hint; // before set_symbols: its symbols_changed() remembers both
        view_->symbols()->set_symbols(u.symbols);
        view_->status_bar()->show_notice(tr("Loaded %1 (%2 symbols).").arg(u.name).arg(u.symbols.size()));
        return;
    }
}

void ScannerPanel::save_universe() {
    const QStringList syms = view_->symbols()->symbols();
    if (syms.isEmpty()) {
        view_->status_bar()->show_notice(tr("Add symbols first, then save them as a list."), QString(), {}, 6000,
                                         fincept::ui::ScreenStatusBar::Tone::Warn);
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Save symbol list"), tr("List name:"), QLineEdit::Normal,
                                               QString(), &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    AlgoUniverse u;
    u.name = name;
    u.symbols = syms;
    u.exchange_hint = exchange_hint_;
    QString err;
    if (!save_user_universe(u, &err)) {
        QMessageBox::warning(this, tr("Save symbol list"), tr("Could not save the list: %1").arg(err));
        return;
    }
    fill_universes();
    view_->universe_combo()->setCurrentIndex(std::max(0, view_->universe_combo()->findData(QStringLiteral("user:") + name)));
    view_->status_bar()->show_notice(
        tr("Saved list \"%1\" (%2 symbols) to %3.").arg(name).arg(syms.size()).arg(algo_universes_path()));
}

void ScannerPanel::populate_accounts() {
    QComboBox* combo = view_->account_combo();
    const QString keep = combo->currentData().toString();
    QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(tr("None (Yahoo fallback)"), "");
    for (const auto& acct : fincept::trading::AccountManager::instance().list_accounts())
        combo->addItem(QString("%1 (%2)").arg(acct.display_name, acct.broker_id), acct.account_id);
    const int idx = combo->findData(keep);
    if (idx >= 0)
        combo->setCurrentIndex(idx);
}

// ── Actions ─────────────────────────────────────────────────────────────────

void ScannerPanel::on_action(const QString& action) {
    if (action == QLatin1String("run")) {
        results_pane_->run(); // the user's scan: bypasses the start gate, keeps the last result dimmed
    } else if (action == QLatin1String("alert")) {
        const QJsonArray conds = view_->rules()->conditions();
        if (conds.isEmpty()) {
            view_->status_bar()->show_notice(tr("Add a rule first."), QString(), {}, 6000,
                                             fincept::ui::ScreenStatusBar::Tone::Warn);
            return;
        }
        emit create_alert_requested(conds, view_->rules()->combined_logic(), view_->symbols()->symbols(),
                                    view_->timeframe_combo()->currentText(),
                                    view_->source_combo()->currentData().toString(),
                                    view_->account_combo()->currentData().toString());
    } else if (action == QLatin1String("builder")) {
        const QString s = view_->selected_symbol();
        if (!s.isEmpty())
            emit symbol_activated(s);
    } else if (action == QLatin1String("export")) {
        export_results();
    }
}

// ── Scan ────────────────────────────────────────────────────────────────────

void ScannerPanel::start_scan(quint64 request_id) {
    auto* rules = view_->rules();
    const QJsonArray conditions = rules->conditions();
    if (conditions.isEmpty()) {
        results_pane_->fail(request_id, tr("Add at least one rule, or pick a preset."), tr("Nothing to scan for"));
        return;
    }
    // An incomplete / impossible rule never runs: the row says why, the page too.
    for (const auto& issue : rules->issues()) {
        if (!issue.blocking)
            continue;
        const QString where = issue.child >= 0 ? QStringLiteral("%1.%2").arg(issue.index + 1).arg(issue.child + 1)
                                               : QString::number(issue.index + 1);
        results_pane_->fail(request_id, tr("Rule %1: %2").arg(where, issue.reason), tr("Fix the rules first"));
        return;
    }
    const QStringList symbols = view_->symbols()->symbols();
    if (symbols.isEmpty()) {
        results_pane_->fail(request_id, tr("Add symbols, or pick a list under UNIVERSE and press LOAD."),
                            tr("No symbols to scan"));
        return;
    }
    if (scan_inflight_) {
        // One scan at a time (the answers of parallel scans cannot be told apart): this
        // one starts when the scan in flight ends, whose answer is then dropped.
        queued_id_ = request_id;
        inflight_id_ = 0;
        return;
    }

    auto source = fincept::algo::data_source_from_string(view_->source_combo()->currentData().toString());
    QString broker_id, account_id;
    if (source != fincept::algo::DataSource::YFinance) {
        account_id = view_->account_combo()->currentData().toString();
        if (!account_id.isEmpty()) {
            auto acct = fincept::trading::AccountManager::instance().get_account(account_id);
            broker_id = acct.broker_id;
        }
    }
    const QString tf = view_->timeframe_combo()->currentText();
    const int lookback_days = view_->range_combo()->isVisibleTo(view_) ? view_->range_combo()->currentData().toInt()
                                                                       : algo_default_lookback_days(tf);

    // Bare symbols are read on the loaded universe's exchange, else the broker's /
    // the user's market region — never an implicit NSE.
    const QString hint =
        exchange_hint_.isEmpty() ? AlgoTradingService::default_exchange_hint(symbols.first()) : exchange_hint_;
    scan_hint_ = hint; // result prices are shown in the currency of the listing scanned
    scan_broker_id_ = broker_id;
    scan_account_id_ = account_id;
    scan_timeframe_ = tf;
    scan_source_ = view_->source_combo()->currentData().toString();
    scan_lookback_days_ = lookback_days;
    scan_inflight_ = true;
    inflight_id_ = request_id;
    results_pane_->set_loading_text(tr("Scanning %1 symbols on %2 bars…").arg(symbols.size()).arg(tf));
    view_->status_bar()->set_text(QStringLiteral("data"),
                                  broker_id.isEmpty() ? tr("BARS · Yahoo Finance") : tr("BARS · %1").arg(broker_id));
    fincept::algo::AlgoScanner::instance().scan(conditions, symbols, tf, lookback_days, rules->combined_logic(), source,
                                                broker_id, account_id, hint);
    LOG_INFO("AlgoTrading",
             QString("Scan started: %1 conditions, %2 symbols").arg(conditions.size()).arg(symbols.size()));
}

void ScannerPanel::on_scan_result(const QJsonObject& payload) {
    scan_inflight_ = false;
    if (const quint64 id = std::exchange(inflight_id_, 0))
        results_pane_->deliver(id, payload); // → apply_scan_payload()
    LOG_DEBUG("AlgoTrading", QString("Scan results: %1 matches").arg(payload.value("matches").toArray().size()));
    if (const quint64 next = std::exchange(queued_id_, 0))
        start_scan(next);
}

void ScannerPanel::on_scan_error(const QString& msg) {
    scan_inflight_ = false;
    if (const quint64 id = std::exchange(inflight_id_, 0))
        results_pane_->fail(id, msg, tr("Scan failed"));
    view_->status_bar()->show_notice(tr("Scan failed: %1").arg(msg.left(160)), QString(), {}, 9000,
                                     fincept::ui::ScreenStatusBar::Tone::Danger);
    if (const quint64 next = std::exchange(queued_id_, 0))
        start_scan(next);
}

void ScannerPanel::apply_scan_payload(const QJsonObject& payload) {
    const QJsonArray matches = payload.value("matches").toArray();
    const int total_scanned = payload.value("total_scanned").toInt();

    // Per-symbol fetch / insufficient-data failures are listed in full (expandable),
    // so "0 matches out of 5 symbols" says that 45 of 50 never got evaluated.
    QStringList err_lines;
    for (const auto& ev : payload.value("errors").toArray())
        err_lines << ev.toObject().value("error").toString();
    view_->set_unevaluated(err_lines);

    last_scan_ = payload;
    view_->set_scan(payload, [this](const QString& s) { return result_currency(s); });

    if (matches.isEmpty()) {
        fincept::ui::EmptyState::Spec spec =
            total_scanned > 0
                ? fincept::ui::EmptyState::no_data_spec(
                      tr("No symbol matches"),
                      tr("%1 symbols were evaluated and none met the rules. Try another preset, loosen a threshold "
                         "or scan a wider list.")
                          .arg(total_scanned),
                      tr("CHANGE PRESET"))
                : fincept::ui::EmptyState::no_data_spec(
                      tr("No symbol could be evaluated"),
                      err_lines.isEmpty() ? tr("The data source returned no bars for these symbols.") : err_lines.first(),
                      tr("CHANGE PRESET"));
        spec.illustration = true;
        spec.illustration_shape = fincept::ui::LoadingSkeleton::Shape::Table;
        view_->show_no_match(spec);
        view_->set_detail_chart_state(fincept::ui::CandleChart::State::Empty, tr("No result to chart"));
    }
}

QString ScannerPanel::result_currency(const QString& symbol) const {
    // The listing the scanner read the symbol on: its exchange hint, else the symbol's own
    // default; the broker's currency as the last resort. Unknown -> "" (no unit shown).
    const QString hint = scan_hint_.isEmpty() ? AlgoTradingService::default_exchange_hint(symbol) : scan_hint_;
    return fincept::ui::algo::algo_currency_for(symbol, hint, scan_broker_id_);
}

void ScannerPanel::export_results() {
    // Machine-readable (R11): C-locale numbers, no grouping or symbol, the price's ISO code
    // in its own column; rule columns hold the value each rule was evaluated on.
    namespace fmt = fincept::ui::formatting;
    const QJsonArray matches = last_scan_.value("matches").toArray();
    if (matches.isEmpty())
        return;
    QStringList headers = {QStringLiteral("symbol"), QStringLiteral("price"),   QStringLiteral("currency"),
                           QStringLiteral("change_pct"), QStringLiteral("bias"), QStringLiteral("matched_rules")};
    headers += view_->rule_headers(); // the rule as written ("RSI < 30.00")
    QVector<QStringList> rows;
    rows.reserve(matches.size());
    for (const auto& mval : matches) {
        const QJsonObject m = mval.toObject();
        const QString symbol = m.value("symbol").toString();
        const double price = m.value("price").toDouble();
        const QJsonValue chg = m.value("change_pct");
        QStringList line = {symbol,
                            price > 0 ? fmt::export_number(price) : QString(),
                            fmt::export_money(price, result_currency(symbol)).currency,
                            chg.isDouble() ? fmt::export_number(chg.toDouble()) : QString(),
                            m.value("bias").toString(),
                            QString::number(m.value("matched").toInt())};
        for (const auto& cv : m.value("conditions").toArray()) {
            const QJsonValue computed = cv.toObject().value("computed");
            line << (computed.isDouble() ? fmt::export_number(computed.toDouble()) : QString());
        }
        while (line.size() < headers.size())
            line << QString();
        rows.append(line);
    }
    fincept::ui::ChartExport::save_csv(headers, rows, QStringLiteral("scan_results"), this);
}

// ── Remembered inputs ───────────────────────────────────────────────────────

QString ScannerPanel::inputs_signature() const {
    QVariantMap v;
    v.insert(QStringLiteral("conditions"),
             QString::fromUtf8(QJsonDocument(view_->rules()->conditions()).toJson(QJsonDocument::Compact)));
    v.insert(QStringLiteral("logic"), view_->rules()->combined_logic());
    v.insert(QStringLiteral("symbols"), view_->symbols()->symbols());
    v.insert(QStringLiteral("exchange"), exchange_hint_);
    v.insert(QStringLiteral("tf"), view_->timeframe_combo()->currentText());
    v.insert(QStringLiteral("range"), view_->range_combo()->currentData());
    v.insert(QStringLiteral("source"), view_->source_combo()->currentData());
    v.insert(QStringLiteral("account"), view_->account_combo()->currentData());
    return fincept::ui::autoload::signature_of(v);
}

void ScannerPanel::remember_inputs() {
    if (!inputs_ || applying_inputs_)
        return;
    inputs_->set_value(QStringLiteral("conditions"),
                       QString::fromUtf8(QJsonDocument(view_->rules()->conditions()).toJson(QJsonDocument::Compact)));
    inputs_->set_value(QStringLiteral("logic"), view_->rules()->combined_logic());
    inputs_->set_value(QStringLiteral("symbols"), view_->symbols()->symbols());
    inputs_->set_value(QStringLiteral("exchange"), exchange_hint_);
}

void ScannerPanel::apply_first_show_inputs() {
    if (first_inputs_done_)
        return;
    first_inputs_done_ = true;
    applying_inputs_ = true;

    // The user's last inputs (rules, symbols and the combos) …
    inputs_ = new fincept::ui::InputMemory(QStringLiteral("algo_trading"), QStringLiteral("scanner"), this);
    inputs_->track(QStringLiteral("timeframe"), view_->timeframe_combo());
    inputs_->track(QStringLiteral("range"), view_->range_combo());
    inputs_->track(QStringLiteral("source"), view_->source_combo());
    inputs_->track(QStringLiteral("account"), view_->account_combo());
    inputs_->track(QStringLiteral("universe"), view_->universe_combo());
    inputs_->restore();
    QJsonArray conds =
        QJsonDocument::fromJson(inputs_->value(QStringLiteral("conditions")).toString().toUtf8()).array();
    // The blank row the old condition editor always kept ("Close > 0") is not a rule.
    if (conds.size() == 1 && rules::is_placeholder_condition(conds.first().toObject()))
        conds = QJsonArray();
    if (!conds.isEmpty())
        view_->rules()->set_conditions(conds, inputs_->value(QStringLiteral("logic"), QStringLiteral("AND")).toString());
    const QStringList syms = inputs_->value(QStringLiteral("symbols")).toStringList();
    if (!syms.isEmpty()) {
        view_->symbols()->set_symbols(syms);
        exchange_hint_ = inputs_->value(QStringLiteral("exchange")).toString();
    }

    // … else a scan that shows something on a first visit: MACD Bullish (histogram
    // above zero — about half of a large-cap list) on the region's large caps.
    if (view_->rules()->conditions().isEmpty()) {
        const auto presets = scanner_presets();
        int index = 1;
        for (int i = 0; i < presets.size(); ++i)
            if (presets[i].name == QLatin1String("MACD Bullish"))
                index = i + 1;
        QSignalBlocker block(view_->preset_combo());
        view_->preset_combo()->setCurrentIndex(index);
        apply_preset(index);
    }
    if (view_->symbols()->symbols().isEmpty()) {
        for (const auto& u : algo_universes()) {
            if (!u.id.startsWith(QLatin1String("region:")) || u.symbols.isEmpty())
                continue;
            view_->symbols()->set_symbols(u.symbols.mid(0, kScanDefaultSymbolCap));
            exchange_hint_ = u.exchange_hint;
            const int idx = view_->universe_combo()->findData(u.id);
            if (idx >= 0) {
                QSignalBlocker block(view_->universe_combo());
                view_->universe_combo()->setCurrentIndex(idx);
            }
            break;
        }
    }
    applying_inputs_ = false;

    // What the next scan would use: a remembered scan from other inputs shows
    // "Inputs changed — RUN AGAIN" instead of being replaced silently.
    results_pane_->set_signature(inputs_signature());
    auto resign = [this]() {
        remember_inputs();
        results_pane_->set_signature(inputs_signature());
    };
    connect(view_->rules(), &fincept::ui::algo::RuleTableEditor::conditions_changed, this, resign);
    connect(view_->symbols(), &fincept::ui::algo::SymbolChipInput::symbols_changed, this, resign);
    connect(inputs_, &fincept::ui::InputMemory::changed, this,
            [this]() { results_pane_->set_signature(inputs_signature()); });
}

// ── Detail chart ────────────────────────────────────────────────────────────

void ScannerPanel::schedule_detail_fetch(const QString& symbol) {
    ++detail_seq_; // an answer for the previous selection no longer applies
    detail_inflight_ = 0;
    detail_pending_ = symbol;
    if (symbol.isEmpty()) {
        detail_timer_->stop();
        view_->set_detail_chart_state(fincept::ui::CandleChart::State::Empty, tr("Select a result to see its chart"));
        return;
    }
    view_->set_detail_chart_state(fincept::ui::CandleChart::State::Loading, tr("Loading %1…").arg(symbol));
    detail_timer_->start();
}

void ScannerPanel::fetch_detail(const QString& symbol) {
    if (symbol.isEmpty() || !isVisible() || scan_timeframe_.isEmpty())
        return;
    const quint64 seq = ++detail_seq_;
    detail_inflight_ = seq;
    const bool daily = fincept::algo::bar_seconds_for(scan_timeframe_) >= 86400;
    const int days = std::max(scan_lookback_days_, daily ? 180 : 5);
    const QString hint = scan_hint_.isEmpty() ? AlgoTradingService::default_exchange_hint(symbol) : scan_hint_;
    const QString ccy = result_currency(symbol);
    QPointer<ScannerPanel> self(this);
    // Network + parsing happen in the fetcher (asynchronous); only the drawing is here.
    fincept::algo::CandleDataFetcher::instance().fetch(
        symbol, scan_timeframe_, days, fincept::algo::data_source_from_string(scan_source_), scan_broker_id_,
        scan_account_id_,
        [self, seq, symbol, ccy](bool ok, const QVector<fincept::algo::OhlcvCandle>& candles, const QString& err) {
            if (!self || seq != self->detail_seq_)
                return; // superseded, hidden or gone
            self->detail_inflight_ = 0;
            if (!ok || candles.isEmpty()) {
                self->view_->set_detail_chart_state(fincept::ui::CandleChart::State::Error,
                                                    err.isEmpty() ? tr("No bars for %1").arg(symbol) : err);
                return;
            }
            QVector<fincept::ui::CandleChart::Bar> bars;
            bars.reserve(candles.size());
            for (const auto& c : candles) {
                fincept::ui::CandleChart::Bar b;
                b.time_ms = static_cast<double>(c.open_time);
                b.open = c.open;
                b.high = c.high;
                b.low = c.low;
                b.close = c.close;
                b.volume = c.volume;
                bars.append(b);
            }
            self->view_->set_detail_bars(symbol, bars, ccy);
        },
        hint);
}

// ── Show / hide / language ──────────────────────────────────────────────────

void ScannerPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    populate_accounts();
    fill_universes(); // the user lists file may have been edited meanwhile
    // Before the results pane decides on its default scan (one event-loop turn later).
    apply_first_show_inputs();
    if (detail_dropped_) {
        detail_dropped_ = false;
        schedule_detail_fetch(view_->selected_symbol()); // the chart the last hide dropped
    }
}

void ScannerPanel::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    // Cancel on leave: a pending fetch does not start, an answer arriving now is dropped.
    if (detail_timer_->isActive() || detail_inflight_ != 0) {
        detail_timer_->stop();
        detail_inflight_ = 0;
        detail_dropped_ = true;
        ++detail_seq_;
    }
}

void ScannerPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslate_inputs();
        fill_universes();
    }
    QWidget::changeEvent(event);
}

// ── CLOSE pre-fill helper ─────────────────────────────────────────────────────

void ScannerPanel::prefill_close_from_price(const QString& /*symbol*/, double price) {
    if (price <= 0)
        return;
    QJsonArray conds = view_->rules()->conditions();
    bool changed = false;
    std::function<void(QJsonArray&)> walk = [&](QJsonArray& arr) {
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject o = arr[i].toObject();
            if (o.contains("children")) {
                QJsonArray ch = o.value("children").toArray();
                walk(ch);
                o["children"] = ch;
            } else if (o.value("indicator").toString() == "CLOSE" &&
                       o.value("compare_mode").toString("value") == "value" &&
                       qFuzzyIsNull(o.value("value").toDouble())) {
                o["value"] = price;
                changed = true;
            }
            arr[i] = o;
        }
    };
    walk(conds);
    if (changed)
        view_->rules()->set_conditions(conds, view_->rules()->combined_logic());
}

} // namespace fincept::screens
