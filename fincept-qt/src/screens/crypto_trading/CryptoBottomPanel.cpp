// CryptoBottomPanel.cpp — production trading-terminal bottom panel.
//
// Tabs: POS · ORD · HIST · MY TRADES · FEES · T&S · DEPTH · MKT · STATS
//
// Design goals:
//  • Fully responsive across desktop/laptop/HiDPI: column resize modes are
//    explicit (Stretch / ResizeToContents / Interactive / Fixed) so tables
//    expand into available width without horizontal scrollbars on common
//    layouts and don't collapse below readable widths on narrow ones.
//  • Row heights derive from QFontMetrics so they scale with the user's
//    terminal font setting instead of being pinned to a hard-coded pixel.
//  • Empty states: each table-tab is wrapped in a QStackedWidget that
//    flips to a centered placeholder when the data set is empty — no more
//    "header-only" tables on relaunch before the first WS tick lands.
//  • MKT / STATS use a 3-column card grid (responsive via QGridLayout)
//    instead of a row stack of labels, so they read like a Bloomberg-style
//    dashboard rather than a ledger.

#include "screens/crypto_trading/CryptoBottomPanel.h"

#include "screens/crypto_trading/CryptoDepthChart.h"
#include "screens/crypto_trading/CryptoTimeSales.h"
#include "services/markets/FxRateService.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/MnemonicText.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QLocale>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimeZone>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::crypto {

using namespace fincept::ui;

namespace {

inline QColor kRowEven() {
    return QColor(colors::BG_BASE());
}
inline QColor kRowOdd() {
    return QColor(colors::ROW_ALT());
}
inline QColor kColorPos() {
    return QColor(colors::POSITIVE());
}
inline QColor kColorNeg() {
    return QColor(colors::NEGATIVE());
}
inline QColor kColorSec() {
    return QColor(colors::TEXT_SECONDARY());
}
inline QColor kColorTert() {
    return QColor(colors::TEXT_TERTIARY());
}

// Per-column resize policy.
//  Stretch:     fills remaining horizontal space (use for the "primary" column
//               like Symbol — at least one per table or the last col stretches
//               by default).
//  Numeric:     Interactive with a width derived from font metrics (e.g.
//               "1234567890.12" worth of digits). User-resizable.
//  Compact:     ResizeToContents — best for short categorical text like
//               "BUY"/"LONG"/"FILLED".
//  Action:      Fixed pixel width — for cancel/close buttons.
enum class ColMode { Stretch, Numeric, Compact, Action };

struct ColSpec {
    QString header;
    ColMode mode = ColMode::Stretch;
    int width_chars = 8; // hint for Numeric mode
    int fixed_px = 32;   // for Action mode
    Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter;
};

QTableWidget* make_table(const QList<ColSpec>& cols) {
    auto* t = new QTableWidget;
    t->setObjectName("cryptoBottomTable");
    t->setColumnCount(cols.size());

    QStringList headers;
    headers.reserve(cols.size());
    for (const auto& c : cols)
        headers << c.header;
    t->setHorizontalHeaderLabels(headers);

    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->setAlternatingRowColors(false); // we paint zebra stripes ourselves
    t->setShowGrid(false);
    t->setWordWrap(false);
    t->setTextElideMode(Qt::ElideRight);
    t->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    t->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    auto* vh = t->verticalHeader();
    vh->hide();
    // Row height scales with the active font — important for HiDPI and for
    // users who bump the terminal font size in Settings → Appearance.
    vh->setDefaultSectionSize(table_row_height(0, 22));

    auto* hh = t->horizontalHeader();
    // Header click sorts. No initial sort column (-1): rows keep the order they
    // arrive in (newest fill first, …) until the user picks a column. Setters
    // turn sorting off while they write rows by index and restore it after.
    hh->setSectionsClickable(true);
    hh->setSortIndicator(-1, Qt::AscendingOrder);
    t->setSortingEnabled(true);
    hh->setHighlightSections(false);
    hh->setStretchLastSection(false);
    // Don't let any column collapse below ~48px or text becomes ellipsis-only.
    hh->setMinimumSectionSize(48);
    hh->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    const QFontMetrics fm(hh->font());
    bool any_stretch = false;
    for (int i = 0; i < cols.size(); ++i) {
        const auto& c = cols[i];
        switch (c.mode) {
            case ColMode::Stretch:
                hh->setSectionResizeMode(i, QHeaderView::Stretch);
                any_stretch = true;
                break;
            case ColMode::Numeric: {
                hh->setSectionResizeMode(i, QHeaderView::Interactive);
                // Width: max of header text and `width_chars` worth of '0'.
                const int header_w = fm.horizontalAdvance(c.header) + 24;
                const int data_w = fm.horizontalAdvance(QString(c.width_chars, QLatin1Char('0'))) + 16;
                t->setColumnWidth(i, std::max(header_w, data_w));
                break;
            }
            case ColMode::Compact: {
                // A fixed-from-font-metrics width, not ResizeToContents: that mode
                // re-measures every row of the column on each fill, a GUI-thread
                // stall on a busy blotter. Short categorical text fits 9 chars.
                hh->setSectionResizeMode(i, QHeaderView::Interactive);
                const int header_w = fm.horizontalAdvance(c.header) + 24;
                const int data_w = fm.horizontalAdvance(QStringLiteral("CANCELLED")) + 16;
                t->setColumnWidth(i, std::max(header_w, data_w));
                break;
            }
            case ColMode::Action:
                hh->setSectionResizeMode(i, QHeaderView::Fixed);
                t->setColumnWidth(i, c.fixed_px);
                break;
        }
    }
    // Fail-safe: if the caller forgot to mark a column as Stretch, stretch
    // the last numeric/compact column to fill remaining space.
    if (!any_stretch)
        hh->setStretchLastSection(true);

    return t;
}

// Pages of every table stack: the rows, the designed empty / unavailable state,
// and a table skeleton while the first rows of the current mode load.
constexpr int kCbpPageTable = 0;
constexpr int kCbpPageEmpty = 1;
constexpr int kCbpPageLoading = 2;

// What an EmptyState's buttons do (stored on it as dynamic properties, so one
// connection per state serves every wording it is given).
enum CbpAction : int {
    kCbpNone = 0,
    kCbpNewOrder,    ///< focus the order ticket
    kCbpRetryLive,   ///< fetch the LIVE account views again
    kCbpAddKey,      ///< open the API key dialog
    kCbpRetryKeys,   ///< read the stored API keys again
    kCbpRetryFees,   ///< fetch the fee schedule again
    kCbpRetryPaper,  ///< load the paper account again
    kCbpShowHistory, ///< switch to the HISTORY tab (paper fills)
};
constexpr const char* kCbpPrimaryProp = "cbpPrimaryAction";
constexpr const char* kCbpSecondaryProp = "cbpSecondaryAction";

void cbp_set_actions(ui::EmptyState* es, int primary, int secondary = kCbpNone) {
    es->setProperty(kCbpPrimaryProp, primary);
    es->setProperty(kCbpSecondaryProp, secondary);
}

/// Show `s` (with its actions) - re-applied only when its words change: the
/// paper positions view is re-set on every price tick and must stay cheap.
void cbp_apply(ui::EmptyState* es, const ui::EmptyState::Spec& s, int primary, int secondary = kCbpNone) {
    const QChar sep(0x1f);
    const QString sig = s.title + sep + s.message + sep + s.primary + sep + s.secondary + sep +
                        QString::number(static_cast<int>(s.glyph_tone)) + sep + s.primary_tooltip + sep +
                        s.secondary_tooltip;
    if (es->property("cbpSpecSig").toString() != sig) {
        es->apply(s);
        es->setProperty("cbpSpecSig", sig);
    }
    cbp_set_actions(es, primary, secondary);
}

/// `table` in a stack with its designed empty state (page 1) and a table
/// skeleton (page 2); starts on the skeleton when `start_loading`.
QWidget* cbp_wrap_states(QTableWidget* table, QStackedWidget*& out_stack, ui::EmptyState*& out_empty,
                         const QString& loading_text, bool start_loading) {
    out_stack = new QStackedWidget;
    out_stack->setObjectName("cryptoBottomStack");
    out_stack->addWidget(table); // kCbpPageTable

    out_empty = new ui::EmptyState(out_stack); // kCbpPageEmpty
    out_empty->set_compact(true);
    out_stack->addWidget(out_empty);

    auto* skeleton = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, out_stack); // kCbpPageLoading
    skeleton->set_row_count(4);
    skeleton->set_message(loading_text);
    out_stack->addWidget(skeleton);

    out_stack->setCurrentIndex(start_loading ? kCbpPageLoading : kCbpPageEmpty);
    return out_stack;
}

} // namespace

QTableWidgetItem* CryptoBottomPanel::ensure_item(QTableWidget* table, int row, int col) {
    auto* it = table->item(row, col);
    if (!it) {
        it = new QTableWidgetItem;
        table->setItem(row, col, it);
    }
    return it;
}

namespace {
/// Role under which the positions table stashes the exact (unrounded) numeric
/// value of a cell. `update_position_prices()` used to recover quantity and
/// entry price by re-parsing the *displayed* strings, which are rounded for
/// presentation — an entry of 1.005 rendered "1.00" and the recomputed
/// unrealised P&L was wrong by 0.005 × qty, while any sub-cent entry rendered
/// "0.00" and the row was skipped entirely.
constexpr int kExactValueRole = Qt::UserRole + 1;
/// Role on a positions row's symbol cell: the currency its P&L is in (a paper
/// position: the pair's quote asset — the engine marks unrealized P&L in price
/// units; a live contract: its settle asset, as the exchange reports it).
constexpr int kPnlCcyRole = Qt::UserRole + 2;
/// Role on a live positions row's quantity cell: the contract size (base units
/// per contract), so a mark-to-market patch values contracts correctly.
constexpr int kContractSizeRole = Qt::UserRole + 3;

namespace fmt = fincept::ui::formatting;

/// The currencies of one table column, for money_in_column() / header().
fmt::CurrencyMix cbp_mix_of(const QStringList& ccys) {
    fmt::CurrencyMix mix;
    for (const QString& c : ccys)
        mix.add(c);
    return mix;
}

/// Gain / loss / flat colour (a zero P&L is neither green nor red).
QColor pnl_color(double v) {
    return v > 0 ? kColorPos() : (v < 0 ? kColorNeg() : kColorSec());
}

/// Suspends header sorting while a setter writes rows by index: with sorting
/// on, every setItem / setText on the sort column could move the row mid-write.
/// Restoring it re-sorts by the user's chosen column (none = arrival order).
class SortSuspend {
  public:
    explicit SortSuspend(QTableWidget* t) : t_(t), was_(t && t->isSortingEnabled()) {
        if (t_ && was_)
            t_->setSortingEnabled(false);
    }
    ~SortSuspend() {
        if (t_ && was_)
            t_->setSortingEnabled(true);
    }
    SortSuspend(const SortSuspend&) = delete;
    SortSuspend& operator=(const SortSuspend&) = delete;

