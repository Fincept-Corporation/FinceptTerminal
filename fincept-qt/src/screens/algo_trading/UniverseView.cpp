// src/screens/algo_trading/UniverseView.cpp
#include "screens/algo_trading/UniverseView.h"

#include "ui/formatting/NumberFormat.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/KeySortProxy.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/algo/SymbolChipInput.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QScrollArea>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

#include <cmath>
#include <limits>

namespace fincept::screens {

namespace kit = fincept::screens::algo_kit;
namespace fmt = fincept::ui::formatting;

namespace {

/// A universe scan can run for hours across thousands of symbols: the most recent matches only.
constexpr int kUvMaxMatchRows = 500;

enum UvCol { kUvTime, kUvSymbol, kUvSignal, kUvPrice, kUvDetail, kUvDeploy, kUvCount };
enum UvPage { kUvPageTable = 0, kUvPageState = 1 };

QString uv_tr(const char* text, const char* disambiguation = nullptr, int n = -1) {
    return QCoreApplication::translate("fincept::screens::UniverseView", text, disambiguation, n);
}

} // namespace

struct UniverseView::Row {
    quint64 seq = 0;
    qint64 time_ms = 0;
    QString symbol;
    int direction = 0;
    double price = std::numeric_limits<double>::quiet_NaN();
    QString currency;
    QString detail;
};

UniverseView::UniverseView(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("algoUniverseView"));
    build_ui();
    retranslate();
}

UniverseView::~UniverseView() = default;

