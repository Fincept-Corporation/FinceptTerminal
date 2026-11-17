// src/screens/algo_trading/ScannerView.cpp
#include "screens/algo_trading/ScannerView.h"

#include "screens/algo_trading/AlgoTabKit.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/KeySortProxy.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/algo/AlgoCurrency.h"
#include "ui/widgets/algo/RuleTableEditor.h"
#include "ui/widgets/algo/SymbolChipInput.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens {

namespace kit = fincept::screens::algo_kit;
namespace fmt = fincept::ui::formatting;

namespace {

constexpr double kScvNaN = std::numeric_limits<double>::quiet_NaN();

enum ScvCol { kScvSymbol, kScvPrice, kScvChange, kScvSignal, kScvMatch, kScvFixed };

QString scv_tr(const char* text, const char* disambiguation = nullptr, int n = -1) {
    return QCoreApplication::translate("fincept::screens::ScannerView", text, disambiguation, n);
}

/// "RSI < 30", "MACD.histogram > 0" — the rule as the scan reports it.
QString scv_rule_text(const QJsonObject& c) {
    const QString fld = c.value(QStringLiteral("field")).toString();
    const QString op = c.value(QStringLiteral("operator")).toString();
    QString rhs;
    if (op != QLatin1String("rising") && op != QLatin1String("falling"))
        rhs = fmt::number(c.value(QStringLiteral("value")).toDouble(), 2);
    QString opt = op;
    opt.replace(QLatin1Char('_'), QLatin1Char(' '));
    return QStringLiteral("%1%2 %3 %4")
        .arg(c.value(QStringLiteral("indicator")).toString(),
             fld.isEmpty() || fld == QLatin1String("value") ? QString() : QLatin1Char('.') + fld, opt, rhs)
        .trimmed();
}

QString scv_value_text(double v) {
    if (!std::isfinite(v))
        return fmt::placeholder();
    return fmt::number(v, std::abs(v) < 10 ? 4 : 2);
}

} // namespace

struct ScannerView::Row {
    struct Rule {
        QString text;
        bool met = false;
        double value = kScvNaN;
        QString error;
        QString reason;
    };
    QString symbol;
    double price = kScvNaN;
    QString currency;
    double price_key = kScvNaN;
    double change = kScvNaN;
    int bias = 0; ///< +1 bullish, -1 bearish
    QString reasons;
    int matched = 0;
    int total = 0;
    QVector<Rule> rules;
};

ScannerView::ScannerView(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("algoScannerView"));
    build_ui();
    install_shortcuts();
    retranslate();
}

ScannerView::~ScannerView() = default;

// ── Layout ──────────────────────────────────────────────────────────────────