  private:
    QTableWidget* t_;
    bool was_;
};

/// A numeric cell: formatted `text`, sorted by `value`. NumericTableWidgetItem
/// fixes its sort value at construction, so a changed value swaps the item,
/// carrying over the old one's brushes and alignment.
QTableWidgetItem* put_num(QTableWidget* t, int row, int col, double value, const QString& text) {
    QTableWidgetItem* old = t->item(row, col);
    if (auto* cur = dynamic_cast<NumericTableWidgetItem*>(old); cur && cur->numeric_value() == value) {
        if (cur->text() != text)
            cur->setText(text);
        return cur;
    }
    auto* it = new NumericTableWidgetItem(text, value);
    if (old) {
        it->setForeground(old->foreground());
        it->setBackground(old->background());
        it->setTextAlignment(Qt::Alignment(old->textAlignment()));
    }
    t->setItem(row, col, it); // deletes `old`
    return it;
}

void style_item(QTableWidgetItem* it, const QColor& fg, const QColor& bg, Qt::Alignment align) {
    if (fg.isValid() && (it->foreground().style() == Qt::NoBrush || it->foreground().color() != fg))
        it->setForeground(fg);
    if (it->background().color() != bg)
        it->setBackground(bg);
    if (Qt::Alignment(it->textAlignment()) != align)
        it->setTextAlignment(align);
}

constexpr Qt::Alignment kRight = Qt::AlignRight | Qt::AlignVCenter;

/// A stored fill time (ISO 8601; a naive stamp is UTC, as the paper engine
/// writes it) in the display zone with seconds, and its epoch ms for sorting.
/// Text that does not parse is shown unchanged (sort key 0), never as a guess.
QString fill_time_text(const QString& raw, qint64* epoch_ms) {
    QDateTime dt = QDateTime::fromString(raw, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(raw, Qt::ISODate);
    if (!dt.isValid()) {
        *epoch_ms = 0;
        return raw;
    }
    if (dt.timeSpec() == Qt::LocalTime)
        dt.setTimeZone(QTimeZone::UTC);
    *epoch_ms = dt.toMSecsSinceEpoch();
    return fmt::date_time(dt, fmt::DateStyle::Short, true);
}

/// Unrealised P&L as a percent of the margin posted; NaN when there is no margin.
double margin_pnl_pct(double pnl, double margin) {
    return margin > 0.0 ? pnl / margin * 100.0 : std::nan("");
}
} // namespace

void CryptoBottomPanel::update_empty_state(QTableWidget* /*table*/, QStackedWidget* stack, int row_count) {
    if (!stack)
        return;
    const int target = (row_count > 0) ? kCbpPageTable : kCbpPageEmpty;
    if (stack->currentIndex() != target)
        stack->setCurrentIndex(target);
}

void CryptoBottomPanel::show_table_loading(QStackedWidget* stack) {
    if (stack && stack->currentIndex() != kCbpPageLoading)
        stack->setCurrentIndex(kCbpPageLoading);
}

CryptoBottomPanel::CryptoBottomPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("cryptoBottomPanel");
    setMinimumHeight(180); // keep at least a couple of rows visible on resize

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Created in place and laid out before its tabs are built: adding a finished
    // tab widget to the panel re-computed the stylesheet of every table in it.
    tabs_ = new QTabWidget(this);
    layout->addWidget(tabs_);
    tabs_->setObjectName("cryptoBottomTabs");
    tabs_->setDocumentMode(true);
    tabs_->tabBar()->setExpanding(false);
    tabs_->tabBar()->setUsesScrollButtons(true);
    tabs_->setUsesScrollButtons(true);
    // Nine tabs do not fit at 1280 px wide (STATS was cut off): let the labels
    // shorten ("POSIT…") before the bar has to scroll, so every tab stays reachable.
    tabs_->tabBar()->setElideMode(Qt::ElideRight);
    tabs_->setElideMode(Qt::ElideRight);

    setup_positions_tab();
    setup_orders_tab();
    setup_trades_tab();
    setup_my_trades_tab();
    setup_fees_tab();

    // Time & Sales
    time_sales_ = new CryptoTimeSales;
    time_sales_tab_idx_ = tabs_->addTab(time_sales_, fincept::ui::literal_ampersands(tr("T&S")));

    // Depth Chart
    depth_chart_ = new CryptoDepthChart;
    depth_tab_idx_ = tabs_->addTab(depth_chart_, tr("DEPTH"));

    setup_market_info_tab();
    setup_stats_tab();

    // One connection per designed state; the action it runs is stored on it.
    for (ui::EmptyState* es : {positions_empty_, orders_empty_, trades_empty_, my_trades_empty_, fees_empty_,
                               stats_empty_}) {
        if (!es)
            continue;
        connect(es, &ui::EmptyState::primary_clicked, this, [this, es]() { on_empty_action(es, true); });
        connect(es, &ui::EmptyState::secondary_clicked, this, [this, es]() { on_empty_action(es, false); });
    }
    apply_all_empty_states();
    // FEES / MARKET load the pair's fee schedule (and MARKET its funding / open
    // interest) when they are opened — the public schedule needs no key.
    connect(tabs_, &QTabWidget::currentChanged, this, &CryptoBottomPanel::on_tab_changed);

    // Item brushes keep the colours they were given: carry them over to the new
    // appearance. Row heights follow the font — re-fitted once the appearance has
    // settled (a layout pass inside theme_changed stalls the GUI thread).
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this](const ThemeTokens&) {
        for (QTableWidget* t : {positions_table_, orders_table_, trades_table_, my_trades_table_, fees_table_}) {
            if (t)
                retheme_items(t);
        }
    });
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() {
        for (QTableWidget* t : {positions_table_, orders_table_, trades_table_, my_trades_table_, fees_table_}) {
            if (t)
                t->verticalHeader()->setDefaultSectionSize(table_row_height(0, 22));
        }
    });

    // A currency / number-format change re-renders the rows, tiles and cards
    // from the data already held (no fetch).
    fincept::ui::formatting::on_format_changed(this, [this]() { rerender_from_cache(); });
}

void CryptoBottomPanel::set_price_market(const QString& market) {
    if (time_sales_)
        time_sales_->set_price_market(market);
    if (depth_chart_)
        depth_chart_->set_price_market(market);
}

void CryptoBottomPanel::update_positions_pnl_header() {
    if (!positions_table_)
        return;
    // One currency in the column: "P&L (USDT)". Mixed: no unit in the header,
    // the code on every cell (money_in_column).
    const fmt::CurrencyMix mix = cbp_mix_of(positions_ccys_);
    if (auto* h = positions_table_->horizontalHeaderItem(5)) {
        h->setText(mix.isEmpty() ? tr("P&L") : fmt::header(tr("P&L"), mix));
        h->setToolTip(tr("Unrealised P&L in each position's own currency: a paper position in its pair's quote "
                         "asset, a live contract in its settlement asset. Not converted or added up."));
    }
}

void CryptoBottomPanel::rerender_from_cache() {
    // Tables: only when they hold rows (an empty table may be showing an
    // "unavailable: …" reason that must stay).
    if (positions_live_ ? !live_positions_.isEmpty() : !paper_positions_.isEmpty()) {
        if (positions_live_)
            set_live_positions(QJsonArray(live_positions_));
        else
            set_positions(QVector<trading::PtPosition>(paper_positions_));
        if (!last_marks_.isEmpty())
            update_position_prices(QHash<QString, double>(last_marks_));
    }
    if (orders_live_ ? !live_orders_.isEmpty() : !paper_orders_.isEmpty()) {
        if (orders_live_)
            set_live_orders(QJsonArray(live_orders_));
        else
            set_orders(QVector<trading::PtOrder>(paper_orders_));
    }
    if (!paper_trades_.isEmpty())
        set_trades(QVector<trading::PtTrade>(paper_trades_));
    if (has_stats_)
        render_stats();
    render_market_info();
    if (has_my_trades_ && !my_trades_json_.contains("error") && my_trades_json_.contains("trades"))
        update_my_trades(QJsonObject(my_trades_json_));
    if (has_fees_ && !fees_json_.contains("error"))
        update_fees(QJsonObject(fees_json_));
    if (has_live_balance_)
        render_live_balance();
}

void CryptoBottomPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void CryptoBottomPanel::retranslateUi() {
    // Tab labels
    if (tabs_) {
        if (positions_tab_idx_ >= 0)
            tabs_->setTabText(positions_tab_idx_, tr("POSITIONS"));
        if (orders_tab_idx_ >= 0)
            tabs_->setTabText(orders_tab_idx_, tr("ORDERS"));
        if (trades_tab_idx_ >= 0)
            tabs_->setTabText(trades_tab_idx_, tr("HISTORY"));
        if (my_trades_tab_idx_ >= 0)
            tabs_->setTabText(my_trades_tab_idx_, tr("MY TRADES"));
        if (fees_tab_idx_ >= 0)
            tabs_->setTabText(fees_tab_idx_, tr("FEES"));
        if (time_sales_tab_idx_ >= 0)
            tabs_->setTabText(time_sales_tab_idx_, fincept::ui::literal_ampersands(tr("T&S")));
        if (depth_tab_idx_ >= 0)
            tabs_->setTabText(depth_tab_idx_, tr("DEPTH"));
        if (market_tab_idx_ >= 0)
            tabs_->setTabText(market_tab_idx_, tr("MARKET"));
        if (stats_tab_idx_ >= 0)
            tabs_->setTabText(stats_tab_idx_, tr("STATS"));
    }

    // Table headers
    if (positions_table_) {
        positions_table_->setHorizontalHeaderLabels(
            {tr("Symbol"), tr("Side"), tr("Qty"), tr("Entry"), tr("Mark"), tr("P&L"), tr("P&L %"), tr("Lev")});
        if (auto* pct_hdr = positions_table_->horizontalHeaderItem(6))
            pct_hdr->setToolTip(tr("Unrealised P&L as a percent of the margin posted (entry × qty ÷ leverage)"));
        update_positions_pnl_header();
    }
    if (orders_table_)
        orders_table_->setHorizontalHeaderLabels(
            {tr("Symbol"), tr("Side"), tr("Type"), tr("Qty"), tr("Price"), tr("Status"), QString()});
    if (trades_table_)
        trades_table_->setHorizontalHeaderLabels(
            {tr("Symbol"), tr("Side"), tr("Price"), tr("Qty"), tr("Fee"), tr("P&L"), tr("Time")});
    if (my_trades_table_)
        my_trades_table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Side"), tr("Price"), tr("Amount"), tr("Cost"),
                                                     tr("Fee"), tr("Fee ccy"), tr("Time")});
    if (fees_table_)
        fees_table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Maker %"), tr("Taker %")});

    // Designed empty / unavailable states
    apply_all_empty_states();

    // Bulk action buttons
    update_bulk_labels();

    // Market Info tile titles (and their "n/a" reasons)
    if (market_strip_) {
        const QList<QPair<const char*, QString>> titles = {
            {"funding", tr("FUNDING RATE")}, {"mark", tr("MARK PRICE")},     {"index", tr("INDEX PRICE")},
            {"oi", tr("OPEN INTEREST")},     {"fees", tr("MAKER / TAKER")}, {"next", tr("NEXT FUNDING")}};
        for (const auto& t : titles) {
            if (auto* tile = market_strip_->tile(QString::fromLatin1(t.first)))
                tile->set_label(t.second);
        }
        render_market_info();
    }

    // Stats + live tile titles (PAPER-prefixed while live) + HISTORY tab visibility
    apply_mode_labels();
}

// ── Designed states ─────────────────────────────────────────────────────────

void CryptoBottomPanel::apply_live_unavailable(ui::EmptyState* es, const QString& what, const QString& reason) {
    if (!es)
        return;
    const QString ex = live_exchange_.isEmpty() ? tr("the exchange") : live_exchange_;
    if (!live_read_error_.isEmpty()) {
        // The keys exist but secure storage could not be read: never ask for them again.
        ui::EmptyState::Spec s = ui::EmptyState::error_spec(
            tr("%1 Your %2 API keys may already be saved — nothing needs to be entered again.")
                .arg(live_read_error_, ex),
            tr("Stored API keys unreadable"), tr("RETRY"));
        s.primary_tooltip = tr("Read the stored %1 API keys again").arg(ex);
        cbp_apply(es, s, kCbpRetryKeys);
        return;
    }
    if (!live_has_keys_) {
        ui::EmptyState::Spec s = ui::EmptyState::no_data_spec(
            tr("No API key for %1").arg(ex),
            tr("Add an API key for %1 to load your live %2 from the exchange. Adding a key places or changes "
               "nothing.")
                .arg(ex, what),
            tr("ADD API KEY"));
        s.glyph_tone = ui::EmptyState::Tone::Warning;
        cbp_apply(es, s, kCbpAddKey);
        return;
    }
    ui::EmptyState::Spec s = ui::EmptyState::error_spec(
        tr("%1 did not return your live %2: %3").arg(ex, what, reason.isEmpty() ? tr("no reply") : reason),
        tr("Live %1 unavailable").arg(what), tr("RETRY"));
    s.secondary = tr("API KEY");
    s.secondary_tooltip = tr("Check the %1 API key").arg(ex);
    cbp_apply(es, s, kCbpRetryLive, kCbpAddKey);
}