void UniverseView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    using Role = ui::ScreenCommandBar::Role;
    bar_ = kit::make_tab_bar(this, kit::TabUniverse);
    // START begins a background watch that only ALERTS (no order); DEPLOY… opens the deploy
    // dialog, which is where an order can start — and carries its own marker.
    bar_->add_action(QStringLiteral("start"), QString(), QString(), QString(), Role::Primary, 100);
    bar_->add_action(QStringLiteral("deploy"), QString(), QStringLiteral("↵"), QString(), Role::Normal, 60);
    bar_->add_action(QStringLiteral("clear"), QString(), QString(), QString(), Role::Normal, 40);
    connect(bar_, &ui::ScreenCommandBar::action_triggered, this, [this](const QString& id) {
        if (id == QLatin1String("deploy")) {
            const QString s = selected_symbol();
            if (!s.isEmpty())
                emit deploy_requested(s);
            return;
        }
        emit action_requested(id);
    });
    root->addWidget(bar_);

    // ── Strip ──
    ui::FlowLayout* flow = nullptr;
    QWidget* strip = kit::make_strip(this, &flow);
    auto combo = [strip](int min_chars) {
        auto* c = new QComboBox(strip);
        c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        c->setMinimumContentsLength(min_chars);
        kit::guard_wheel(c);
        return c;
    };
    strategy_ = combo(18);
    universe_ = combo(18);
    account_ = combo(14);
    sweep_ = new QSpinBox(strip);
    sweep_->setRange(200, 5000);
    sweep_->setSingleStep(100);
    sweep_->setValue(500);
    kit::guard_wheel(sweep_);
    cooldown_ = new QSpinBox(strip);
    cooldown_->setRange(0, 240);
    cooldown_->setValue(5);
    kit::guard_wheel(cooldown_);
    QWidget* g_strategy = kit::field_group(strip, uv_tr("STRATEGY"), {strategy_});
    QWidget* g_universe = kit::field_group(strip, uv_tr("UNIVERSE"), {universe_});
    QWidget* g_account = kit::field_group(strip, uv_tr("ACCOUNT"), {account_});
    QWidget* g_sweep = kit::field_group(strip, uv_tr("SWEEP"), {sweep_});
    QWidget* g_cooldown = kit::field_group(strip, uv_tr("COOLDOWN"), {cooldown_});
    strategy_cap_ = g_strategy->findChild<QLabel*>();
    universe_cap_ = g_universe->findChild<QLabel*>();
    account_cap_ = g_account->findChild<QLabel*>();
    sweep_cap_ = g_sweep->findChild<QLabel*>();
    cooldown_cap_ = g_cooldown->findChild<QLabel*>();
    for (QWidget* g : {g_strategy, g_universe, g_account, g_sweep, g_cooldown})
        flow->addWidget(g);
    root->addWidget(strip);

    // Custom symbols: their own band (a chip list wraps over several lines).
    custom_strip_ = new QWidget(this);
    custom_strip_->setObjectName(QStringLiteral("algoStrip"));
    custom_strip_->setAttribute(Qt::WA_StyledBackground, true);
    auto* ch = new QHBoxLayout(custom_strip_);
    ch->setContentsMargins(8, 4, 8, 4);
    ch->setSpacing(6);
    symbols_cap_ = new QLabel(custom_strip_);
    symbols_cap_->setObjectName(QStringLiteral("algoFieldLabel"));
    symbols_ = new ui::algo::SymbolChipInput(custom_strip_);
    ch->addWidget(symbols_cap_, 0, Qt::AlignTop);
    ch->addWidget(symbols_, 1);
    custom_strip_->setVisible(false);
    root->addWidget(custom_strip_);

    // ── Panels ──
    auto* split = kit::make_splitter(Qt::Horizontal, this);
    kit::Panel matches = kit::make_panel(QString(), split, 0);
    matches_header_ = matches.header;
    stack_ = new QStackedWidget(matches.body_widget);
    table_ = new QTableView(stack_);
    model_ = new ui::LiveTableModel<Row>(table_);
    model_->set_key([](const Row& r) { return QString::number(r.seq); });
    model_->set_cell([](const Row& r, int col, int role) -> QVariant {
        using Model = ui::LiveTableModel<Row>;
        if (role == Qt::TextAlignmentRole)
            return col == kUvPrice ? Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter).toInt()
                                   : Qt::Alignment(Qt::AlignLeft | Qt::AlignVCenter).toInt();
        if (role == Qt::DisplayRole) {
            switch (col) {
                case kUvTime:
                    return QDateTime::fromMSecsSinceEpoch(r.time_ms).toString(QStringLiteral("HH:mm:ss"));
                case kUvSymbol:
                    return r.symbol;
                case kUvSignal:
                    return r.direction > 0 ? uv_tr("▲ BULLISH") : (r.direction < 0 ? uv_tr("▼ BEARISH") : uv_tr("MATCH"));
                case kUvPrice:
                    return std::isfinite(r.price) && r.price > 0
                               ? fmt::price_with_symbol(r.price, fmt::instrument_for_symbol(r.symbol, r.currency))
                               : fmt::placeholder();
                case kUvDetail:
                    return r.detail;
                case kUvDeploy:
                    return uv_tr("DEPLOY…");
                default:
                    break;
            }
            return {};
        }
        if (role == Model::SortRole) {
            switch (col) {
                case kUvTime:
                    return r.time_ms;
                case kUvSymbol:
                    return r.symbol;
                case kUvSignal:
                    return r.direction;
                case kUvPrice:
                    // One listing per scan, so one currency: the raw price sorts.
                    return std::isfinite(r.price) ? QVariant(r.price) : QVariant();
                case kUvDetail:
                    return r.detail;
                default:
                    break;
            }
            return {};
        }
        if (role == Qt::ForegroundRole) {
            if (col == kUvSignal) // bullish / bearish is price direction (with its arrow)
                return kit::tone_color(r.direction > 0 ? kit::Tone::Pos
                                                       : (r.direction < 0 ? kit::Tone::Neg : kit::Tone::Dim));
            if (col == kUvDeploy)
                return kit::tone_color(kit::Tone::Accent);
            if (col == kUvTime)
                return kit::tone_color(kit::Tone::Dim);
            return {};
        }
        if (role == Qt::ToolTipRole) {
            if (col == kUvDeploy)
                return uv_tr("Open the deploy dialog for %1 (paper by default; nothing starts until you confirm)")
                    .arg(r.symbol);
            if (col == kUvDetail)
                return r.detail;
            return {};
        }
        return {};
    });
    proxy_ = new ui::KeySortProxy(table_);
    proxy_->setSourceModel(model_);
    proxy_->setSortRole(ui::LiveTableModel<Row>::SortRole);
    table_->setModel(proxy_);
    kit::setup_table(table_, QStringLiteral("universe_matches"));
    kit::set_column_chars(table_, {9, 12, 11, 13, 40, 10});
    table_->sortByColumn(kUvTime, Qt::DescendingOrder);
    stack_->addWidget(table_);
    connect(table_, &QTableView::clicked, this, [this](const QModelIndex& idx) {
        if (idx.column() == kUvDeploy) {
            const QString s = selected_symbol();
            if (!s.isEmpty())
                emit deploy_requested(s);
        }
    });
    connect(table_, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        if (idx.column() == kUvDeploy)
            return; // the single click above already asked
        const QString s = selected_symbol();
        if (!s.isEmpty())
            emit deploy_requested(s);
    });
    auto* enter = new QShortcut(QKeySequence(Qt::Key_Return), table_);
    enter->setContext(Qt::WidgetShortcut);
    connect(enter, &QShortcut::activated, this, [this]() {
        const QString s = selected_symbol();
        if (!s.isEmpty())
            emit deploy_requested(s);
    });
    connect(table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this]() { bar_->set_action_enabled(QStringLiteral("deploy"), !selected_symbol().isEmpty()); });

    state_ = new ui::EmptyState(stack_);
    connect(state_, &ui::EmptyState::primary_clicked, this, &UniverseView::state_action);
    stack_->addWidget(state_);
    matches.body->addWidget(stack_, 1);

    kit::Panel strat = kit::make_panel(QString(), split, 0);
    strategy_header_ = strat.header;
    auto* scroll = new QScrollArea(strat.body_widget);
    scroll->setObjectName(QStringLiteral("algoScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget(scroll);
    auto* bv = new QVBoxLayout(body);
    bv->setContentsMargins(10, 8, 10, 10);
    bv->setSpacing(6);
    s_name_ = new QLabel(body);
    s_name_->setObjectName(QStringLiteral("algoValue"));
    s_name_->setWordWrap(true);
    bv->addWidget(s_name_);
    s_meta_ = kit::make_note(QString(), body);
    bv->addWidget(s_meta_);
    auto* rule = new QFrame(body);
    rule->setObjectName(QStringLiteral("algoHairline"));
    bv->addWidget(rule);
    s_entry_cap_ = new QLabel(body);
    s_entry_cap_->setObjectName(QStringLiteral("algoFieldLabel"));
    bv->addWidget(s_entry_cap_);
    s_entry_ = new QLabel(body);
    s_entry_->setObjectName(QStringLiteral("algoSentence"));
    s_entry_->setWordWrap(true);
    s_entry_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bv->addWidget(s_entry_);
    s_note_ = kit::make_note(QString(), body);
    bv->addWidget(s_note_);
    bv->addStretch(1);
    scroll->setWidget(body);
    strat.body->addWidget(scroll, 1);

    split->addWidget(matches.frame);
    split->addWidget(strat.frame);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    root->addWidget(split, 1);

    status_ = kit::make_status_bar(this);
    root->addWidget(status_);

    update_counts();
    stack_->setCurrentIndex(kUvPageState);
}

void UniverseView::retranslate() {
    bar_->set_action_text(QStringLiteral("start"), running_ ? uv_tr("STOP SCAN") : uv_tr("START SCAN"));
    bar_->set_action_tooltip(QStringLiteral("start"),
                             running_ ? uv_tr("Stop the live scan (its alerts so far are kept)")
                                      : uv_tr("Start a background watch on the broker's live feed: matches are listed "
                                              "and alerted — no order is placed"));
    bar_->set_action_text(QStringLiteral("deploy"), uv_tr("DEPLOY…"));
    bar_->set_action_tooltip(QStringLiteral("deploy"),
                             uv_tr("Open the deploy dialog for the selected match (Enter) — nothing starts until you "
                                   "confirm it"));
    bar_->set_action_text(QStringLiteral("clear"), uv_tr("CLEAR"));
    bar_->set_action_tooltip(QStringLiteral("clear"), uv_tr("Clear the match list (the scan keeps running)"));
    if (strategy_cap_)
        strategy_cap_->setText(uv_tr("STRATEGY"));
    if (universe_cap_)
        universe_cap_->setText(uv_tr("UNIVERSE"));
    if (account_cap_)
        account_cap_->setText(uv_tr("ACCOUNT"));
    if (sweep_cap_) {
        sweep_cap_->setText(uv_tr("SWEEP"));
        sweep_cap_->setToolTip(uv_tr("How often every symbol is evaluated"));
    }
    if (cooldown_cap_) {
        cooldown_cap_->setText(uv_tr("COOLDOWN"));
        cooldown_cap_->setToolTip(uv_tr("Quiet time per symbol after a match"));
    }
    symbols_cap_->setText(uv_tr("SYMBOLS"));
    kit::mark_field(strategy_, uv_tr("Strategy"), uv_tr("A saved equity strategy: its entry rules are what is scanned"));
    kit::mark_field(universe_, uv_tr("Universe"),
                    uv_tr("Every equity of an exchange your broker lists, a saved list, or custom symbols"));
    kit::mark_field(account_, uv_tr("Broker account"), uv_tr("The live feed the scan reads (required)"));
    kit::mark_field(sweep_, uv_tr("Evaluation sweep (milliseconds)"), uv_tr("Evaluation sweep, in milliseconds"));
    sweep_->setSuffix(uv_tr(" ms"));
    kit::mark_field(cooldown_, uv_tr("Cooldown (minutes)"), uv_tr("Cooldown per symbol after a match, in minutes"));
    cooldown_->setSuffix(uv_tr(" min"));
    matches_header_->set_title(uv_tr("Live matches"));
    strategy_header_->set_title(uv_tr("What runs"));
    s_entry_cap_->setText(uv_tr("ENTRY RULES SCANNED"));
    model_->set_headers({uv_tr("TIME"), uv_tr("SYMBOL"), uv_tr("SIGNAL"), uv_tr("PRICE"), uv_tr("MATCHED RULES"),
                         QString()});
    status_->set_text(QStringLiteral("data"), uv_tr("FEED · broker live"));
}

void UniverseView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void UniverseView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    status_->start_clock();
}