void ScannerView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    using Role = ui::ScreenCommandBar::Role;
    bar_ = kit::make_tab_bar(this, kit::TabScanner);
    bar_->add_action(QStringLiteral("run"), QString(), QStringLiteral("Ctrl+↵"), QString(), Role::Primary, 100);
    bar_->add_action(QStringLiteral("alert"), QString(), QString(), QString(), Role::Normal, 70);
    bar_->add_action(QStringLiteral("builder"), QString(), QStringLiteral("Ctrl+B"), QString(), Role::Normal, 60);
    bar_->add_action(QStringLiteral("export"), QString(), QStringLiteral("Ctrl+E"), QString(), Role::Normal, 40);
    connect(bar_, &ui::ScreenCommandBar::action_triggered, this, [this](const QString& id) { emit action_requested(id); });
    root->addWidget(bar_);

    // ── Strip: the condition set, the universe and the data ──
    ui::FlowLayout* flow = nullptr;
    QWidget* strip = kit::make_strip(this, &flow);
    auto combo = [strip]() {
        auto* c = new QComboBox(strip);
        c->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        c->setObjectName(QStringLiteral("algoField"));
        kit::guard_wheel(c);
        return c;
    };
    preset_ = combo();
    universe_ = combo();
    universe_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    universe_->setMinimumContentsLength(18);
    load_universe_ = kit::make_tool_button(QString(), QString(), strip);
    save_universe_ = kit::make_tool_button(QString(), QString(), strip);
    timeframe_ = combo();
    range_ = combo();
    source_ = combo();
    account_ = combo();
    account_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    account_->setMinimumContentsLength(14);
    QWidget* g_preset = kit::field_group(strip, scv_tr("PRESET"), {preset_});
    QWidget* g_universe = kit::field_group(strip, scv_tr("UNIVERSE"), {universe_, load_universe_, save_universe_});
    QWidget* g_tf = kit::field_group(strip, scv_tr("BARS"), {timeframe_});
    range_group_ = kit::field_group(strip, scv_tr("RANGE"), {range_});
    QWidget* g_source = kit::field_group(strip, scv_tr("SOURCE"), {source_});
    account_group_ = kit::field_group(strip, scv_tr("ACCOUNT"), {account_});
    preset_cap_ = g_preset->findChild<QLabel*>();
    universe_cap_ = g_universe->findChild<QLabel*>();
    tf_cap_ = g_tf->findChild<QLabel*>();
    range_cap_ = range_group_->findChild<QLabel*>();
    source_cap_ = g_source->findChild<QLabel*>();
    account_cap_ = account_group_->findChild<QLabel*>();
    for (QWidget* g : {g_preset, g_universe, g_tf, range_group_, g_source, account_group_})
        flow->addWidget(g);
    root->addWidget(strip);

    // ── Panels ──
    auto* main = kit::make_splitter(Qt::Horizontal, this);
    auto* left = kit::make_splitter(Qt::Vertical, main);
    auto* right = kit::make_splitter(Qt::Vertical, main);

    kit::Panel rules_panel = kit::make_panel(QString(), left, 0);
    rules_header_ = rules_panel.header;
    auto* rules_scroll = new QScrollArea(rules_panel.body_widget);
    rules_scroll->setObjectName(QStringLiteral("algoScroll"));
    rules_scroll->setWidgetResizable(true);
    rules_scroll->setFrameShape(QFrame::NoFrame);
    rules_ = new ui::algo::RuleTableEditor(ui::algo::RuleTableEditor::Kind::Scan, rules_scroll);
    rules_->setContentsMargins(8, 6, 8, 6);
    rules_scroll->setWidget(rules_);
    rules_panel.body->addWidget(rules_scroll, 1);
    // The editor's ALL / ANY + RULE + GROUP controls belong in the panel header.
    rules_header_->add_control(rules_->header_controls());
    rules_->header_controls()->show();

    kit::Panel sym_panel = kit::make_panel(QString(), left, 0);
    symbols_header_ = sym_panel.header;
    auto* sym_scroll = new QScrollArea(sym_panel.body_widget);
    sym_scroll->setObjectName(QStringLiteral("algoScroll"));
    sym_scroll->setWidgetResizable(true);
    sym_scroll->setFrameShape(QFrame::NoFrame);
    sym_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* sym_host = new QWidget(sym_scroll);
    auto* sh = new QVBoxLayout(sym_host);
    sh->setContentsMargins(8, 6, 8, 6);
    sh->setSpacing(4);
    symbols_ = new ui::algo::SymbolChipInput(sym_host);
    sh->addWidget(symbols_);
    sh->addStretch(1);
    sym_scroll->setWidget(sym_host);
    sym_panel.body->addWidget(sym_scroll, 1);
    connect(symbols_, &ui::algo::SymbolChipInput::symbols_changed, this,
            [this]() { symbols_header_->set_badge(QString::number(symbols_->symbols().size())); });

    left->addWidget(rules_panel.frame);
    left->addWidget(sym_panel.frame);
    left->setStretchFactor(0, 3);
    left->setStretchFactor(1, 2);

    kit::Panel res_panel = kit::make_panel(QString(), right, 0);
    results_header_ = res_panel.header;
    asof_ = new QLabel(res_panel.header);
    asof_->setObjectName(QStringLiteral("algoNote"));
    errors_btn_ = kit::make_tool_button(QString(), QString(), res_panel.header);
    errors_btn_->setCheckable(true);
    errors_btn_->setVisible(false);
    kit::set_tone(errors_btn_, kit::Tone::Warn);
    results_header_->add_control(asof_);
    results_header_->add_control(errors_btn_);
    results_host_ = new QWidget(res_panel.body_widget);
    auto* rh = new QVBoxLayout(results_host_);
    rh->setContentsMargins(0, 0, 0, 0);
    rh->setSpacing(0);
    res_panel.body->addWidget(results_host_, 1);
    auto* err_scroll = new QScrollArea(res_panel.body_widget);
    err_scroll->setObjectName(QStringLiteral("algoScroll"));
    err_scroll->setWidgetResizable(true);
    err_scroll->setFrameShape(QFrame::NoFrame);
    err_scroll->setVisible(false);
    err_scroll->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 7 + 12);
    errors_ = kit::make_note(QString(), err_scroll, kit::Tone::None);
    errors_->setContentsMargins(8, 4, 8, 4);
    err_scroll->setWidget(errors_);
    res_panel.body->addWidget(err_scroll);
    connect(errors_btn_, &QPushButton::toggled, err_scroll, &QWidget::setVisible);

    kit::Panel detail = kit::make_panel(QString(), right, 0);
    detail_header_ = detail.header;
    auto* dbox = new QWidget(detail.body_widget);
    auto* dh = new QHBoxLayout(dbox);
    dh->setContentsMargins(0, 0, 0, 0);
    dh->setSpacing(0);
    chart_ = new ui::CandleChart(dbox);
    chart_->set_export_name(QStringLiteral("scan_symbol"));
    chart_->set_show_volume(true);
    chart_->set_default_visible_bars(120);
    dh->addWidget(chart_, 3);
    auto* vscroll = new QScrollArea(dbox);
    vscroll->setObjectName(QStringLiteral("algoScroll"));
    vscroll->setWidgetResizable(true);
    vscroll->setFrameShape(QFrame::NoFrame);
    vscroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    vscroll->setMinimumWidth(QFontMetrics(font()).averageCharWidth() * 30);
    values_box_ = new QWidget(vscroll);
    auto* vv = new QVBoxLayout(values_box_);
    vv->setContentsMargins(8, 6, 8, 6);
    vv->setSpacing(4);
    values_ = kit::make_note(QString(), values_box_);
    vv->addWidget(values_);
    detail_empty_ = kit::make_note(QString(), values_box_);
    vv->addWidget(detail_empty_);
    vv->addStretch(1);
    vscroll->setWidget(values_box_);
    dh->addWidget(vscroll, 2);
    detail.body->addWidget(dbox, 1);

    right->addWidget(res_panel.frame);
    right->addWidget(detail.frame);
    right->setStretchFactor(0, 3);
    right->setStretchFactor(1, 2);

    main->addWidget(left);
    main->addWidget(right);
    main->setStretchFactor(0, 5);
    main->setStretchFactor(1, 6);
    root->addWidget(main, 1);

    status_ = kit::make_status_bar(this);
    root->addWidget(status_);

    chart_->set_state(ui::CandleChart::State::Empty, scv_tr("Select a result to see its chart"));
    update_detail_values();
}