void CryptoBottomPanel::apply_positions_empty() {
    if (!positions_empty_)
        return;
    if (!is_paper_ && !positions_reason_.isEmpty()) {
        apply_live_unavailable(positions_empty_, tr("positions"), positions_reason_);
        return;
    }
    if (is_paper_ && !paper_error_.isEmpty()) {
        cbp_apply(positions_empty_,
                  ui::EmptyState::error_spec(paper_error_, tr("Paper account not loaded"), tr("RETRY")),
                  kCbpRetryPaper);
        return;
    }
    const QString ex = live_exchange_.isEmpty() ? tr("the exchange") : live_exchange_;
    cbp_apply(positions_empty_,
              is_paper_ ? ui::EmptyState::no_data_spec(tr("No open paper positions"),
                                                       tr("Positions you open from the order ticket appear here, "
                                                          "marked to market on every tick."),
                                                       tr("NEW ORDER"))
                        : ui::EmptyState::no_data_spec(tr("No open positions on %1").arg(ex),
                                                       tr("Open positions of your %1 account appear here with "
                                                          "their live P&L.")
                                                           .arg(ex),
                                                       tr("NEW ORDER")),
              kCbpNewOrder);
}

void CryptoBottomPanel::apply_orders_empty() {
    if (!orders_empty_)
        return;
    if (!is_paper_ && !orders_reason_.isEmpty()) {
        apply_live_unavailable(orders_empty_, tr("orders"), orders_reason_);
        return;
    }
    if (is_paper_ && !paper_error_.isEmpty()) {
        cbp_apply(orders_empty_,
                  ui::EmptyState::error_spec(paper_error_, tr("Paper account not loaded"), tr("RETRY")),
                  kCbpRetryPaper);
        return;
    }
    const QString ex = live_exchange_.isEmpty() ? tr("the exchange") : live_exchange_;
    cbp_apply(orders_empty_,
              is_paper_ ? ui::EmptyState::no_data_spec(tr("No resting paper orders"),
                                                       tr("Limit and stop orders from the ticket wait here until "
                                                          "they fill or you cancel them."),
                                                       tr("NEW ORDER"))
                        : ui::EmptyState::no_data_spec(tr("No open orders on %1").arg(ex),
                                                       tr("Open orders of your %1 account for the market on screen "
                                                          "appear here; each can be cancelled from its row.")
                                                           .arg(ex),
                                                       tr("NEW ORDER")),
              kCbpNewOrder);
}

void CryptoBottomPanel::apply_trades_empty() {
    if (!trades_empty_)
        return;
    if (!paper_error_.isEmpty()) {
        cbp_apply(trades_empty_,
                  ui::EmptyState::error_spec(paper_error_, tr("Paper account not loaded"), tr("RETRY")),
                  kCbpRetryPaper);
        return;
    }
    cbp_apply(trades_empty_,
              ui::EmptyState::no_data_spec(tr("No paper trades yet"),
                                           tr("Every paper fill is listed here with its fee and realised P&L."),
                                           tr("NEW ORDER")),
              kCbpNewOrder);
}

void CryptoBottomPanel::apply_my_trades_empty() {
    if (!my_trades_empty_)
        return;
    const QString ex = live_exchange_.isEmpty() ? tr("the exchange") : live_exchange_;
    if (!is_paper_) {
        if (!my_trades_reason_.isEmpty() || !live_read_error_.isEmpty() || !live_has_keys_) {
            apply_live_unavailable(my_trades_empty_, tr("fills"), my_trades_reason_);
            return;
        }
        cbp_apply(my_trades_empty_,
                  ui::EmptyState::no_data_spec(tr("No fills on %1 yet").arg(ex),
                                               tr("Fills of your %1 account for the market on screen appear here.")
                                                   .arg(ex),
                                               tr("NEW ORDER")),
                  kCbpNewOrder);
        return;
    }
    // PAPER: what this tab is for, and what LIVE needs.
    if (!live_read_error_.isEmpty()) {
        ui::EmptyState::Spec s = ui::EmptyState::error_spec(
            tr("%1 Your %2 API keys may already be saved — nothing needs to be entered again.")
                .arg(live_read_error_, ex),
            tr("Stored API keys unreadable"), tr("RETRY"));
        s.primary_tooltip = tr("Read the stored %1 API keys again").arg(ex);
        s.secondary = tr("SHOW PAPER HISTORY");
        cbp_apply(my_trades_empty_, s, kCbpRetryKeys, kCbpShowHistory);
        return;
    }
    ui::EmptyState::Spec s = ui::EmptyState::no_data_spec(
        tr("Exchange fills appear in LIVE mode"),
        live_has_keys_ ? tr("Fills of your %1 account are listed here while the screen is in LIVE mode (the "
                            "PAPER / LIVE button). Paper fills are on HISTORY.")
                             .arg(ex)
                       : tr("Fills of your %1 account are listed here in LIVE mode once an API key is added. "
                            "Paper fills are on HISTORY.")
                             .arg(ex),
        live_has_keys_ ? tr("SHOW PAPER HISTORY") : tr("ADD API KEY"));
    s.glyph_tone = ui::EmptyState::Tone::Info;
    if (!live_has_keys_)
        s.secondary = tr("SHOW PAPER HISTORY");
    if (live_has_keys_)
        cbp_apply(my_trades_empty_, s, kCbpShowHistory);
    else
        cbp_apply(my_trades_empty_, s, kCbpAddKey, kCbpShowHistory);
}

void CryptoBottomPanel::apply_fees_empty() {
    if (!fees_empty_)
        return;
    const QString ex = live_exchange_.isEmpty() ? tr("the exchange") : live_exchange_;
    if (!fees_error_.isEmpty() && fees_unsupported_) {
        // Not a failure: the venue publishes no schedule for the pair without an account.
        // RETRY could never succeed; the account's own tier needs a key.
        ui::EmptyState::Spec s = ui::EmptyState::no_data_spec(
            tr("No published fee schedule"),
            tr("%1 publishes no fee schedule for this pair without an account (%2). With an API key in LIVE mode "
               "the fees of your account's tier are shown here.")
                .arg(ex, fees_error_),
            live_has_keys_ ? QString() : tr("ADD API KEY"));
        s.glyph_tone = ui::EmptyState::Tone::Info;
        cbp_apply(fees_empty_, s, kCbpAddKey);
        return;
    }
    if (!fees_error_.isEmpty()) {
        cbp_apply(fees_empty_,
                  ui::EmptyState::error_spec(
                      tr("%1 did not return a fee schedule for this pair: %2").arg(ex, fees_error_),
                      tr("Fees unavailable"), tr("RETRY")),
                  kCbpRetryFees);
        return;
    }
    ui::EmptyState::Spec s = ui::EmptyState::no_data_spec(
        tr("Fee schedule"),
        tr("Maker and taker fees for the pair on screen — the exchange's published schedule, or your account's "
           "tier in LIVE mode. They load when this tab opens."),
        tr("LOAD FEES"));
    s.glyph_tone = ui::EmptyState::Tone::Info;
    cbp_apply(fees_empty_, s, kCbpRetryFees);
}

void CryptoBottomPanel::apply_all_empty_states() {
    apply_positions_empty();
    apply_orders_empty();
    apply_trades_empty();
    apply_my_trades_empty();
    apply_fees_empty();
    if (stats_empty_) {
        cbp_apply(stats_empty_,
                  ui::EmptyState::no_data_spec(tr("No closed paper trades yet"),
                                               tr("P&L, win rate, profit factor and the other statistics fill in "
                                                  "after your first paper trade."),
                                               tr("NEW ORDER")),
                  kCbpNewOrder);
    }
}

void CryptoBottomPanel::on_empty_action(ui::EmptyState* es, bool primary) {
    if (!es)
        return;
    const int action = es->property(primary ? kCbpPrimaryProp : kCbpSecondaryProp).toInt();
    auto* stack = qobject_cast<QStackedWidget*>(es->parentWidget());
    switch (action) {
        case kCbpNewOrder:
            emit order_ticket_requested();
            break;
        case kCbpRetryLive:
            show_table_loading(stack); // the next reply fills it (rows, empty or the error again)
            emit live_refresh_requested();
            break;
        case kCbpAddKey:
            emit api_key_requested();
            break;
        case kCbpRetryKeys:
            emit credentials_retry_requested();
            break;
        case kCbpRetryFees:
            emit fees_wanted(true);
            break;
        case kCbpRetryPaper:
            paper_error_.clear();
            for (QStackedWidget* s : {positions_stack_, orders_stack_, trades_stack_})
                show_table_loading(s);
            emit paper_retry_requested();
            break;
        case kCbpShowHistory:
            if (tabs_ && trades_tab_idx_ >= 0 && tabs_->isTabVisible(trades_tab_idx_))
                tabs_->setCurrentIndex(trades_tab_idx_);
            break;
        default:
            break;
    }
}

void CryptoBottomPanel::set_live_key_state(const QString& exchange, bool has_keys, const QString& read_error) {
    if (exchange == live_exchange_ && has_keys == live_has_keys_ && read_error == live_read_error_)
        return;
    live_exchange_ = exchange;
    live_has_keys_ = has_keys;
    live_read_error_ = read_error;
    apply_all_empty_states();
}

void CryptoBottomPanel::set_paper_unavailable(const QString& reason) {
    paper_error_ = reason.isEmpty() ? tr("The paper account could not be opened.") : reason;
    if (!is_paper_)
        return; // shown when the screen is back in PAPER
    paper_positions_.clear();
    paper_orders_.clear();
    paper_trades_.clear();
    for (QTableWidget* t : {positions_table_, orders_table_, trades_table_}) {
        if (t)
            t->setRowCount(0);
    }
    apply_positions_empty();
    apply_orders_empty();
    apply_trades_empty();
    update_empty_state(positions_table_, positions_stack_, 0);
    update_empty_state(orders_table_, orders_stack_, 0);
    update_empty_state(trades_table_, trades_stack_, 0);
}

void CryptoBottomPanel::set_fees_loading() {
    fees_loading_ = true;
    fees_error_.clear();
    fees_unsupported_ = false;
    if (fees_table_ && fees_table_->rowCount() == 0)
        show_table_loading(fees_stack_);
    render_market_info();
}

void CryptoBottomPanel::set_funding_unavailable(const QString& why) {
    funding_na_ = why.isEmpty() ? tr("not available") : why;
    render_market_info();
}

void CryptoBottomPanel::set_open_interest_unavailable(const QString& why) {
    oi_na_ = why.isEmpty() ? tr("not available") : why;
    render_market_info();
}

bool CryptoBottomPanel::fee_view_visible() const {
    if (!tabs_)
        return false;
    const int idx = tabs_->currentIndex();
    return idx >= 0 && (idx == fees_tab_idx_ || idx == market_tab_idx_);
}

bool CryptoBottomPanel::market_view_visible() const {
    return tabs_ && market_tab_idx_ >= 0 && tabs_->currentIndex() == market_tab_idx_;
}