void UniverseView::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    status_->stop_clock();
}

void UniverseView::set_custom_visible(bool visible) {
    custom_strip_->setVisible(visible);
}

void UniverseView::set_running(bool running) {
    running_ = running;
    bar_->set_action_role(QStringLiteral("start"),
                          running ? ui::ScreenCommandBar::Role::Danger : ui::ScreenCommandBar::Role::Primary);
    retranslate();
    kit::set_state(status_, running ? kit::RunState::Running : kit::RunState::Idle,
                   running ? uv_tr("SCANNING") : uv_tr("STOPPED"));
    for (QWidget* w : std::initializer_list<QWidget*>{strategy_, universe_, account_, sweep_, cooldown_, symbols_})
        w->setEnabled(!running); // what runs is fixed while it runs
}

void UniverseView::set_status(const QString& text, kit::Tone tone) {
    using SB = ui::ScreenStatusBar;
    SB::Tone t = SB::Tone::Normal;
    switch (tone) {
        case kit::Tone::Danger:
            t = SB::Tone::Danger;
            break;
        case kit::Tone::Warn:
            t = SB::Tone::Warn;
            break;
        case kit::Tone::Ok:
            t = SB::Tone::Ok;
            break;
        case kit::Tone::Accent:
            t = SB::Tone::Accent;
            break;
        case kit::Tone::Dim:
            t = SB::Tone::Dim;
            break;
        default:
            break;
    }
    status_->set_item(QStringLiteral("info"), text, t);
    status_->set_tooltip(QStringLiteral("info"), text);
    if (tone == kit::Tone::Danger)
        status_->show_notice(text, QString(), {}, 9000, SB::Tone::Danger);
}