QWidget* ScannerView::build_results_content(QWidget* parent) {
    if (results_stack_)
        return results_stack_;
    results_stack_ = new QStackedWidget(parent);
    table_ = new QTableView(results_stack_);
    model_ = new ui::LiveTableModel<Row>(table_);
    model_->set_key([](const Row& r) { return r.symbol; });
    model_->set_cell([](const Row& r, int col, int role) -> QVariant {
        using Model = ui::LiveTableModel<Row>;
        if (role == Qt::TextAlignmentRole)
            return col == kScvSymbol || col == kScvSignal
                       ? Qt::Alignment(Qt::AlignLeft | Qt::AlignVCenter).toInt()
                       : Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter).toInt();
        const int rule = col - kScvFixed;
        if (role == Qt::DisplayRole) {
            switch (col) {
                case kScvSymbol:
                    return r.symbol;
                case kScvPrice:
                    return std::isfinite(r.price) && r.price > 0
                               ? fmt::price_with_symbol(r.price, fmt::instrument_for_symbol(r.symbol, r.currency))
                               : fmt::placeholder();
                case kScvChange:
                    return std::isfinite(r.change) ? fmt::pct(r.change, 2, true) : fmt::placeholder();
                case kScvSignal:
                    return r.bias > 0 ? scv_tr("▲ BULLISH") : (r.bias < 0 ? scv_tr("▼ BEARISH") : scv_tr("NEUTRAL"));
                case kScvMatch:
                    return QStringLiteral("%1/%2").arg(r.matched).arg(r.total);
                default:
                    break;
            }
            if (rule >= 0 && rule < r.rules.size()) {
                const auto& c = r.rules[rule];
                return (c.met ? QStringLiteral("✓ ") : QStringLiteral("✗ ")) +
                       (std::isfinite(c.value) ? scv_value_text(c.value)
                                               : (c.error.isEmpty() ? fmt::placeholder() : c.error));
            }
            return {};
        }
        if (role == Model::SortRole) {
            switch (col) {
                case kScvSymbol:
                    return r.symbol;
                case kScvPrice:
                    return r.price_key;
                case kScvChange:
                    return r.change;
                case kScvSignal:
                    return r.bias;
                case kScvMatch:
                    return r.total > 0 ? double(r.matched) / r.total : kScvNaN;
                default:
                    break;
            }
            if (rule >= 0 && rule < r.rules.size())
                return r.rules[rule].value;
            return {};
        }
        if (role == Qt::ForegroundRole) {
            if (col == kScvChange)
                return kit::tone_color(kit::sign_tone(r.change));
            if (col == kScvSignal) // bullish / bearish is price direction (with its arrow)
                return kit::tone_color(r.bias > 0 ? kit::Tone::Pos : (r.bias < 0 ? kit::Tone::Neg : kit::Tone::Dim));
            if (rule >= 0 && rule < r.rules.size())
                return kit::tone_color(r.rules[rule].met ? kit::Tone::Ok : kit::Tone::Dim);
            return {};
        }
        if (role == Qt::ToolTipRole) {
            if (col == kScvSignal)
                return r.reasons;
            if (col == kScvChange)
                return scv_tr("Change against the previous session's close");
            if (col == kScvMatch)
                return scv_tr("%1 of %2 rules hold on the last bar").arg(r.matched).arg(r.total);
            if (rule >= 0 && rule < r.rules.size())
                return r.rules[rule].reason.isEmpty() ? r.rules[rule].text
                                                      : r.rules[rule].text + QStringLiteral("\n") + r.rules[rule].reason;
            return {};
        }
        return {};
    });
    proxy_ = new ui::KeySortProxy(table_);
    proxy_->setSourceModel(model_);
    proxy_->setSortRole(ui::LiveTableModel<Row>::SortRole);
    table_->setModel(proxy_);
    kit::setup_table(table_, QStringLiteral("scan_results"));
    table_->setToolTip(scv_tr("Double-click a row (or Enter) to backtest the symbol in the Builder"));
    model_->set_headers({scv_tr("SYMBOL"), scv_tr("PRICE"), scv_tr("CHG %"), scv_tr("SIGNAL"), scv_tr("MATCH")});
    kit::set_column_chars(table_, {11, 13, 8, 10, 7});
    results_stack_->addWidget(table_);
    connect(table_, &QTableView::doubleClicked, this, [this](const QModelIndex&) {
        const QString s = selected_symbol();
        if (!s.isEmpty())
            emit activated(s);
    });
    auto* enter = new QShortcut(QKeySequence(Qt::Key_Return), table_);
    enter->setContext(Qt::WidgetShortcut);
    connect(enter, &QShortcut::activated, this, [this]() {
        const QString s = selected_symbol();
        if (!s.isEmpty())
            emit activated(s);
    });
    connect(table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this]() {
        update_detail_values();
        emit selection_changed(selected_symbol());
    });

    no_match_ = new ui::EmptyState(results_stack_);
    no_match_->set_compact(true);
    connect(no_match_, &ui::EmptyState::primary_clicked, this, &ScannerView::no_match_action);
    results_stack_->addWidget(no_match_);
    results_stack_->setCurrentWidget(table_);
    return results_stack_;
}