bool CryptoBottomPanel::show_section(const QString& key) {
    if (!tabs_)
        return false;
    const QString k = key.trimmed().toLower();
    int idx = -1;
    if (k == QLatin1String("positions"))
        idx = positions_tab_idx_;
    else if (k == QLatin1String("orders") || k == QLatin1String("open_orders"))
        idx = orders_tab_idx_;
    else if (k == QLatin1String("history"))
        idx = trades_tab_idx_;
    else if (k == QLatin1String("my_trades") || k == QLatin1String("fills"))
        idx = my_trades_tab_idx_;
    else if (k == QLatin1String("fees"))
        idx = fees_tab_idx_;
    else if (k == QLatin1String("market"))
        idx = market_tab_idx_;
    else if (k == QLatin1String("stats"))
        idx = stats_tab_idx_;
    else if (k == QLatin1String("tape") || k == QLatin1String("time_sales"))
        idx = time_sales_tab_idx_;
    else if (k == QLatin1String("depth"))
        idx = depth_tab_idx_;
    if (idx < 0 || idx >= tabs_->count() || !tabs_->isTabVisible(idx))
        return false;
    tabs_->setCurrentIndex(idx); // currentChanged -> on_tab_changed, as on a click
    return true;
}

void CryptoBottomPanel::set_live_feed_error(const QString& message) {
    const QString why = message.isEmpty() ? QString() : tr("Live data unavailable — %1").arg(message);
    if (time_sales_)
        time_sales_->set_status_text(why);
    if (depth_chart_)
        depth_chart_->set_status_text(why);
}

void CryptoBottomPanel::on_tab_changed(int index) {
    if (index < 0)
        return;
    if (index == fees_tab_idx_ || index == market_tab_idx_)
        emit fees_wanted(false);
    if (index == market_tab_idx_)
        emit market_info_wanted();
}

void CryptoBottomPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Opened on FEES / MARKET: their data is wanted now (the screen skips what it already has).
    if (fee_view_visible())
        emit fees_wanted(false);
    if (market_view_visible())
        emit market_info_wanted();
}

void CryptoBottomPanel::setup_positions_tab() {
    positions_table_ = make_table({
        {tr("Symbol"), ColMode::Stretch},
        {tr("Side"), ColMode::Compact},
        {tr("Qty"), ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("Entry"), ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("Mark"), ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("P&L"), ColMode::Numeric, 11, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("P&L %"), ColMode::Numeric, 8, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("Lev"), ColMode::Compact},
    });
    if (auto* pct_hdr = positions_table_->horizontalHeaderItem(6))
        pct_hdr->setToolTip(tr("Unrealised P&L as a percent of the margin posted (entry × qty ÷ leverage)"));

    // Wrap table + action bar in a container
    auto* container = new QWidget;
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);

    // Action bar with Square Off All button
    auto* action_bar = new QWidget;
    action_bar->setObjectName("cryptoPositionsActionBar");
    auto* bar_layout = new QHBoxLayout(action_bar);
    bar_layout->setContentsMargins(10, 3, 10, 3);
    bar_layout->setSpacing(8);
    bar_layout->addStretch(1);

    close_all_btn_ = new QPushButton(tr("SQUARE OFF ALL"));
    close_all_btn_->setObjectName("cryptoCloseAllBtn");
    close_all_btn_->setProperty("fincept_agent_risk", "order");
    close_all_btn_->setCursor(Qt::PointingHandCursor);
    close_all_btn_->setAccessibleName(tr("Close every open position at market"));
    close_all_btn_->setStyleSheet(
        QString("QPushButton{background:%3;color:%1;border:1px solid %2;"
                "padding:3px 12px;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:0.5px;border-radius:2px;}"
                "QPushButton:hover{background:%4;}")
            .arg(colors::NEGATIVE(), colors::NEGATIVE_DIM(), colors::alpha(colors::NEGATIVE, 0.12),
                 colors::alpha(colors::NEGATIVE, 0.25)));
    connect(close_all_btn_, &QPushButton::clicked, this, [this]() {
        // No account_id_ gate: nothing ever called set_account_id(), so the old
        // `if (account_id_.isEmpty()) return;` made this button (and CANCEL ALL
        // below) a silent no-op. The screen's handler resolves the paper
        // portfolio / live exchange itself and ignores the argument.
        // LIVE: the screen fetches the positions in scope fresh and lists the
        // exact closing orders in its own confirmation — a generic "close ALL?"
        // here would be a second, less precise question.
        if (is_paper_) {
            auto answer = QMessageBox::warning(this, tr("Square Off All Positions"),
                                               tr("This will close ALL open paper positions at market.\n\n"
                                                  "Are you sure?"),
                                               QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes)
                return;
        }
        emit close_all_positions_requested(account_id_);
    });
    bar_layout->addWidget(close_all_btn_);
    vlay->addWidget(action_bar);

    // Per-position close: close_position_requested had a receiver in the screen
    // but nothing ever emitted it. A right-click menu on the row is the obvious
    // place; closing is a market order, so confirm it (mode-aware).
    positions_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    positions_table_->setToolTip(tr("Right-click a position to close it"));
    connect(positions_table_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        const int row = positions_table_->rowAt(pos.y());
        auto* sym_item = row >= 0 ? positions_table_->item(row, 0) : nullptr;
        if (!sym_item || sym_item->text().isEmpty())
            return;
        const QString symbol = sym_item->text();
        QMenu menu(this);
        QAction* close_act = menu.addAction(tr("Close position (%1) at market").arg(symbol));
        close_act->setProperty("fincept_agent_risk", "order");
        if (menu.exec(positions_table_->viewport()->mapToGlobal(pos)) != close_act)
            return;
        if (!is_paper_) {
            // LIVE: the screen prepares the exact reduce-only order and confirms it.
            emit close_position_requested(symbol);
            return;
        }
        const auto answer = QMessageBox::warning(this, tr("Close Position"),
                                                 tr("Close your paper position in %1 at market?").arg(symbol),
                                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes)
            emit close_position_requested(symbol);
    });

    // Starts on the skeleton: the paper account loads on a worker right after the screen opens.
    auto* stack_host = cbp_wrap_states(positions_table_, positions_stack_, positions_empty_,
                                       tr("Loading positions…"), true);
    vlay->addWidget(stack_host, 1);

    positions_tab_idx_ = tabs_->addTab(container, tr("POSITIONS"));
}

void CryptoBottomPanel::setup_orders_tab() {
    orders_table_ = make_table({
        {tr("Symbol"), ColMode::Stretch},
        {tr("Side"), ColMode::Compact},
        {tr("Type"), ColMode::Compact},
        {tr("Qty"), ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("Price"), ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("Status"), ColMode::Compact},
        {QString(), ColMode::Action, 0, 36}, // cancel button column
    });

    // Wrap table + action bar in a container
    auto* container = new QWidget;
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);

    // Action bar with Cancel All button
    auto* action_bar = new QWidget;
    action_bar->setObjectName("cryptoOrdersActionBar");
    auto* bar_layout = new QHBoxLayout(action_bar);
    bar_layout->setContentsMargins(10, 3, 10, 3);
    bar_layout->setSpacing(8);
    bar_layout->addStretch(1);

    cancel_all_btn_ = new QPushButton(tr("CANCEL ALL ORDERS"));
    cancel_all_btn_->setObjectName("cryptoCancelAllBtn");
    cancel_all_btn_->setProperty("fincept_agent_risk", "order");
    cancel_all_btn_->setCursor(Qt::PointingHandCursor);
    cancel_all_btn_->setAccessibleName(tr("Cancel every pending order"));
    cancel_all_btn_->setStyleSheet(
        QString("QPushButton{background:%3;color:%1;border:1px solid %2;"
                "padding:3px 12px;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:0.5px;border-radius:2px;}"
                "QPushButton:hover{background:%4;}")
            .arg(colors::AMBER(), colors::AMBER_DIM(), colors::alpha(colors::AMBER, 0.12),
                 colors::alpha(colors::AMBER, 0.25)));
    connect(cancel_all_btn_, &QPushButton::clicked, this, [this]() {
        // LIVE: the screen lists the orders in scope (fetched fresh) and confirms.
        if (is_paper_) {
            auto answer = QMessageBox::warning(this, tr("Cancel All Orders"),
                                               tr("This will cancel ALL pending paper orders.\n\nAre you sure?"),
                                               QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes)
                return;
        }
        emit cancel_all_orders_requested(account_id_);
    });
    bar_layout->addWidget(cancel_all_btn_);
    vlay->addWidget(action_bar);

    auto* stack_host = cbp_wrap_states(orders_table_, orders_stack_, orders_empty_, tr("Loading orders…"), true);
    vlay->addWidget(stack_host, 1);

    orders_tab_idx_ = tabs_->addTab(container, tr("ORDERS"));
}

void CryptoBottomPanel::setup_trades_tab() {
    trades_table_ = make_table({
        {tr("Symbol"), ColMode::Stretch},
        {tr("Side"), ColMode::Compact},
        {tr("Price"), ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("Qty"), ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("Fee"), ColMode::Numeric, 8, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("P&L"), ColMode::Numeric, 11, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("Time"), ColMode::Compact},
    });
    auto* host = cbp_wrap_states(trades_table_, trades_stack_, trades_empty_, tr("Loading trade history…"), true);
    trades_tab_idx_ = tabs_->addTab(host, tr("HISTORY"));
}

void CryptoBottomPanel::setup_my_trades_tab() {
    my_trades_table_ = make_table({
        {tr("Symbol"), ColMode::Stretch},
        {tr("Side"), ColMode::Compact},
        {tr("Price"), ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("Amount"), ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("Cost"), ColMode::Numeric, 11, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("Fee"), ColMode::Numeric, 9, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("Fee ccy"), ColMode::Compact},
        {tr("Time"), ColMode::Compact},
    });
    // PAPER (the start mode) shows what the tab is for and what LIVE needs; LIVE
    // shows the skeleton until the first fills reply (clear_account_views).
    auto* host = cbp_wrap_states(my_trades_table_, my_trades_stack_, my_trades_empty_,
                                 tr("Loading your exchange fills…"), false);
    my_trades_tab_idx_ = tabs_->addTab(host, tr("MY TRADES"));
}

void CryptoBottomPanel::setup_fees_tab() {
    fees_table_ = make_table({
        {tr("Symbol"), ColMode::Stretch},
        {tr("Maker %"), ColMode::Numeric, 8, 0, Qt::AlignRight | Qt::AlignVCenter},
        {tr("Taker %"), ColMode::Numeric, 8, 0, Qt::AlignRight | Qt::AlignVCenter},
    });
    // Loaded when FEES (or MARKET) opens: the published schedule needs no key.
    auto* host = cbp_wrap_states(fees_table_, fees_stack_, fees_empty_, tr("Loading the fee schedule…"), false);
    fees_tab_idx_ = tabs_->addTab(host, tr("FEES"));
}

void CryptoBottomPanel::setup_market_info_tab() {
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("cryptoCardHost");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* widget = new QWidget;
    widget->setObjectName("cryptoCardHost");
    auto* outer = new QVBoxLayout(widget);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(0);

    // Six tiles, three per row (wrapping at narrow widths / large fonts). They
    // load (skeleton tiles) until the funding, open-interest and fee replies land;
    // a figure the venue does not have for the pair says why instead of "--".
    market_strip_ = new ui::KpiStrip(widget);
    market_strip_->set_max_columns(3);
    const QList<QPair<const char*, QString>> tiles = {
        {"funding", tr("FUNDING RATE")}, {"mark", tr("MARK PRICE")},     {"index", tr("INDEX PRICE")},
        {"oi", tr("OPEN INTEREST")},     {"fees", tr("MAKER / TAKER")}, {"next", tr("NEXT FUNDING")}};
    for (const auto& t : tiles)
        market_strip_->add_tile(QString::fromLatin1(t.first), t.second)->set_state(ui::StatTile::State::Loading);
    outer->addWidget(market_strip_);
    outer->addStretch();

    scroll->setWidget(widget);
    market_tab_idx_ = tabs_->addTab(scroll, tr("MARKET"));
}