void UniverseView::set_strategy_summary(const QString& name, const QString& entry_sentence, const QString& meta) {
    s_name_->setText(name.isEmpty() ? uv_tr("No strategy selected") : name);
    s_meta_->setText(meta);
    s_entry_->setText(entry_sentence);
    s_entry_cap_->setVisible(!entry_sentence.isEmpty());
    s_entry_->setVisible(!entry_sentence.isEmpty());
    s_note_->setText(uv_tr("A match is alerted (toast) and listed here; DEPLOY… on a match opens the deploy dialog "
                           "for that symbol."));
    strategy_header_->set_badge(name);
}

// ── Matches ─────────────────────────────────────────────────────────────────

void UniverseView::add_match(qint64 time_ms, const QString& symbol, int direction, double price,
                             const QString& currency, const QString& detail) {
    Row r;
    r.seq = ++next_seq_;
    r.time_ms = time_ms;
    r.symbol = symbol;
    r.direction = direction;
    r.price = price;
    r.currency = currency;
    r.detail = detail;
    rows_.prepend(r);
    while (rows_.size() > kUvMaxMatchRows)
        rows_.removeLast();
    // Keep the selected match selected across the model refresh.
    QString keep;
    if (table_->selectionModel() && table_->selectionModel()->currentIndex().isValid())
        keep = proxy_->mapToSource(table_->selectionModel()->currentIndex())
                   .siblingAtColumn(0)
                   .data(ui::LiveTableModel<Row>::KeyRole)
                   .toString();
    model_->upsert(rows_);
    if (!keep.isEmpty()) {
        const int src = model_->index_of_key(keep);
        if (src >= 0)
            table_->selectionModel()->setCurrentIndex(proxy_->mapFromSource(model_->index(src, 0)),
                                                      QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    stack_->setCurrentIndex(kUvPageTable);
    update_counts();
    status_->set_text(QStringLiteral("asof"),
                      uv_tr("last match %1").arg(QDateTime::fromMSecsSinceEpoch(time_ms).toString(QStringLiteral("HH:mm:ss"))));
}

void UniverseView::clear_matches() {
    rows_.clear();
    model_->upsert(rows_);
    update_counts();
    status_->set_text(QStringLiteral("asof"), QString());
}

int UniverseView::match_count() const {
    return static_cast<int>(rows_.size());
}

void UniverseView::refresh_text() {
    // The cell texts are formatted on read: a reset re-reads them all.
    QVector<Row> copy = rows_;
    model_->clear();
    model_->upsert(copy);
}

void UniverseView::show_state(const ui::EmptyState::Spec& spec) {
    state_->apply(spec);
    stack_->setCurrentIndex(kUvPageState);
}

void UniverseView::update_counts() {
    const int n = match_count();
    matches_header_->set_badge(n > 0 ? QString::number(n) : QString());
    bar_->set_action_enabled(QStringLiteral("clear"), n > 0);
    bar_->set_action_enabled(QStringLiteral("deploy"), !selected_symbol().isEmpty());
    status_->set_text(QStringLiteral("info"), n > 0 ? uv_tr("%n match(es)", nullptr, n) : QString());
}

QString UniverseView::selected_symbol() const {
    if (!table_ || !table_->selectionModel())
        return {};
    const QModelIndex cur = table_->selectionModel()->currentIndex();
    if (!cur.isValid())
        return {};
    return proxy_->mapToSource(cur).siblingAtColumn(kUvSymbol).data(Qt::DisplayRole).toString();
}

} // namespace fincept::screens