void ScannerView::install_shortcuts() {
    auto add = [this](const QKeySequence& seq, const QString& action) {
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, [this, action]() { emit action_requested(action); });
    };
    add(QKeySequence(Qt::CTRL | Qt::Key_Return), QStringLiteral("run"));
    add(QKeySequence(Qt::CTRL | Qt::Key_Enter), QStringLiteral("run"));
    add(QKeySequence(Qt::CTRL | Qt::Key_B), QStringLiteral("builder"));
    add(QKeySequence(Qt::CTRL | Qt::Key_E), QStringLiteral("export"));
}

void ScannerView::retranslate() {
    bar_->set_action_text(QStringLiteral("run"), running_ ? scv_tr("SCANNING…") : scv_tr("RUN SCAN"));
    bar_->set_action_tooltip(QStringLiteral("run"), scv_tr("Scan the symbols with these rules now (Ctrl+Enter)"));
    bar_->set_action_text(QStringLiteral("alert"), scv_tr("CREATE ALERT"));
    bar_->set_action_tooltip(QStringLiteral("alert"),
                             scv_tr("Watch these rules in the background: opens ALERTS pre-filled (nothing starts "
                                    "until you save the watch there)"));
    bar_->set_action_text(QStringLiteral("builder"), scv_tr("OPEN IN BUILDER"));
    bar_->set_action_tooltip(QStringLiteral("builder"),
                             scv_tr("Backtest the selected result's symbol in the Builder (Ctrl+B)"));
    bar_->set_action_text(QStringLiteral("export"), scv_tr("EXPORT CSV…"));
    bar_->set_action_tooltip(QStringLiteral("export"),
                             scv_tr("The results as CSV: plain numbers and a currency column (Ctrl+E)"));
    if (preset_cap_)
        preset_cap_->setText(scv_tr("PRESET"));
    if (universe_cap_)
        universe_cap_->setText(scv_tr("UNIVERSE"));
    if (tf_cap_)
        tf_cap_->setText(scv_tr("BARS"));
    if (range_cap_)
        range_cap_->setText(scv_tr("RANGE"));
    if (source_cap_)
        source_cap_->setText(scv_tr("SOURCE"));
    if (account_cap_)
        account_cap_->setText(scv_tr("ACCOUNT"));
    kit::mark_field(preset_, scv_tr("Preset rule set"), scv_tr("A ready-made condition set; edit it in RULES"));
    kit::mark_field(universe_, scv_tr("Universe"),
                    scv_tr("Symbol lists: region large caps, your saved lists, NSE lists — LOAD puts them in SYMBOLS"));
    kit::mark_field(timeframe_, scv_tr("Bar size"), scv_tr("The rules are evaluated on the last bar of this size"));
    kit::mark_field(range_, scv_tr("History range"), scv_tr("How much history is fetched for the indicators"));
    kit::mark_field(source_, scv_tr("Data source"));
    kit::mark_field(account_, scv_tr("Broker account"), scv_tr("The broker the bars come from (Auto / Broker only)"));
    load_universe_->setText(scv_tr("LOAD"));
    load_universe_->setToolTip(scv_tr("Replace SYMBOLS with the chosen list"));
    load_universe_->setAccessibleName(scv_tr("Load list"));
    save_universe_->setText(scv_tr("SAVE LIST…"));
    save_universe_->setToolTip(scv_tr("Save SYMBOLS as a named list (Universe tab and here)"));
    save_universe_->setAccessibleName(scv_tr("Save list"));
    rules_header_->set_title(scv_tr("Rules"));
    symbols_header_->set_title(scv_tr("Symbols"));
    results_header_->set_title(scv_tr("Results"));
    detail_header_->set_title(scv_tr("Detail"));
    errors_btn_->setToolTip(scv_tr("Symbols the scan could not evaluate, and why"));
    detail_empty_->setText(scv_tr("Select a result: its candles and the value each rule was evaluated on appear "
                                  "here."));
    status_->set_text(QStringLiteral("data"), scv_tr("BARS · broker or Yahoo Finance"));
    if (model_ && model_->columnCount() >= kScvFixed) {
        QStringList headers = {scv_tr("SYMBOL"), scv_tr("PRICE"), scv_tr("CHG %"), scv_tr("SIGNAL"), scv_tr("MATCH")};
        headers += rule_headers_;
        model_->set_headers(headers);
    }
    update_summary();
}