namespace {
// Stats strip tile keys, in display order (titles in stats_tile_title()).
const char* const kStatKeys[] = {"pnl", "today", "win", "pf", "trades", "avg_win",
                                 "avg_loss", "best", "worst", "fees", "turnover"};
const char* const kLiveKeys[] = {"bal", "eq", "margin"};

QString stats_tile_title(const char* key) {
    static const QHash<QString, const char*> titles = {
        {"pnl", QT_TRANSLATE_NOOP("fincept::screens::crypto::CryptoBottomPanel", "TOTAL P&L")},
        {"today", QT_TRANSLATE_NOOP("fincept::screens::crypto::CryptoBottomPanel", "TODAY P&L")},
        {"win", QT_TRANSLATE_NOOP("fincept::screens::crypto::CryptoBottomPanel", "WIN RATE")},
        {"pf", QT_TRANSLATE_NOOP("fincept::screens::crypto::CryptoBottomPanel", "PROFIT FACTOR")},
        {"trades", QT_TRANSLATE_NOOP("fincept::screens::crypto::CryptoBottomPanel", "TOTAL TRADES")},
        {"avg_win", QT_TRANSLATE_NOOP("fincept::screens::crypto::CryptoBottomPanel", "AVG WIN")},
        {"avg_loss", QT_TRANSLATE_NOOP("fincept::screens::crypto::CryptoBottomPanel", "AVG LOSS")},
        {"best", QT_TRANSLATE_NOOP("fincept::screens::crypto::CryptoBottomPanel", "BEST TRADE")},
        {"worst", QT_TRANSLATE_NOOP("fincept::screens::crypto::CryptoBottomPanel", "WORST TRADE")},
        {"fees", QT_TRANSLATE_NOOP("fincept::screens::crypto::CryptoBottomPanel", "FEES")},
        {"turnover", QT_TRANSLATE_NOOP("fincept::screens::crypto::CryptoBottomPanel", "TURNOVER")},
        {"bal", QT_TRANSLATE_NOOP("fincept::screens::crypto::CryptoBottomPanel", "LIVE BALANCE")},
        {"eq", QT_TRANSLATE_NOOP("fincept::screens::crypto::CryptoBottomPanel", "LIVE EQUITY")},
        {"margin", QT_TRANSLATE_NOOP("fincept::screens::crypto::CryptoBottomPanel", "USED MARGIN")},
    };
    return QCoreApplication::translate("fincept::screens::crypto::CryptoBottomPanel",
                                       titles.value(QString::fromLatin1(key), ""));
}
} // namespace

void CryptoBottomPanel::setup_stats_tab() {
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("cryptoCardHost");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* widget = new QWidget;
    widget->setObjectName("cryptoCardHost");
    auto* outer = new QVBoxLayout(widget);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(10);

    // No paper trade yet: a designed state instead of a row of zeros (render_stats).
    stats_empty_ = new ui::EmptyState(widget);
    stats_empty_->set_compact(true);
    stats_empty_->setVisible(false);
    outer->addWidget(stats_empty_);

    // Paper trade statistics (pt_get_stats): every aggregate the struct carries.
    // Skeleton tiles until the paper account's first statistics arrive.
    stats_strip_ = new ui::KpiStrip(widget);
    stats_strip_->set_max_columns(4);
    for (const char* key : kStatKeys)
        stats_strip_->add_tile(QString::fromLatin1(key), stats_tile_title(key))
            ->set_state(ui::StatTile::State::Loading);
    outer->addWidget(stats_strip_);

    // Live account tiles. set_live_balance() / set_balance_unavailable() fill
    // them; they are shown in LIVE mode only (apply_mode_labels).
    live_strip_ = new ui::KpiStrip(widget);
    live_strip_->set_max_columns(3);
    for (const char* key : kLiveKeys)
        live_strip_->add_tile(QString::fromLatin1(key), stats_tile_title(key))
            ->set_state(ui::StatTile::State::Loading);
    live_strip_->setVisible(!is_paper_);
    outer->addWidget(live_strip_);
    outer->addStretch();

    scroll->setWidget(widget);
    stats_tab_idx_ = tabs_->addTab(scroll, tr("STATS"));
}

// ── Forwarding methods for new widgets ──────────────────────────────────────

void CryptoBottomPanel::add_trade_entry(const TradeEntry& trade) {
    time_sales_->add_trade(trade);
}

void CryptoBottomPanel::seed_trade_history(const QVector<TradeEntry>& trades) {
    if (time_sales_)
        time_sales_->merge_history(trades);
}

void CryptoBottomPanel::reset_market_data() {
    if (time_sales_)
        time_sales_->set_trades({});
    if (depth_chart_)
        depth_chart_->set_data({}, {}, 0.0, 0.0);
    // The MARKET tiles and the fee schedule belong to the previous pair / venue:
    // back to loading until the new one's figures arrive.
    market_info_ = MarketInfoData{};
    funding_na_.clear();
    oi_na_.clear();
    fees_json_ = QJsonObject();
    has_fees_ = false;
    fees_known_ = false;
    fees_loading_ = false;
    fees_error_.clear();
    fees_unsupported_ = false;
    if (fees_table_)
        fees_table_->setRowCount(0);
    apply_fees_empty();
    update_empty_state(fees_table_, fees_stack_, 0);
    render_market_info();
}

void CryptoBottomPanel::set_live_positions_unavailable(const QString& reason) {
    // In LIVE mode the table may still hold paper rows from before the toggle;
    // clear them and say why in the designed state.
    positions_live_ = true;
    live_positions_ = QJsonArray();
    last_marks_.clear();
    positions_ccys_.clear();
    update_positions_pnl_header();
    if (positions_table_)
        positions_table_->setRowCount(0);
    positions_reason_ = reason.isEmpty() ? tr("no reply") : reason;
    apply_positions_empty();
    update_empty_state(positions_table_, positions_stack_, 0);
}

void CryptoBottomPanel::set_live_orders_unavailable(const QString& reason) {
    orders_live_ = true;
    live_orders_ = QJsonArray();
    if (orders_table_)
        orders_table_->setRowCount(0);
    orders_reason_ = reason.isEmpty() ? tr("no reply") : reason;
    apply_orders_empty();
    update_empty_state(orders_table_, orders_stack_, 0);
}

void CryptoBottomPanel::set_depth_data(const QVector<QPair<double, double>>& bids,
                                       const QVector<QPair<double, double>>& asks, double spread, double spread_pct) {
    depth_chart_->set_data(bids, asks, spread, spread_pct);
}

// ── Data setters ────────────────────────────────────────────────────────────

void CryptoBottomPanel::set_positions(const QVector<trading::PtPosition>& positions) {
    if (positions_live_)
        last_marks_.clear(); // marks of the other mode's rows
    positions_live_ = false;
    paper_positions_ = positions;
    const int n = positions.size();
    // Each paper position's unrealized P&L is in its pair's quote asset (the
    // engine marks it in price units, unconverted).
    positions_ccys_.clear();
    for (const auto& pos : positions)
        positions_ccys_ << pair_quote_ccy(pos.symbol);
    const fmt::CurrencyMix pnl_mix = cbp_mix_of(positions_ccys_);
    update_positions_pnl_header();
    SortSuspend no_sort(positions_table_);
    positions_table_->setUpdatesEnabled(false);
    if (positions_table_->rowCount() != n)
        positions_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        const auto& pos = positions[i];
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(positions_table_, i, col);
            if (it->text() != text)
                it->setText(text);
            style_item(it, fg, bg, align);
        };
        auto num = [&](int col, double value, const QString& text, const QColor& fg = QColor()) {
            style_item(put_num(positions_table_, i, col, value, text), fg, bg, kRight);
        };

        set(0, pos.symbol);
        set(1, pos.side.toUpper(), pos.side == "long" ? kColorPos() : kColorNeg());
        num(2, pos.quantity, format_size(pos.quantity));
        // Prices in the pair's quote asset (the symbol on the row names it).
        num(3, pos.entry_price, pair_price(pos.entry_price, pos.symbol));
        num(4, pos.current_price, pair_price(pos.current_price, pos.symbol));
        num(5, pos.unrealized_pnl,
            fmt::money_in_column(pos.unrealized_pnl, positions_ccys_.value(i), pnl_mix, fmt::Compact::None, -1, true),
            pnl_color(pos.unrealized_pnl));
        // Margin posted = entry × qty ÷ leverage (the paper engine's own model).
        const double lev = pos.leverage > 0.0 ? pos.leverage : 1.0;
        const double margin = pos.entry_price * pos.quantity / lev;
        const double pct = margin_pnl_pct(pos.unrealized_pnl, margin);
        num(6, std::isfinite(pct) ? pct : 0.0, std::isfinite(pct) ? fmt::pct(pct, 2) : fmt::placeholder(),
            std::isfinite(pct) ? pnl_color(pct) : kColorSec());
        num(7, pos.leverage, fmt::ratio(pos.leverage, 1));

        // Stash the exact values the live mark-to-market path needs so it
        // never has to re-parse rounded display text.
        ensure_item(positions_table_, i, 0)->setData(kPnlCcyRole, positions_ccys_.value(i));
        ensure_item(positions_table_, i, 2)->setData(kExactValueRole, pos.quantity);
        ensure_item(positions_table_, i, 3)->setData(kExactValueRole, pos.entry_price);
        ensure_item(positions_table_, i, 6)->setData(kExactValueRole, margin);
    }
    positions_table_->setUpdatesEnabled(true);
    paper_error_.clear(); // the paper account answered
    if (n == 0)
        apply_positions_empty();
    update_empty_state(positions_table_, positions_stack_, n);
}

void CryptoBottomPanel::update_position_prices(const QHash<QString, double>& last_prices) {
    if (last_prices.isEmpty() || !positions_table_)
        return;
    const int n = positions_table_->rowCount();
    if (n == 0)
        return;
    for (auto it = last_prices.constBegin(); it != last_prices.constEnd(); ++it)
        last_marks_.insert(it.key(), it.value()); // re-applied after a format-change re-render
    const fmt::CurrencyMix pnl_mix = cbp_mix_of(positions_ccys_);
    // Each row is patched from its own cells (symbol, side, stashed exact
    // values), so a user sort does not misalign it; sorting is only paused so
    // the rows do not move while this loop walks them by index.
    SortSuspend no_sort(positions_table_);
    positions_table_->setUpdatesEnabled(false);
    for (int i = 0; i < n; ++i) {
        auto* sym_item = positions_table_->item(i, 0);
        if (!sym_item)
            continue;
        const QString sym = sym_item->text();
        auto it = last_prices.constFind(sym);
        if (it == last_prices.constEnd())
            continue;
        const double mark = it.value();
        if (mark <= 0.0)
            continue;

        auto* side_item = positions_table_->item(i, 1);
        auto* qty_item = positions_table_->item(i, 2);
        auto* entry_item = positions_table_->item(i, 3);
        auto* cur_item = positions_table_->item(i, 4);
        auto* pnl_item = positions_table_->item(i, 5);
        if (!side_item || !qty_item || !entry_item || !cur_item || !pnl_item)
            continue;

        const bool is_long = side_item->text().compare("LONG", Qt::CaseInsensitive) == 0;
        // Prefer the exact values stashed by set_positions/set_live_positions;
        // fall back to the display text only for rows written by an older
        // path that didn't stash them.
        const QVariant qty_exact = qty_item->data(kExactValueRole);
        const QVariant entry_exact = entry_item->data(kExactValueRole);
        // A live contract row holds contracts: × the contract size gives base units.
        const QVariant csize_v = qty_item->data(kContractSizeRole);
        const double csize = csize_v.isValid() && csize_v.toDouble() > 0.0 ? csize_v.toDouble() : 1.0;
        const double qty = (qty_exact.isValid() ? qty_exact.toDouble() : qty_item->text().toDouble()) * csize;
        const double entry = entry_exact.isValid() ? entry_exact.toDouble() : entry_item->text().toDouble();
        if (qty <= 0.0 || entry <= 0.0)
            continue;

        const double pnl = is_long ? (mark - entry) * qty : (entry - mark) * qty;

        put_num(positions_table_, i, 4, mark, pair_price(mark, sym));
        // (mark - entry) × size is in the pair's quote asset. A row whose P&L is
        // in another asset (an inverse contract settling in BTC) keeps the P&L the
        // exchange reported rather than showing a quote-asset figure under it.
        const QString pnl_ccy = sym_item->data(kPnlCcyRole).toString();
        if (!pnl_ccy.isEmpty() && pnl_ccy != pair_quote_ccy(sym))
            continue;
        auto* new_pnl = put_num(positions_table_, i, 5, pnl,
                                fmt::money_in_column(pnl, pnl_ccy, pnl_mix, fmt::Compact::None, -1, true));
        new_pnl->setForeground(pnl_color(pnl));
        if (auto* pct_item = positions_table_->item(i, 6)) {
            const double margin = pct_item->data(kExactValueRole).toDouble();
            const double pct = margin_pnl_pct(pnl, margin);
            if (std::isfinite(pct)) {
                auto* new_pct = put_num(positions_table_, i, 6, pct, fmt::pct(pct, 2));
                new_pct->setData(kExactValueRole, margin);
                new_pct->setForeground(pnl_color(pct));
            }
        }
    }
    positions_table_->setUpdatesEnabled(true);
}

void CryptoBottomPanel::set_orders(const QVector<trading::PtOrder>& orders) {
    orders_live_ = false;
    paper_orders_ = orders;
    QVector<trading::PtOrder> active;
    for (const auto& o : orders) {
        if (o.status == "pending" || o.status == "partial")
            active.append(o);
    }

    const int n = active.size();
    SortSuspend no_sort(orders_table_);
    orders_table_->setUpdatesEnabled(false);
    if (orders_table_->rowCount() != n)
        orders_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        const auto& o = active[i];
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(orders_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        set(0, o.symbol);
        set(1, o.side.toUpper(), o.side == "buy" ? kColorPos() : kColorNeg());
        set(2, o.order_type.toUpper());
        style_item(put_num(orders_table_, i, 3, o.quantity, format_size(o.quantity)), QColor(), bg, kRight);
        style_item(put_num(orders_table_, i, 4, o.price ? *o.price : 0.0,
                           o.price ? pair_price(*o.price, o.symbol) : QStringLiteral("MKT")),
                   QColor(), bg, kRight);
        set(5, o.status.toUpper(), kColorSec());

        auto* existing = qobject_cast<QPushButton*>(orders_table_->cellWidget(i, 6));
        if (existing) {
            existing->setProperty("order_id", o.id);
            existing->setProperty("order_symbol", o.symbol);
            existing->setEnabled(true); // a LIVE row's in-flight cancel state never carries over
        } else {
            auto* cancel_btn = new QPushButton(QStringLiteral("✕"));
            cancel_btn->setObjectName("cryptoCancelBtn");
            cancel_btn->setToolTip(tr("Cancel order"));
            cancel_btn->setCursor(Qt::PointingHandCursor);
            cancel_btn->setProperty("order_id", o.id);
            cancel_btn->setProperty("order_symbol", o.symbol);
            cancel_btn->setProperty("fincept_agent_risk", "order");
            cancel_btn->setFocusPolicy(Qt::NoFocus);
            connect(cancel_btn, &QPushButton::clicked, this, [this, cancel_btn]() {
                emit cancel_order_requested(cancel_btn->property("order_id").toString(),
                                            cancel_btn->property("order_symbol").toString());
            });
            orders_table_->setCellWidget(i, 6, cancel_btn);
        }
    }
    orders_table_->setUpdatesEnabled(true);
    if (n == 0)
        apply_orders_empty();
    update_empty_state(orders_table_, orders_stack_, n);
}

void CryptoBottomPanel::set_trades(const QVector<trading::PtTrade>& trades, const QString& ccy) {
    paper_trades_ = trades;
    if (!ccy.trimmed().isEmpty())
        trades_ccy_ = ccy.trimmed();
    // Price in the pair's quote asset; fee and realized P&L as the paper engine
    // books them: in the portfolio's currency (at the pair's FX factor).
    const QString& book_ccy = trades_ccy_;
    if (auto* h = trades_table_->horizontalHeaderItem(5))
        h->setToolTip(book_ccy.isEmpty()
                          ? QString()
                          : tr("Realized P&L as booked by the paper account, in %1 (converted from the pair's "
                               "quote asset at the rate when the fill was booked)")
                                .arg(book_ccy));
    const int n = std::min(static_cast<int>(trades.size()), 50);
    SortSuspend no_sort(trades_table_);
    trades_table_->setUpdatesEnabled(false);
    if (trades_table_->rowCount() != n)
        trades_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        const auto& t = trades[i];
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(trades_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };
        auto num = [&](int col, double value, const QString& text, const QColor& fg = QColor()) {
            auto* it = put_num(trades_table_, i, col, value, text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(kRight);
            return it;
        };

        set(0, t.symbol);
        set(1, t.side.toUpper(), t.side == "buy" ? kColorPos() : kColorNeg());
        num(2, t.price, pair_price(t.price, t.symbol));
        num(3, t.quantity, format_size(t.quantity));
        num(4, t.fee, fmt::money_precise(t.fee, book_ccy), kColorSec());
        // An opening fill realises nothing: "—" rather than a green 0.00.
        num(5, t.pnl, t.pnl == 0.0 ? fmt::placeholder() : fmt::signed_money(t.pnl, book_ccy), pnl_color(t.pnl));
        qint64 ts_ms = 0;
        const QString when = fill_time_text(t.timestamp, &ts_ms);
        auto* time_item = num(6, static_cast<double>(ts_ms), when, kColorTert());
        time_item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        time_item->setToolTip(t.timestamp);
    }
    trades_table_->setUpdatesEnabled(true);
    if (n == 0)
        apply_trades_empty();
    update_empty_state(trades_table_, trades_stack_, n);
}

void CryptoBottomPanel::set_stats(const trading::PtStats& stats, const QString& ccy, const QStringList& quote_assets) {
    if (!ccy.trimmed().isEmpty())
        stats_ccy_ = ccy.trimmed();
    stats_ = stats;
    stats_quote_assets_ = quote_assets;
    has_stats_ = true;
    render_stats();
}

void CryptoBottomPanel::render_stats() {
    if (!stats_strip_ || !has_stats_)
        return;
    const trading::PtStats& stats = stats_;
    // No paper trade yet: the designed state instead of a row of zeros and dashes.
    const bool no_trades = stats.total_trades == 0;
    if (stats_empty_ && stats_empty_->isHidden() == no_trades)
        stats_empty_->setVisible(no_trades);
    if (stats_strip_->isHidden() != no_trades)
        stats_strip_->setVisible(!no_trades);
    using Tone = ui::StatTile::Tone;
    auto tone_of = [](double v) { return v > 0 ? Tone::Positive : (v < 0 ? Tone::Negative : Tone::Neutral); };
    auto put = [this](const char* key, const QString& value, Tone tone = Tone::Neutral, const QString& sub = {},
                      const QString& tip = {}) {
        if (auto* t = stats_strip_->tile(QString::fromLatin1(key))) {
            t->set_state(ui::StatTile::State::Normal);
            t->set_value(value);
            t->set_tone(tone);
            t->set_sub_label(sub);
            t->set_tooltip_text(tip);
        }
    };
    // P&L, fees and the per-trade figures are realized amounts the paper engine
    // books in the portfolio's currency (each fill converted at the pair's FX
    // factor), so they add up in it.
    const QString& c = stats_ccy_;
    const int64_t decided = stats.winning_trades + stats.losing_trades;

    put("pnl", fmt::signed_money(stats.total_pnl, c), tone_of(stats.total_pnl));
    put("today", fmt::signed_money(stats.today_pnl, c), tone_of(stats.today_pnl));
    // win_rate is a fraction (0..1) of decided trades — never print it as a percent as is.
    put("win", decided > 0 ? fmt::fraction_pct(stats.win_rate, 1, false) : fmt::placeholder(), Tone::Neutral,
        tr("%1 W / %2 L").arg(stats.winning_trades).arg(stats.losing_trades));
    // Gross profit ÷ |gross loss|; with no losing trade yet it is unbounded.
    QString pf = fmt::placeholder();
    Tone pf_tone = Tone::Neutral;
    if (stats.losing_trades > 0) {
        pf = fmt::number(stats.profit_factor, 2);
        pf_tone = stats.profit_factor >= 1.0 ? Tone::Positive : Tone::Negative;
    } else if (stats.winning_trades > 0) {
        pf = QStringLiteral("∞");
        pf_tone = Tone::Positive;
    }
    put("pf", pf, pf_tone);
    put("trades", fmt::number(static_cast<double>(stats.total_trades), 0), Tone::Neutral,
        tr("%1 closing").arg(decided));
    put("avg_win", stats.winning_trades > 0 ? fmt::signed_money(stats.avg_win, c) : fmt::placeholder(),
        Tone::Positive);
    put("avg_loss", stats.losing_trades > 0 ? fmt::signed_money(stats.avg_loss, c) : fmt::placeholder(),
        Tone::Negative);
    put("best", stats.winning_trades > 0 ? fmt::signed_money(stats.largest_win, c) : fmt::placeholder(),
        Tone::Positive);
    put("worst", stats.losing_trades > 0 ? fmt::signed_money(stats.largest_loss, c) : fmt::placeholder(),
        Tone::Negative);
    put("fees", fmt::money(stats.total_fees, c));

    // TURNOVER is Σ price × quantity over the fills, each in its pair's QUOTE
    // asset (the engine does not convert it). One quote asset: shown converted to
    // the portfolio currency with the rate and its time. Several: never added up.
    QStringList quotes;
    for (const QString& q : stats_quote_assets_) {
        const QString u = q.trimmed().toUpper();
        if (!u.isEmpty() && !quotes.contains(u))
            quotes << u;
    }
    if (stats.turnover == 0.0) {
        put("turnover", fmt::money(0.0, c));
    } else if (quotes.size() > 1) {
        put("turnover", fmt::placeholder(), Tone::Neutral, tr("mixed quote assets"),
            tr("Turnover adds trade values in each pair's own quote asset (%1); they are different currencies, so "
               "no total is shown.")
                .arg(quotes.join(QStringLiteral(", "))));
    } else if (quotes.size() == 1) {
        const QString q = quotes.first();
        if (c.isEmpty() || q == c) {
            put("turnover", fmt::money(stats.turnover, q, fmt::Compact::FromSettings));
        } else {
            const fmt::ConvertedMoney cv = fincept::services::fx::convert_for_display(stats.turnover, q, c);
            if (cv.ok())
                put("turnover", fmt::converted_money(cv, fmt::Compact::FromSettings), Tone::Neutral,
                    tr("from %1").arg(fmt::money(stats.turnover, q, fmt::Compact::FromSettings)), fmt::fx_note(cv));
            else
                put("turnover", fmt::placeholder(), Tone::Neutral,
                    fmt::money(stats.turnover, q, fmt::Compact::FromSettings),
                    tr("No %1 → %2 exchange rate yet (it is being fetched).").arg(q, c));
        }
    } else {
        // Which quote assets the fills were in is not known yet.
        put("turnover", fmt::placeholder());
    }
}

void CryptoBottomPanel::set_market_info(const MarketInfoData& info) {
    if (!info.has_data)
        return;
    market_info_ = info;
    render_market_info();
}