void ScannerView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void ScannerView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    status_->start_clock();
}

void ScannerView::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    status_->stop_clock();
}

// ── Results ─────────────────────────────────────────────────────────────────

void ScannerView::set_scan(const QJsonObject& payload, const std::function<QString(const QString&)>& currency_of) {
    if (!results_stack_)
        return;
    const QJsonArray matches = payload.value(QStringLiteral("matches")).toArray();
    condition_count_ = payload.value(QStringLiteral("condition_count")).toInt();
    total_scanned_ = payload.value(QStringLiteral("total_scanned")).toInt();

    // Prices are in each symbol's own currency; a PRICE column mixing currencies sorts on
    // USD-normalised keys (never raw numbers in different currencies).
    QHash<QString, QString> ccy_of;
    fmt::CurrencyMix mix;
    bool any_unknown = false;
    for (const auto& mv : matches) {
        const QString s = mv.toObject().value(QStringLiteral("symbol")).toString();
        const QString c = currency_of ? currency_of(s) : QString();
        ccy_of.insert(s, c);
        any_unknown = any_unknown || c.isEmpty();
        mix.add(c);
    }
    const bool price_mixed = mix.mixed() || (any_unknown && !mix.isEmpty());

    rule_headers_.clear();
    if (!matches.isEmpty())
        for (const auto& cv : matches.first().toObject().value(QStringLiteral("conditions")).toArray())
            rule_headers_ << scv_rule_text(cv.toObject());

    QVector<Row> rows;
    rows.reserve(matches.size());
    for (const auto& mv : matches) {
        const QJsonObject m = mv.toObject();
        Row r;
        r.symbol = m.value(QStringLiteral("symbol")).toString();
        r.price = m.value(QStringLiteral("price")).toDouble(kScvNaN);
        r.currency = ccy_of.value(r.symbol);
        r.price_key = std::isfinite(r.price) && r.price > 0 ? fincept::ui::algo::algo_sort_key(r.price, r.currency, price_mixed)
                                                            : kScvNaN;
        const QJsonValue chg = m.value(QStringLiteral("change_pct"));
        r.change = chg.isDouble() ? chg.toDouble() : kScvNaN;
        // Direction from what the matched rules MEAN (RSI < 30 is oversold → bullish),
        // computed by the engine (algo_engine/ScanSignal.h) — not from the operator.
        const QString bias = m.value(QStringLiteral("bias")).toString();
        r.bias = bias == QLatin1String("bullish") ? 1 : (bias == QLatin1String("bearish") ? -1 : 0);
        QStringList reasons;
        const QJsonArray conds = m.value(QStringLiteral("conditions")).toArray();
        for (const auto& cv : conds) {
            const QJsonObject c = cv.toObject();
            Row::Rule rule;
            rule.text = scv_rule_text(c);
            rule.met = c.value(QStringLiteral("met")).toBool();
            const QJsonValue computed = c.value(QStringLiteral("computed"));
            rule.value = computed.isDouble() ? computed.toDouble() : kScvNaN;
            rule.error = c.value(QStringLiteral("error")).toString();
            rule.reason = c.value(QStringLiteral("bias_reason")).toString();
            if (rule.met && !rule.reason.isEmpty())
                reasons << rule.reason;
            r.rules.append(rule);
        }
        reasons.removeDuplicates();
        r.reasons = reasons.join(QStringLiteral(", "));
        r.matched = m.value(QStringLiteral("matched")).toInt();
        r.total = condition_count_ > 0 ? condition_count_ : static_cast<int>(conds.size());
        rows.append(r);
    }

    const QString keep = selected_symbol();
    QStringList headers = {scv_tr("SYMBOL"), scv_tr("PRICE"), scv_tr("CHG %"), scv_tr("SIGNAL"), scv_tr("MATCH")};
    headers += rule_headers_;
    model_->set_headers(headers);
    model_->upsert(rows);
    QVector<int> chars = {11, 13, 8, 10, 7};
    for (int i = 0; i < rule_headers_.size(); ++i)
        chars << std::clamp(static_cast<int>(rule_headers_[i].size()) + 2, 9, 22);
    kit::set_column_chars(table_, chars);
    if (table_->horizontalHeader()->sortIndicatorSection() < 0)
        table_->sortByColumn(kScvMatch, Qt::DescendingOrder);
    // Keep the symbol the user was looking at; else the first row.
    int src = keep.isEmpty() ? -1 : model_->index_of_key(keep);
    QModelIndex idx = src >= 0 ? proxy_->mapFromSource(model_->index(src, 0)) : proxy_->index(0, 0);
    if (idx.isValid())
        table_->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    if (!rows.isEmpty())
        results_stack_->setCurrentWidget(table_);
    update_detail_values();
    update_summary();
    emit selection_changed(selected_symbol());
}

void ScannerView::show_no_match(const ui::EmptyState::Spec& spec) {
    if (!results_stack_)
        return;
    no_match_->apply(spec);
    results_stack_->setCurrentWidget(no_match_);
}

void ScannerView::set_unevaluated(const QStringList& lines) {
    errors_->setText(lines.join(QLatin1Char('\n')));
    errors_btn_->setText(scv_tr("! %n NOT EVALUATED", nullptr, static_cast<int>(lines.size())));
    errors_btn_->setAccessibleName(errors_btn_->text());
    errors_btn_->setVisible(!lines.isEmpty());
    if (lines.isEmpty())
        errors_btn_->setChecked(false);
}

void ScannerView::set_running(bool running) {
    running_ = running;
    bar_->set_action_text(QStringLiteral("run"), running ? scv_tr("SCANNING…") : scv_tr("RUN SCAN"));
    bar_->set_action_enabled(QStringLiteral("run"), !running);
    kit::set_state(status_, running ? kit::RunState::Running : kit::RunState::Ready,
                   running ? scv_tr("SCANNING") : QString());
}

int ScannerView::result_count() const {
    return model_ ? model_->rowCount() : 0;
}

QString ScannerView::selected_symbol() const {
    if (!table_ || !table_->selectionModel())
        return {};
    const QModelIndex cur = table_->selectionModel()->currentIndex();
    if (!cur.isValid())
        return {};
    return proxy_->mapToSource(cur).siblingAtColumn(0).data(ui::LiveTableModel<Row>::KeyRole).toString();
}