void CryptoBottomPanel::render_market_info() {
    if (!market_strip_)
        return;
    const MarketInfoData& info = market_info_;
    using St = ui::StatTile;
    auto loading = [this](const char* key) {
        if (auto* t = market_strip_->tile(QString::fromLatin1(key)))
            t->set_state(St::State::Loading);
    };
    auto put = [this](const char* key, const QString& value, const QString& sub = QString(),
                      const QString& tip = QString()) {
        if (auto* t = market_strip_->tile(QString::fromLatin1(key))) {
            t->set_state(St::State::Normal);
            t->set_value(value);
            t->set_sub_label(sub);
            t->set_tooltip_text(tip);
        }
    };
    // A figure the venue does not have for the pair: "—" with the reason, not a fake 0.
    auto not_available = [&put](const char* key, const QString& why) { put(key, fmt::placeholder(), why, why); };

    // The funding and open-interest fetches land separately and either may be
    // unsupported for the pair (spot, or a venue without the endpoint) — a zero
    // is "no data", not a real 0.00 % / 0 reading.
    const bool have_funding =
        info.has_data && (info.mark_price > 0.0 || info.funding_rate != 0.0 || info.next_funding_time > 0);
    if (have_funding) {
        put("funding", fmt::pct(info.funding_rate * 100.0, 4));
        // Mark / index prices are in the market's quote asset (USDT for BTC/USDT:USDT).
        put("mark", info.mark_price > 0.0 ? pair_price_with_unit(info.mark_price, info.symbol) : fmt::placeholder());
        put("index",
            info.index_price > 0.0 ? pair_price_with_unit(info.index_price, info.symbol) : fmt::placeholder());
        // next_funding_time is the exchange's ms-epoch (ccxt nextFundingTimestamp);
        // reading it as seconds produced a garbage time-of-day.
        put("next", info.next_funding_time > 0
                        ? QDateTime::fromMSecsSinceEpoch(info.next_funding_time).toString(QStringLiteral("HH:mm:ss"))
                        : fmt::placeholder());
    } else if (!funding_na_.isEmpty()) {
        for (const char* key : {"funding", "mark", "index", "next"})
            not_available(key, funding_na_);
    } else {
        for (const char* key : {"funding", "mark", "index", "next"})
            loading(key);
    }

    // Open-interest value (ccxt openInterestValue) is in the market's quote
    // asset; abbreviated per the user's compact-number scheme. With no market
    // known it is a bare abbreviated number, never labelled as some currency.
    if (info.open_interest_value > 0.0) {
        const QString oi_ccy = pair_quote_ccy(info.symbol);
        put("oi",
            oi_ccy.isEmpty() ? fmt::compact(info.open_interest_value)
                             : fmt::money(info.open_interest_value, oi_ccy, fmt::Compact::FromSettings),
            info.symbol,
            info.open_interest > 0.0
                ? tr("Open interest %1 (%2)").arg(fmt::quantity(info.open_interest, 4), info.symbol)
                : QString());
    } else if (!oi_na_.isEmpty()) {
        not_available("oi", oi_na_);
    } else {
        loading("oi");
    }

    // MAKER / TAKER: from the fee schedule of the pair (FEES), else the market figures.
    const auto fee_text = [](double maker, double taker) {
        return QStringLiteral("%1 / %2").arg(fmt::pct(maker * 100.0, 3, false), fmt::pct(taker * 100.0, 3, false));
    };
    if (fees_known_) {
        put("fees", fee_text(fees_maker_, fees_taker_),
            fees_default_schedule_ ? tr("published schedule") : tr("your account tier"),
            fees_default_schedule_ ? tr("Exchange default fee schedule — your account tier may differ") : QString());
    } else if (info.maker_fee > 0.0 || info.taker_fee > 0.0) {
        put("fees", fee_text(info.maker_fee, info.taker_fee));
    } else if (!fees_error_.isEmpty()) {
        not_available("fees", fees_error_);
    } else {
        loading("fees");
    }
}

void CryptoBottomPanel::set_mode(bool is_paper) {
    is_paper_ = is_paper;
    apply_mode_labels();
    apply_all_empty_states(); // paper / live wording of the designed states
}

void CryptoBottomPanel::apply_mode_labels() {
    // HISTORY is the paper engine's trade log; exchange fills live on MY TRADES.
    if (tabs_ && trades_tab_idx_ >= 0)
        tabs_->setTabVisible(trades_tab_idx_, is_paper_);
    // The statistics tiles are computed from paper trades only (pt_get_stats) —
    // label them as such while LIVE so they can't be read as the live account's.
    if (stats_strip_) {
        for (const char* key : kStatKeys) {
            if (auto* t = stats_strip_->tile(QString::fromLatin1(key)))
                t->set_label(is_paper_ ? stats_tile_title(key) : tr("PAPER %1").arg(stats_tile_title(key)));
        }
    }
    if (live_strip_) {
        for (const char* key : kLiveKeys) {
            if (auto* t = live_strip_->tile(QString::fromLatin1(key)))
                t->set_label(stats_tile_title(key));
        }
        live_strip_->setVisible(!is_paper_);
    }
    update_bulk_labels();
}

void CryptoBottomPanel::set_scope_market(const QString& market) {
    scope_market_ = market;
    update_bulk_labels();
}

void CryptoBottomPanel::update_bulk_labels() {
    // LIVE bulk actions are scoped to the market on screen (the tables show only
    // it); the screen's confirmation lists what will be cancelled / closed and
    // offers the whole account as a separate, explicit choice.
    const bool scoped = !is_paper_ && !scope_market_.isEmpty();
    if (close_all_btn_) {
        close_all_btn_->setText(scoped ? tr("SQUARE OFF %1").arg(scope_market_)
                                       : (is_paper_ ? tr("SQUARE OFF ALL (PAPER)") : tr("SQUARE OFF ALL")));
        close_all_btn_->setAccessibleName(scoped ? tr("Close the open position in %1 at market").arg(scope_market_)
                                                 : tr("Close every open position at market"));
    }
    if (cancel_all_btn_) {
        cancel_all_btn_->setText(scoped ? tr("CANCEL ORDERS %1").arg(scope_market_)
                                        : (is_paper_ ? tr("CANCEL ALL (PAPER)") : tr("CANCEL ALL ORDERS")));
        cancel_all_btn_->setAccessibleName(scoped ? tr("Cancel the open orders in %1").arg(scope_market_)
                                                  : tr("Cancel every pending order"));
    }
}

void CryptoBottomPanel::clear_account_views() {
    // The held data belongs to the mode just left: drop it with the rows.
    paper_positions_.clear();
    live_positions_ = QJsonArray();
    last_marks_.clear();
    positions_ccys_.clear();
    paper_orders_.clear();
    live_orders_ = QJsonArray();
    paper_trades_.clear();
    has_stats_ = false;
    stats_quote_assets_.clear();
    my_trades_json_ = QJsonObject();
    has_my_trades_ = false;
    has_live_balance_ = false;
    update_positions_pnl_header();
    for (QTableWidget* t : {positions_table_, orders_table_, trades_table_, my_trades_table_}) {
        if (t)
            t->setRowCount(0);
    }
    // Reset any "unavailable: …" reason left by the other mode; the new mode's
    // first reply fills the tables, until then they show a skeleton (MY TRADES
    // in PAPER explains itself instead: it has no paper data).
    positions_reason_.clear();
    orders_reason_.clear();
    my_trades_reason_.clear();
    apply_all_empty_states();
    show_table_loading(positions_stack_);
    show_table_loading(orders_stack_);
    show_table_loading(trades_stack_);
    if (is_paper_)
        update_empty_state(my_trades_table_, my_trades_stack_, 0);
    else
        show_table_loading(my_trades_stack_);

    for (ui::KpiStrip* strip : {stats_strip_, live_strip_}) {
        if (!strip)
            continue;
        for (int i = 0; i < strip->count(); ++i) {
            if (auto* t = strip->tile(i)) {
                t->set_value(fmt::placeholder());
                t->set_tone(ui::StatTile::Tone::Neutral);
                t->set_sub_label(QString());
                t->set_tooltip_text(QString());
                t->set_state(ui::StatTile::State::Loading);
            }
        }
    }
    if (stats_empty_ && !stats_empty_->isHidden())
        stats_empty_->setVisible(false);
    if (stats_strip_ && stats_strip_->isHidden())
        stats_strip_->setVisible(true);
}

void CryptoBottomPanel::set_account_id(const QString& account_id) {
    account_id_ = account_id;
}

void CryptoBottomPanel::set_live_positions(const QJsonArray& positions) {
    if (!positions_live_)
        last_marks_.clear(); // marks of the other mode's rows
    positions_live_ = true;
    live_positions_ = positions;
    const int n = positions.size();
    // A live contract's unrealized P&L is in its settle asset as the exchange
    // reports it ("BTC/USDT:USDT" -> USDT, inverse "BTC/USD:BTC" -> BTC).
    positions_ccys_.clear();
    for (const auto& v : positions)
        positions_ccys_ << pair_settle_ccy(v.toObject().value("symbol").toString());
    const fmt::CurrencyMix pnl_mix = cbp_mix_of(positions_ccys_);
    update_positions_pnl_header();
    SortSuspend no_sort(positions_table_);
    positions_table_->setUpdatesEnabled(false);
    if (positions_table_->rowCount() != n)
        positions_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        auto p = positions[i].toObject();
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(positions_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };
        auto num = [&](int col, double value, const QString& text, const QColor& fg = QColor()) {
            style_item(put_num(positions_table_, i, col, value, text), fg, bg, kRight);
        };

        const QString side_str = p.value("side").toString();
        const QString sym = p.value("symbol").toString();
        const double contracts = p.value("contracts").toDouble();
        const double entry_px = p.value("entryPrice").toDouble();
        const double mark_px = p.value("markPrice").toDouble();
        set(0, sym);
        set(1, side_str.toUpper(), side_str.contains("long") ? kColorPos() : kColorNeg());
        num(2, contracts, format_size(contracts));
        num(3, entry_px, pair_price(entry_px, sym));
        num(4, mark_px, pair_price(mark_px, sym));
        const double pnl = p.value("unrealizedPnl").toDouble();
        num(5, pnl, fmt::money_in_column(pnl, positions_ccys_.value(i), pnl_mix, fmt::Compact::None, -1, true),
            pnl_color(pnl));
        // ccxt reports leverage as null when the venue doesn't say — "—", not "0x".
        const double lev = p.value("leverage").toDouble();
        // Margin posted: the venue's initialMargin when it reports one, else
        // entry × contracts × contract size ÷ leverage; unknown leverage = no %.
        double margin = p.value("initialMargin").toDouble();
        if (!(margin > 0.0) && lev > 0.0) {
            const double csize = p.value("contractSize").toDouble();
            margin = entry_px * contracts * (csize > 0.0 ? csize : 1.0) / lev;
        }
        const double pct = margin_pnl_pct(pnl, margin);
        num(6, std::isfinite(pct) ? pct : 0.0, std::isfinite(pct) ? fmt::pct(pct, 2) : fmt::placeholder(),
            std::isfinite(pct) ? pnl_color(pct) : kColorSec());
        num(7, lev, lev > 0.0 ? fmt::ratio(lev, 0) : fmt::placeholder());

        ensure_item(positions_table_, i, 0)->setData(kPnlCcyRole, positions_ccys_.value(i));
        ensure_item(positions_table_, i, 2)->setData(kExactValueRole, contracts);
        const double row_csize = p.value("contractSize").toDouble();
        ensure_item(positions_table_, i, 2)->setData(kContractSizeRole, row_csize > 0.0 ? row_csize : 1.0);
        ensure_item(positions_table_, i, 3)->setData(kExactValueRole, entry_px);
        ensure_item(positions_table_, i, 6)->setData(kExactValueRole, margin > 0.0 ? margin : 0.0);
    }
    positions_table_->setUpdatesEnabled(true);
    positions_reason_.clear(); // the live fetch answered
    if (n == 0)
        apply_positions_empty();
    update_empty_state(positions_table_, positions_stack_, n);
}