void ScannerView::update_summary() {
    const int n = result_count();
    results_header_->set_badge(total_scanned_ > 0 ? scv_tr("%1 of %2").arg(n).arg(total_scanned_) : QString());
    status_->set_text(QStringLiteral("info"), total_scanned_ > 0
                                                  ? scv_tr("%1 match(es) · %2 symbols evaluated").arg(n).arg(total_scanned_)
                                                  : QString());
    bar_->set_action_enabled(QStringLiteral("export"), n > 0);
    bar_->set_action_enabled(QStringLiteral("builder"), !selected_symbol().isEmpty());
}

// ── Detail ──────────────────────────────────────────────────────────────────

void ScannerView::update_detail_values() {
    // Rebuild the per-rule lines (a few labels) for the selected row.
    for (QWidget* w : values_box_->findChildren<QWidget*>(QStringLiteral("algoRuleLine"), Qt::FindDirectChildrenOnly)) {
        values_box_->layout()->removeWidget(w);
        w->hide();
        w->setObjectName(QString());
        w->deleteLater();
    }
    const QString sym = selected_symbol();
    const Row* r = (model_ && !sym.isEmpty()) ? model_->row_for_key(sym) : nullptr;
    detail_header_->set_badge(sym);
    detail_empty_->setVisible(!r);
    values_->setVisible(r != nullptr);
    if (bar_)
        bar_->set_action_enabled(QStringLiteral("builder"), r != nullptr);
    if (!r)
        return;
    values_->setText(scv_tr("%1 of %2 rules hold on the last bar · %3")
                         .arg(r->matched)
                         .arg(r->total)
                         .arg(r->bias > 0 ? scv_tr("▲ bullish") : (r->bias < 0 ? scv_tr("▼ bearish") : scv_tr("neutral"))));
    auto* layout = static_cast<QVBoxLayout*>(values_box_->layout());
    int at = layout->indexOf(detail_empty_);
    for (const auto& rule : r->rules) {
        auto* line = new QWidget(values_box_);
        line->setObjectName(QStringLiteral("algoRuleLine"));
        auto* h = new QHBoxLayout(line);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        auto* mark = kit::make_chip(rule.met ? scv_tr("✓ MET") : scv_tr("✗ NOT"), rule.met ? kit::Tone::Ok : kit::Tone::Dim,
                                    line);
        auto* text = new QLabel(rule.text, line);
        text->setObjectName(QStringLiteral("algoKey"));
        text->setWordWrap(true);
        text->setToolTip(rule.reason);
        auto* value = new QLabel(std::isfinite(rule.value) ? scv_value_text(rule.value)
                                                           : (rule.error.isEmpty() ? fmt::placeholder() : rule.error),
                                 line);
        value->setObjectName(QStringLiteral("algoValue"));
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        value->setToolTip(scv_tr("The value the rule was evaluated on"));
        h->addWidget(mark, 0, Qt::AlignLeft | Qt::AlignVCenter);
        h->addWidget(text, 1);
        h->addWidget(value, 0, Qt::AlignRight | Qt::AlignVCenter);
        layout->insertWidget(at++, line);
    }
}

void ScannerView::set_detail_bars(const QString& symbol, const QVector<ui::CandleChart::Bar>& bars,
                                  const QString& currency) {
    chart_symbol_ = symbol;
    chart_->clear();
    if (bars.isEmpty()) {
        chart_->set_state(ui::CandleChart::State::Empty, scv_tr("No bars for %1").arg(symbol));
        return;
    }
    chart_->set_state(ui::CandleChart::State::Ready);
    chart_->set_title(symbol);
    chart_->set_bars(bars);
    const fmt::InstrumentFmt inst = fmt::instrument_for_symbol(symbol, currency);
    chart_->set_price_formatter([inst](double x) { return fmt::price(x, inst); });
    chart_->reset_view();
}

void ScannerView::set_detail_chart_state(ui::CandleChart::State state, const QString& message) {
    if (state != ui::CandleChart::State::Ready)
        chart_->clear();
    chart_->set_state(state, message);
}

void ScannerView::set_account_visible(bool visible) {
    account_group_->setVisible(visible);
}

void ScannerView::set_range_visible(bool visible) {
    range_group_->setVisible(visible);
}

} // namespace fincept::screens