void CryptoBottomPanel::set_live_orders(const QJsonArray& orders) {
    orders_live_ = true;
    live_orders_ = orders;
    const int n = orders.size();
    SortSuspend no_sort(orders_table_);
    orders_table_->setUpdatesEnabled(false);
    if (orders_table_->rowCount() != n)
        orders_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        auto o = orders[i].toObject();
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(orders_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        const QString side_str = o.value("side").toString();
        set(0, o.value("symbol").toString());
        set(1, side_str.toUpper(), side_str == "buy" ? kColorPos() : kColorNeg());
        set(2, o.value("type").toString().toUpper());
        const double live_amt = o.value("amount").toDouble();
        style_item(put_num(orders_table_, i, 3, live_amt, format_size(live_amt)), QColor(), bg, kRight);
        // A market / plain-stop order has no limit price (ccxt: null) — show "MKT"
        // like the paper table instead of a misleading "0.00".
        const double live_px = o.value("price").toDouble();
        style_item(put_num(orders_table_, i, 4, live_px > 0.0 ? live_px : 0.0,
                           live_px > 0.0 ? pair_price(live_px, o.value("symbol").toString()) : QStringLiteral("MKT")),
                   QColor(), bg, kRight);
        // A cancel in flight keeps saying so until the exchange answers (set_order_cancelling).
        const bool cancelling = cancelling_ids_.contains(o.value("id").toString());
        set(5, cancelling ? tr("CANCELLING…") : o.value("status").toString().toUpper(), kColorSec());

        // The cancel must target the order's own market (its "symbol"), not the
        // screen's selected display pair — they differ on remapped venues.
        const QString order_symbol = o.value("symbol").toString();
        auto* existing_btn = qobject_cast<QPushButton*>(orders_table_->cellWidget(i, 6));
        if (existing_btn) {
            existing_btn->setProperty("order_id", o.value("id").toString());
            existing_btn->setProperty("order_symbol", order_symbol);
            existing_btn->setEnabled(!cancelling);
        } else {
            auto* cancel_btn = new QPushButton(QStringLiteral("✕"));
            cancel_btn->setObjectName("cryptoCancelBtn");
            cancel_btn->setToolTip(tr("Cancel order"));
            cancel_btn->setCursor(Qt::PointingHandCursor);
            cancel_btn->setProperty("order_id", o.value("id").toString());
            cancel_btn->setProperty("order_symbol", order_symbol);
            cancel_btn->setProperty("fincept_agent_risk", "order");
            cancel_btn->setFocusPolicy(Qt::NoFocus);
            connect(cancel_btn, &QPushButton::clicked, this, [this, cancel_btn]() {
                emit cancel_order_requested(cancel_btn->property("order_id").toString(),
                                            cancel_btn->property("order_symbol").toString());
            });
            cancel_btn->setEnabled(!cancelling);
            orders_table_->setCellWidget(i, 6, cancel_btn);
        }
    }
    orders_table_->setUpdatesEnabled(true);
    orders_reason_.clear(); // the live fetch answered
    if (n == 0)
        apply_orders_empty();
    update_empty_state(orders_table_, orders_stack_, n);
}

void CryptoBottomPanel::set_order_cancelling(const QString& order_id, bool on) {
    if (order_id.isEmpty())
        return;
    if (on)
        cancelling_ids_.insert(order_id);
    else
        cancelling_ids_.remove(order_id);
    if (!orders_live_)
        return;
    // Update just that row (the next refresh keeps the state while the id is in the set).
    for (int i = 0; i < orders_table_->rowCount(); ++i) {
        auto* btn = qobject_cast<QPushButton*>(orders_table_->cellWidget(i, 6));
        if (!btn || btn->property("order_id").toString() != order_id)
            continue;
        btn->setEnabled(!on);
        if (QTableWidgetItem* status = orders_table_->item(i, 5)) {
            QString text = tr("CANCELLING…");
            if (!on) {
                text.clear();
                for (const auto& v : std::as_const(live_orders_))
                    if (v.toObject().value("id").toString() == order_id)
                        text = v.toObject().value("status").toString().toUpper();
            }
            status->setText(text);
        }
        break;
    }
}

void CryptoBottomPanel::update_my_trades(const QJsonObject& json) {
    my_trades_json_ = json;
    has_my_trades_ = true;
    // A failed fetch ({"success":false,"error":…} / timeout {"error":…}) used to
    // render as zero rows under "No exchange-side fills" — say why instead.
    if (json.contains("error") || !json.contains("trades")) {
        my_trades_table_->setRowCount(0);
        const QString reason = json.value("error").toString();
        my_trades_reason_ = reason.isEmpty() ? tr("no reply") : reason;
        apply_my_trades_empty(); // names unreadable keys / a missing key / the error, with its action
        update_empty_state(my_trades_table_, my_trades_stack_, 0);
        return;
    }
    my_trades_reason_.clear();
    const QJsonArray trades = json.value("trades").toArray();
    if (trades.isEmpty())
        apply_my_trades_empty();
    const int n = trades.size();
    // COST (price × amount) is in each fill's quote asset: the column names the
    // unit in its header when there is one, else the code on every cell.
    QStringList cost_ccys;
    for (const auto& v : trades)
        cost_ccys << pair_quote_ccy(v.toObject().value("symbol").toString());
    const fmt::CurrencyMix cost_mix = cbp_mix_of(cost_ccys);
    if (auto* h = my_trades_table_->horizontalHeaderItem(4))
        h->setText(cost_mix.isEmpty() ? tr("Cost") : fmt::header(tr("Cost"), cost_mix));
    SortSuspend no_sort(my_trades_table_);
    my_trades_table_->setUpdatesEnabled(false);
    if (my_trades_table_->rowCount() != n)
        my_trades_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        const auto t = trades[i].toObject();
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(my_trades_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };
        auto num = [&](int col, double value, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = kRight) {
            auto* it = put_num(my_trades_table_, i, col, value, text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        const QString side = t.value("side").toString();
        const qint64 ts_ms = t.value("timestamp").toVariant().toLongLong();
        // Same display zone / locale as the HISTORY tab's fills.
        const QString time_str = ts_ms > 0 ? fmt::date_time(QDateTime::fromMSecsSinceEpoch(ts_ms),
                                                            fmt::DateStyle::Short, true)
                                           : fmt::placeholder();
        const double price = t.value("price").toDouble();
        const double amount = t.value("amount").toDouble();
        const double cost = t.value("cost").toDouble();
        const double fee = t.value("fee").toDouble();

        const QString sym = t.value("symbol").toString();
        set(0, sym);
        set(1, side.toUpper(), side == "buy" ? kColorPos() : kColorNeg());
        num(2, price, pair_price(price, sym));
        num(3, amount, format_size(amount));
        num(4, cost, fmt::money_in_column(cost, cost_ccys.value(i), cost_mix));
        // In the fee's own currency (the next column names it); trailing zeros trimmed.
        num(5, fee, fmt::quantity(fee, 8), kColorSec());
        set(6, t.value("fee_currency").toString(), kColorTert());
        num(7, static_cast<double>(ts_ms), time_str, kColorTert(), Qt::AlignLeft | Qt::AlignVCenter);
    }
    my_trades_table_->setUpdatesEnabled(true);
    update_empty_state(my_trades_table_, my_trades_stack_, n);
}

void CryptoBottomPanel::update_fees(const QJsonObject& json) {
    fees_json_ = json;
    has_fees_ = true;
    auto write_row = [&](int row, const QColor& bg, const QString& sym, double maker, double taker) {
        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = Qt::AlignRight | Qt::AlignVCenter) {
            auto* it = ensure_item(fees_table_, row, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };
        set(0, sym, QColor(), Qt::AlignLeft | Qt::AlignVCenter);
        for (int col = 1; col <= 2; ++col) {
            const double rate = col == 1 ? maker : taker;
            auto* it = put_num(fees_table_, row, col, rate, fmt::pct(rate * 100.0, 4, false));
            it->setForeground(kColorSec());
            it->setBackground(bg);
            it->setTextAlignment(kRight);
        }
    };
    SortSuspend no_sort(fees_table_);
    fees_loading_ = false;

    // An error reply has neither "symbol" nor "fees": say why (with RETRY) rather
    // than a generic "no fee schedule".
    if (json.contains("error") || (!json.contains("symbol") && !json.contains("fees"))) {
        fees_table_->setRowCount(0);
        const QString reason = json.value("error").toString();
        fees_error_ = reason.isEmpty() ? tr("no reply") : reason;
        fees_unsupported_ =
            json.value("code").toString().compare(QLatin1String("NOT_SUPPORTED"), Qt::CaseInsensitive) == 0;
        fees_known_ = false;
        apply_fees_empty();
        update_empty_state(fees_table_, fees_stack_, 0);
        render_market_info();
        return;
    }
    fees_error_.clear();
    fees_unsupported_ = false;
    // "market" = the venue's published default schedule (no account fee endpoint, or
    // no API key) — not necessarily the tier this account actually pays.
    fees_default_schedule_ = json.value("source").toString() == QLatin1String("market");

    if (json.contains("symbol")) {
        fees_table_->setUpdatesEnabled(false);
        fees_table_->setRowCount(1);
        write_row(0, kRowEven(), json.value("symbol").toString(), json.value("maker").toDouble(),
                  json.value("taker").toDouble());
        fees_table_->setUpdatesEnabled(true);
        // Feed the MARKET tab's MAKER / TAKER tile too — nothing else ever set it
        // (set_market_info's fee fields are never filled by the screen).
        fees_known_ = true;
        fees_maker_ = json.value("maker").toDouble();
        fees_taker_ = json.value("taker").toDouble();
        update_empty_state(fees_table_, fees_stack_, 1);
        render_market_info();
        return;
    }

    const QJsonArray fees = json.value("fees").toArray();
    const int n = fees.size();
    fees_table_->setUpdatesEnabled(false);
    if (fees_table_->rowCount() != n)
        fees_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        const auto f = fees[i].toObject();
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();
        write_row(i, bg, f.value("symbol").toString(), f.value("maker").toDouble(), f.value("taker").toDouble());
    }
    fees_table_->setUpdatesEnabled(true);
    if (n == 0)
        apply_fees_empty();
    update_empty_state(fees_table_, fees_stack_, n);
    render_market_info();
}

void CryptoBottomPanel::set_live_balance(double balance, double equity, double used_margin, const QString& ccy) {
    live_balance_ = balance;
    live_equity_ = equity;
    live_used_margin_ = used_margin;
    live_balance_ccy_ = ccy.trimmed();
    has_live_balance_ = true;
    render_live_balance();
}

void CryptoBottomPanel::render_live_balance() {
    if (!live_strip_ || !has_live_balance_)
        return;
    // The currency the exchange reported the balance in; none known = no unit
    // (never a guessed one).
    const QString& c = live_balance_ccy_;
    const double values[] = {live_balance_, live_equity_, live_used_margin_};
    for (int i = 0; i < 3; ++i) {
        if (auto* t = live_strip_->tile(QString::fromLatin1(kLiveKeys[i]))) {
            // Normal state clears any error reason left by a prior unavailable fetch.
            t->set_state(ui::StatTile::State::Normal);
            t->set_value(fmt::money(values[i], c));
            t->set_sub_label(c.isEmpty() ? QString() : (i == 0 ? tr("free %1").arg(c) : c));
        }
    }
}

void CryptoBottomPanel::set_balance_unavailable(const QString& reason) {
    has_live_balance_ = false;
    if (!live_strip_)
        return;
    const QString tip = reason.isEmpty() ? tr("Balance unavailable") : tr("Balance unavailable: %1").arg(reason);
    for (const char* key : kLiveKeys) {
        if (auto* t = live_strip_->tile(QString::fromLatin1(key))) {
            t->set_value(fmt::placeholder());
            t->set_sub_label(QString());
            t->set_state(ui::StatTile::State::Error, tip);
        }
    }
}

} // namespace fincept::screens::crypto
