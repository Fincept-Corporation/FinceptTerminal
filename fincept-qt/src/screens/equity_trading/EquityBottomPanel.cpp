// EquityBottomPanel.cpp — tabbed portfolio display
#include "screens/equity_trading/EquityBottomPanel.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/MnemonicText.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QShowEvent>
#include <QTimer>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimeZone>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace fincept::screens::equity {

namespace {

// Funds card slots (order must match setup_funds_tab + retranslateUi + set_funds_view).
enum FundCard {
    FC_Available = 0,
    FC_UsedMargin,
    FC_TotalEquity,
    FC_RealizedPnl,
    FC_UnrealizedPnl,
    FC_HoldingsValue,
    FC_OpeningBalance,
    FC_MarginUtil,
    FC_Collateral,
};

// Stats card slots (order must match setup_stats_tab + retranslateUi + set_stats_view).
enum StatCard {
    SC_NetPnl = 0,
    SC_TodayPnl,
    SC_ReturnPct,
    SC_WinRate,
    SC_ProfitFactor,
    SC_TotalTrades,
    SC_AvgWin,
    SC_AvgLoss,
    SC_Turnover,
    SC_LargestWin,
    SC_LargestLoss,
    SC_TotalFees,
};

// Money in the account currency `ccy` (ISO code; empty = unknown, printed without
// a unit) in the user's format (Settings > Currency & Number Format: symbol style,
// grouping incl. lakh, negative style). `signed_pfx` = P&L: an explicit sign.
QString eqbp_money(double v, const QString& ccy, bool signed_pfx = false) {
    return signed_pfx ? fincept::ui::formatting::signed_money(v, ccy) : fincept::ui::formatting::money(v, ccy);
}

// A money cell of a column that is all in `ccy` (its header names the unit): the
// number only, in the currency's own decimals.
QString eqbp_amount(double v, const QString& ccy) {
    return fincept::ui::formatting::number(v, ccy.isEmpty() ? 2 : fincept::ui::formatting::currency_decimals(ccy));
}

// Suffix for a positions / holdings total that leaves out rows whose price
// currency has no rate to the account currency yet (never added in at 1:1): the
// excluded count in words, never by colour alone.
QString eqbp_partial_note(int excluded) {
    return excluded > 0 ? QCoreApplication::translate("EquityBottomPanel", " (excl. %n without an FX rate)", nullptr,
                                                      excluded)
                        : QString();
}

// Holdings numeric cell. The cell's TEXT is formatted (digit grouping, fixed decimals, "%"), and the
// raw number rides in UserRole so sorting stays numeric. The previous code stored the double in the
// Display/Edit role (QTableWidgetItem treats them as ONE role), so the view rendered the bare double
// with 'g' / 6 significant digits: 1,234,567.89 showed as "1.23457E+06" and 123,456.79 as "123,457"
// — in the Invested / Current columns of any real portfolio.
class NumItem : public QTableWidgetItem {
  public:
    bool operator<(const QTableWidgetItem& other) const override {
        return data(Qt::UserRole).toDouble() < other.data(Qt::UserRole).toDouble();
    }
};

QTableWidgetItem* ensure_num_item(QTableWidget* table, int row, int col) {
    auto* item = table->item(row, col);
    if (!dynamic_cast<NumItem*>(item)) { // also true for a missing item
        item = new NumItem;
        item->setForeground(QColor(fincept::ui::colors::TEXT_PRIMARY()));
        table->setItem(row, col, item); // replaces (and deletes) any plain item already there
    }
    return item;
}

void set_num_cell(QTableWidgetItem* item, double value, const QString& text) {
    item->setText(text);
    item->setData(Qt::UserRole, value);
}

// setStyleSheet() re-parses the sheet and re-polishes the widget. The positions / holdings
// summary labels are refreshed on EVERY quote tick, but their colour only flips with the sign of
// the P&L — so only touch the stylesheet when it actually changed (CLAUDE.md P7).
void set_style_if_changed(QWidget* widget, const QString& style) {
    if (widget && widget->styleSheet() != style)
        widget->setStyleSheet(style);
}

QColor order_status_color(const QString& status) {
    const QString s = status.toLower();
    if (s == "filled" || s == "complete" || s == "completed")
        return {fincept::ui::colors::POSITIVE()};
    if (s == "rejected" || s == "cancelled" || s == "canceled")
        return {fincept::ui::colors::NEGATIVE()};
    return {fincept::ui::colors::AMBER()}; // pending / partial / open / accepted
}

// Terminal (dead) order statuses across the broker adapters. Everything else is
// still working at the exchange and MUST expose EDIT + CANCEL.
//
// The previous check was an exact, case-sensitive whitelist of Alpaca's four
// statuses ("new"/"accepted"/"partially_filled"/"pending_new"), so a live OPEN
// order on Zerodha ("OPEN"/"TRIGGER PENDING"), Fyers ("open"/"transit"),
// Dhan ("pending"/"transit") or Upstox ("open") showed NO cancel button at all —
// the trader could not pull a resting order from the blotter. Blacklisting the
// terminal states instead is correct for every broker, including future ones.
bool order_is_terminal(const QString& status) {
    static const QSet<QString> kTerminal = {
        QStringLiteral("complete"),   QStringLiteral("completed"), QStringLiteral("filled"),
        QStringLiteral("traded"),     QStringLiteral("executed"),  QStringLiteral("cancelled"),
        QStringLiteral("canceled"),   QStringLiteral("rejected"),  QStringLiteral("expired"),
        QStringLiteral("closed"),     QStringLiteral("replaced"),  QStringLiteral("done_for_day"),
        QStringLiteral("stopped"),    QStringLiteral("suspended"),
    };
    return kTerminal.contains(status.trimmed().toLower());
}

// ── Orders blotter ──────────────────────────────────────────────────────────

// Orders table columns (order must match setup_orders_tab + retranslateUi).
enum OrderCol {
    OC_Time = 0,
    OC_Symbol,
    OC_Side,
    OC_Type,
    OC_Product,
    OC_Qty,
    OC_Filled,
    OC_Price,
    OC_Trigger,
    OC_AvgFill,
    OC_Status,
    OC_OrderId,
    OC_Action,
    kOrdCols
};

// Zone of a naive (offset-less) broker timestamp. Indian brokers send the
// exchange's IST wall time; for any other venue the zone is not known.
QTimeZone eqbp_naive_order_zone(const QString& exchange) {
    const QString ex = exchange.trimmed().toUpper();
    static const QSet<QString> kIndia = {
        QStringLiteral("NSE"), QStringLiteral("BSE"), QStringLiteral("NFO"),    QStringLiteral("BFO"),
        QStringLiteral("MCX"), QStringLiteral("CDS"), QStringLiteral("BCD"),    QStringLiteral("NCDEX"),
        QStringLiteral("NCO"), QStringLiteral("NSE_EQ"), QStringLiteral("BSE_EQ"), QStringLiteral("NSE_FO"),
        QStringLiteral("BSE_FO"), QStringLiteral("NSE_CD"), QStringLiteral("MCX_FO"), QStringLiteral("NSE_INDEX"),
        QStringLiteral("BSE_INDEX")};
    if (kIndia.contains(ex))
        return QTimeZone(QByteArrayLiteral("Asia/Kolkata"));
    return {};
}

// A broker / paper order timestamp as an absolute instant, or invalid when the
// text is not understood. Brokers differ: ISO with Z / offset (Alpaca, Saxo),
// naive exchange wall time ("2024-01-15 09:15:32" Zerodha / Upstox / Dhan,
// "09:15:32 15-01-2024" Shoonya / Flattrade, "15-Jan-2024 09:15:32" Angel),
// epoch seconds / ms, or "/Date(ms+0530)/" (5paisa). A naive time is only
// placed when `naive_zone` is known — otherwise the caller shows the raw text
// rather than a time shifted by a guessed zone.
QDateTime eqbp_parse_order_time(const QString& raw, const QTimeZone& naive_zone) {
    const QString s = raw.trimmed();
    if (s.isEmpty())
        return {};
    static const QRegularExpression epoch_re(QStringLiteral("^\\d{10}(\\d{3})?$"));
    if (epoch_re.match(s).hasMatch()) {
        const qint64 v = s.toLongLong();
        return QDateTime::fromMSecsSinceEpoch(s.size() == 13 ? v : v * 1000, QTimeZone(QTimeZone::UTC));
    }
    static const QRegularExpression net_re(QStringLiteral("^/Date\\((-?\\d+)"));
    if (const auto m = net_re.match(s); m.hasMatch())
        return QDateTime::fromMSecsSinceEpoch(m.captured(1).toLongLong(), QTimeZone(QTimeZone::UTC));

    QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(s, Qt::ISODate);
    bool naive = dt.isValid() && dt.timeSpec() == Qt::LocalTime;
    if (!dt.isValid()) {
        static const char* const kNaiveFormats[] = {
            "yyyy-MM-dd HH:mm:ss", "yyyy-MM-dd HH:mm:ss.zzz", "yyyy/MM/dd HH:mm:ss", "dd-MM-yyyy HH:mm:ss",
            "dd/MM/yyyy HH:mm:ss", "HH:mm:ss dd-MM-yyyy",     "dd-MMM-yyyy HH:mm:ss", "dd MMM yyyy HH:mm:ss"};
        const QLocale c = QLocale::c();
        for (const char* f : kNaiveFormats) {
            dt = c.toDateTime(s, QString::fromLatin1(f));
            if (dt.isValid()) {
                naive = true;
                break;
            }
        }
    }
    if (!dt.isValid())
        return {};
    if (naive) {
        if (!naive_zone.isValid())
            return {};
        dt = QDateTime(dt.date(), dt.time(), naive_zone);
    }
    return dt;
}

// Time cell for the orders blotter: the display-zone time with seconds (full
// date-time in the tooltip) and its epoch ms as the sort key; raw text (sort
// key 0) when the timestamp could not be placed.
void eqbp_set_time_cell(QTableWidgetItem* item, const QString& raw, const QTimeZone& naive_zone,
                        const QTimeZone& exchange_tz) {
    namespace fmt = fincept::ui::formatting;
    const QDateTime dt = eqbp_parse_order_time(raw, naive_zone);
    if (dt.isValid()) {
        item->setText(fmt::time(dt, true, exchange_tz));
        item->setData(Qt::UserRole, static_cast<double>(dt.toMSecsSinceEpoch()));
        item->setToolTip(fmt::date_time(dt, fmt::DateStyle::Medium, true, exchange_tz));
    } else {
        item->setText(raw.trimmed().isEmpty() ? fmt::placeholder() : raw);
        item->setData(Qt::UserRole, 0.0);
        item->setToolTip(raw);
    }
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

// Gain / loss / flat colour: a P&L of exactly 0 is neither green nor red.
QColor pnl_qcolor(double v) {
    if (v > 0)
        return QColor(fincept::ui::colors::POSITIVE());
    if (v < 0)
        return QColor(fincept::ui::colors::NEGATIVE());
    return QColor(fincept::ui::colors::TEXT_SECONDARY());
}

// BUY / LONG green, SELL / SHORT red, anything else the default text colour.
QColor side_qcolor(const QString& side) {
    const QString s = side.trimmed().toLower();
    if (s == QLatin1String("buy") || s == QLatin1String("long") || s == QLatin1String("b"))
        return QColor(fincept::ui::colors::POSITIVE());
    if (s == QLatin1String("sell") || s == QLatin1String("short") || s == QLatin1String("s"))
        return QColor(fincept::ui::colors::NEGATIVE());
    return QColor(fincept::ui::colors::TEXT_PRIMARY());
}

// Right-aligned positions cell (the positions table stays unsorted: its live
// quote patch and row menu are index-aligned with the backing vectors).
void set_right_cell(QTableWidgetItem* it, const QString& text, const QColor& fg = QColor()) {
    it->setText(text);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (fg.isValid())
        it->setForeground(fg);
}

// Per-row action buttons used to carry an inline setStyleSheet each — a full CSS
// reparse per button per blotter refresh, on a table that ticks constantly. The
// variants now live in ONE panel-level sheet keyed off the `act` property, so a
// row rebuild only sets a dynamic property (CLAUDE.md P7).
QString table_btn_styles() {
    namespace c = fincept::ui::colors;
    return QString("QPushButton#eqTableBtn{border:1px solid %1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:0 6px;border-radius:2px;}"
                   "QPushButton#eqTableBtn[act=\"sell\"]{background:" + c::alpha(c::NEGATIVE, 0.15) + ";color:%2;}"
                   "QPushButton#eqTableBtn[act=\"sell\"]:hover{background:" + c::alpha(c::NEGATIVE, 0.30) + ";}"
                   "QPushButton#eqTableBtn[act=\"cover\"]{background:" + c::alpha(c::POSITIVE, 0.15) + ";color:%3;}"
                   "QPushButton#eqTableBtn[act=\"cover\"]:hover{background:" + c::alpha(c::POSITIVE, 0.30) + ";}"
                   "QPushButton#eqTableBtn[act=\"cnc\"]{background:" + c::alpha(c::INFO, 0.15) + ";color:%4;}"
                   "QPushButton#eqTableBtn[act=\"cnc\"]:hover{background:" + c::alpha(c::INFO, 0.30) + ";}"
                   "QPushButton#eqTableBtn[act=\"edit\"]{background:" + c::alpha(c::AMBER, 0.15) + ";color:%5;border-color:%6;}"
                   "QPushButton#eqTableBtn[act=\"edit\"]:hover{background:" + c::alpha(c::AMBER, 0.30) + ";}"
                   "QPushButton#eqTableBtn[act=\"cancel\"]{background:" + c::alpha(c::NEGATIVE, 0.12) + ";color:%2;border-color:%7;}"
                   "QPushButton#eqTableBtn[act=\"cancel\"]:hover{background:" + c::alpha(c::NEGATIVE, 0.28) + ";}")
        .arg(c::BORDER_MED(), c::NEGATIVE(), c::POSITIVE(), c::INFO(), c::AMBER(), c::AMBER_DIM(), c::NEGATIVE_DIM());
}

// Height of a per-row action button placed directly in a cell: 18 px at the
// default font size, growing with the button text (font-size px(-4) above).
int cell_button_height() {
    return std::max(18, QFontMetrics(fincept::ui::fonts::font(-4)).height() + 6);
}

// Exchange for the cross-screen "Open in…" row actions: what the broker reported for this
// symbol (positions, then holdings), else the account's broker default (paper rows carry none).
QString eqbp_row_exchange(const QVector<trading::BrokerPosition>& positions,
                          const QVector<trading::BrokerHolding>& holdings, const QString& symbol,
                          const QString& account_id) {
    for (const auto& p : positions)
        if (p.symbol == symbol && !p.exchange.isEmpty())
            return p.exchange;
    for (const auto& h : holdings)
        if (h.symbol == symbol && !h.exchange.isEmpty())
            return h.exchange;
    const auto account = trading::AccountManager::instance().get_account(account_id);
    if (auto* broker = trading::BrokerRegistry::instance().get(account.broker_id))
        return broker->profile().default_exchange;
    return {};
}

// nav.open_symbol hand-off (WindowFrame navigates, then delivers the symbol via IGroupLinked).
void eqbp_open_symbol(const QString& screen_id, const QString& symbol, const QString& exchange) {
    EventBus::instance().publish("nav.open_symbol",
                                 {{"screen_id", screen_id}, {"symbol", symbol}, {"exchange", exchange}});
}

// Selection + scroll survive a rebuild. setRowCount()/setItem() throw away the
// current row, so a trader mid-click on a position lost it on every refresh.
struct TableViewState {
    QString key;
    int scroll = 0;
};

TableViewState capture_view(QTableWidget* t, int key_col) {
    TableViewState s;
    if (!t)
        return s;
    if (auto* bar = t->verticalScrollBar())
        s.scroll = bar->value();
    const int row = t->currentRow();
    if (row >= 0)
        if (auto* it = t->item(row, key_col))
            s.key = it->text();
    return s;
}

void restore_view(QTableWidget* t, int key_col, const TableViewState& s) {
    if (!t)
        return;
    if (!s.key.isEmpty()) {
        for (int r = 0; r < t->rowCount(); ++r) {
            auto* it = t->item(r, key_col);
            if (it && it->text() == s.key) {
                t->selectRow(r); // selectRow() does not emit cellClicked
                break;
            }
        }
    }
    if (auto* bar = t->verticalScrollBar())
        bar->setValue(qMin(s.scroll, bar->maximum()));
}

/// Holds a row of summary cells without demanding their full width. A
/// QTabWidget is as wide as its WIDEST page, shown or not, so the Holdings
/// summary strip (six cells + three buttons, ~990 px) made the whole chart
/// column of the Equity Trading screen that wide — wider than a 1440 px window
/// once the watchlist had room for its tickers, which pushed the order column
/// past the window edge. Narrower than its cells, this scrolls them sideways: a
/// thin bar appears and the row grows by its height; wide enough, it looks and
/// behaves exactly like the plain row did.
class EqbpCellsScroller : public QScrollArea {
  public:
    explicit EqbpCellsScroller(QWidget* cells, const QString& object_name) {
        setObjectName(object_name);
        cells->setObjectName(object_name + QStringLiteral("Cells"));
        setWidget(cells);
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        // The strip paints the background; the global sheet would paint BG_BASE here.
        setStyleSheet(QStringLiteral("#%1, #%1 > QWidget, #%1Cells { background: transparent; border: none; }")
                          .arg(object_name));
        viewport()->setAutoFillBackground(false);
        cells->setAutoFillBackground(false);
        fit_height();
    }

  protected:
    void resizeEvent(QResizeEvent* event) override {
        QScrollArea::resizeEvent(event);
        update_bar(event->size().width());
    }
    void showEvent(QShowEvent* event) override {
        QScrollArea::showEvent(event);
        fit_height(); // style sheets (fonts) are applied by now
        update_bar(width());
    }
    bool eventFilter(QObject* obj, QEvent* event) override {
        // The cells' size changed (a value got longer, fonts restyled): follow it.
        if (obj == widget() && event->type() == QEvent::LayoutRequest)
            QTimer::singleShot(0, this, [this]() {
                fit_height();
                update_bar(width());
            });
        return QScrollArea::eventFilter(obj, event);
    }

  private:
    // Depends on the width only, so changing the height cannot loop.
    void update_bar(int w) {
        const bool need_bar = widget() && widget()->sizeHint().width() > w;
        if (need_bar == bar_shown_)
            return;
        bar_shown_ = need_bar;
        setHorizontalScrollBarPolicy(need_bar ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
        fit_height();
    }
    void fit_height() {
        const int cells_h = widget() ? widget()->sizeHint().height() : 0;
        setFixedHeight(cells_h + (bar_shown_ ? horizontalScrollBar()->sizeHint().height() : 0));
    }

    bool bar_shown_ = false;
};

} // namespace

EquityBottomPanel::EquityBottomPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("eqBottomPanel");
    // One sheet for every per-row action button (see table_btn_styles).
    setStyleSheet(table_btn_styles());
    // Item brushes and row heights keep the values they were given; carry them
    // over to the new appearance.
    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::theme_changed, this,
            [this](const fincept::ui::ThemeTokens&) {
                for (QTableWidget* t : {positions_table_, holdings_table_, orders_table_, auctions_table_,
                                        time_sales_table_, calendar_table_, fills_table_}) {
                    if (!t)
                        continue;
                    const bool tape = (t == auctions_table_ || t == time_sales_table_);
                    t->verticalHeader()->setDefaultSectionSize(table_row_height(-2, tape ? 20 : 22));
                    retheme_items(t);
                }
                if (holdings_table_) {
                    for (int r = 0; r < holdings_table_->rowCount(); ++r) {
                        if (QWidget* sell_btn = holdings_table_->cellWidget(r, 9))
                            sell_btn->setFixedHeight(cell_button_height());
                    }
                }
            });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Created in place and laid out before its tabs are built: adding a finished
    // tab widget to the panel re-computed the stylesheet of every table in it.
    tabs_ = new QTabWidget(this);
    layout->addWidget(tabs_);
    tabs_->setObjectName("eqBottomTabs");
    tabs_->setDocumentMode(true);
    // Show full tab labels; scroll (don't truncate) when they overflow the width.
    tabs_->setElideMode(Qt::ElideNone);
    tabs_->setUsesScrollButtons(true);

    // Blotter order: POSITIONS · HOLDINGS · ORDERS · FILLS · FUNDS · STATS, then the
    // broker-only tape tabs. Every index below is recorded from addTab().
    setup_positions_tab();
    setup_holdings_tab();
    setup_orders_tab();
    setup_fills_tab();
    setup_funds_tab();
    setup_stats_tab();
    setup_time_sales_tab();
    setup_auctions_tab();
    setup_calendar_tab();
    tabs_->setTabVisible(auctions_tab_idx_, false); // shown by set_auctions() once it has rows

    // Collapse toggle pinned to the tab-bar corner. Collapsing leaves only the
    // tab-bar header and returns the vertical space to the chart above.
    collapse_btn_ = new QToolButton;
    collapse_btn_->setObjectName("eqBottomCollapseBtn");
    collapse_btn_->setCursor(Qt::PointingHandCursor);
    collapse_btn_->setAutoRaise(true);
    collapse_btn_->setText(QStringLiteral("▾")); // ▾ (click to collapse)
    collapse_btn_->setToolTip(tr("Collapse panel"));
    collapse_btn_->setStyleSheet(
        QString("QToolButton{background:transparent;border:none;color:%1;font-size:" + fincept::ui::fonts::px(-2) + "px;padding:0 10px;}"
                "QToolButton:hover{color:%2;}")
            .arg(fincept::ui::colors::TEXT_SECONDARY(), fincept::ui::colors::TEXT_PRIMARY()));
    connect(collapse_btn_, &QToolButton::clicked, this, &EquityBottomPanel::toggle_collapsed);
    tabs_->setCornerWidget(collapse_btn_, Qt::TopRightCorner);

    // Clicking any tab while collapsed expands the sheet to that tab.
    connect(tabs_, &QTabWidget::tabBarClicked, this, [this](int) {
        if (collapsed_)
            toggle_collapsed();
    });

    // QTabWidget's page stack — hidden while collapsed so the panel shrinks to
    // the tab-bar height without fighting the pages' minimum heights.
    tab_content_ = tabs_->findChild<QStackedWidget*>();


    // Empty-state notes (inactive until set_empty_notice() supplies text).
    install_empty_state(positions_table_, false);
    install_empty_state(holdings_table_, false);
    install_empty_state(orders_table_, false);
    install_empty_state(time_sales_table_, true);
    install_empty_state(auctions_table_, true);
    install_empty_state(calendar_table_, true);

    // Settings > Currency & Number Format changed: re-format what is on screen from
    // the data already held (deferred, coalesced; once on the next show if hidden).
    fincept::ui::formatting::on_format_changed(this, [this]() { rerender_from_cache(); });
}

void EquityBottomPanel::rerender_from_cache() {
    if (is_paper_) {
        const QVector<trading::PtPosition> positions = last_paper_positions_;
        set_paper_positions(positions);
    } else {
        const QVector<trading::BrokerPosition> positions = last_positions_;
        set_positions(positions);
    }
    apply_money_headers();
    populate_holdings_view();
    if (orders_are_paper_) {
        const QVector<trading::PtOrder> orders = last_paper_orders_;
        set_paper_orders(orders);
    } else {
        const QVector<trading::BrokerOrderInfo> orders = last_orders_;
        set_orders(orders);
    }
    if (have_paper_trades_) {
        const QVector<trading::PtTrade> trades = last_paper_trades_;
        set_paper_trades(trades, last_trades_note_);
    }
    if (have_funds_view_)
        set_funds_view(last_funds_view_);
    if (have_stats_view_)
        set_stats_view(last_stats_view_);
}

void EquityBottomPanel::apply_money_headers() {
    if (!holdings_table_)
        return;
    namespace uf = fincept::ui::formatting;
    // Money columns of the holdings table carry the account currency in the header;
    // their cells are bare numbers (prices keep their own currency per row).
    const QString ccy = currency_.isEmpty() ? QStringLiteral("-") : currency_;
    const QList<QPair<int, QString>> cols = {
        {4, uf::header(tr("Invested"), ccy)}, {5, uf::header(tr("Current"), ccy)},
        {6, uf::header(tr("P&L"), ccy)},      {8, uf::header(tr("Day P&L"), ccy)}};
    for (const auto& c : cols) {
        QTableWidgetItem* h = holdings_table_->horizontalHeaderItem(c.first);
        if (!h)
            holdings_table_->setHorizontalHeaderItem(c.first, new QTableWidgetItem(c.second));
        else if (h->text() != c.second)
            h->setText(c.second);
    }
}

// ── Empty states ─────────────────────────────────────────────────────────────

EquityBottomPanel::~EquityBottomPanel() {
    // The empty-state slots are driven by each table's model signals, and a
    // QTableWidget's model emits modelReset from its own destructor — while the
    // table is being torn down and after its overlay label is gone. Left
    // connected, refresh_empty_state() then ran on those dead widgets and the
    // app crashed on exit (SIGSEGV in QWidget::setGeometry). Cut the connections
    // here, before any child is destroyed.
    for (const auto& s : empty_states_) {
        if (s.table && s.table->model())
            disconnect(s.table->model(), nullptr, this, nullptr);
    }
}

void EquityBottomPanel::install_empty_state(QTableWidget* table, bool tape) {
    if (!table)
        return;
    auto* label = new QLabel(table);
    label->setObjectName(QStringLiteral("eqEmptyState"));
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setAttribute(Qt::WA_TransparentForMouseEvents); // the table stays interactive
    label->setStyleSheet(QString("QLabel#eqEmptyState{background:transparent;border:none;color:%1;"
                                 "font-size:" + fincept::ui::fonts::px(-3) + "px;padding:12px 24px;}")
                             .arg(fincept::ui::colors::TEXT_SECONDARY()));
    label->hide();
    table->viewport()->installEventFilter(this);
    const EmptyState s{table, label, tape};
    empty_states_.append(s);
    // setRowCount() / clearContents() surface as row inserts / removals / resets.
    auto* model = table->model();
    connect(model, &QAbstractItemModel::rowsInserted, this, [this, s]() { refresh_empty_state(s); });
    connect(model, &QAbstractItemModel::rowsRemoved, this, [this, s]() { refresh_empty_state(s); });
    connect(model, &QAbstractItemModel::modelReset, this, [this, s]() { refresh_empty_state(s); });
}

void EquityBottomPanel::refresh_empty_state(const EmptyState& s) {
    const QString& text = s.tape ? tape_notice_ : account_notice_;
    const bool show = !text.isEmpty() && s.table->rowCount() == 0;
    if (show) {
        if (s.label->text() != text)
            s.label->setText(text);
        s.label->setGeometry(s.table->viewport()->geometry());
        s.label->raise();
    }
    s.label->setVisible(show);
}

void EquityBottomPanel::set_empty_notice(const QString& account_text, const QString& tape_text) {
    if (account_text == account_notice_ && tape_text == tape_notice_)
        return;
    account_notice_ = account_text;
    tape_notice_ = tape_text;
    for (const auto& s : empty_states_)
        refresh_empty_state(s);
}

bool EquityBottomPanel::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
        for (const auto& s : empty_states_) {
            if (s.table->viewport() == obj) {
                if (s.label->isVisible())
                    s.label->setGeometry(s.table->viewport()->geometry());
                break;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void EquityBottomPanel::add_extra_tab(QWidget* page, const QString& title) {
    if (!page || !tabs_ || tabs_->indexOf(page) >= 0)
        return;
    // The tab stack takes the largest page's size: a big page (the all-accounts
    // monitor) must not raise the blotter's height for every tab.
    page->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    tabs_->addTab(page, fincept::ui::literal_ampersands(title));
}

void EquityBottomPanel::set_extra_tab_visible(QWidget* page, bool visible) {
    const int idx = tabs_ ? tabs_->indexOf(page) : -1;
    if (idx >= 0)
        tabs_->setTabVisible(idx, visible);
}

void EquityBottomPanel::show_extra_tab(QWidget* page) {
    const int idx = tabs_ ? tabs_->indexOf(page) : -1;
    if (idx < 0)
        return;
    tabs_->setTabVisible(idx, true);
    tabs_->setCurrentIndex(idx);
    if (collapsed_)
        toggle_collapsed();
}

bool EquityBottomPanel::show_section(const QString& id) {
    if (!tabs_)
        return false;
    const QString k = id.trimmed().toLower();
    const int idx = k == QLatin1String("positions") ? positions_tab_idx_
                    : k == QLatin1String("holdings")  ? holdings_tab_idx_
                    : k == QLatin1String("orders")    ? orders_tab_idx_
                    : k == QLatin1String("fills")     ? fills_tab_idx_
                    : k == QLatin1String("funds")     ? funds_tab_idx_
                    : k == QLatin1String("stats")     ? stats_tab_idx_
                                                      : -1;
    if (idx < 0 || idx >= tabs_->count() || !tabs_->isTabVisible(idx))
        return false;
    tabs_->setCurrentIndex(idx);
    if (collapsed_)
        toggle_collapsed();
    return true;
}

void EquityBottomPanel::toggle_collapsed() {
    collapsed_ = !collapsed_;
    apply_collapsed();
}

void EquityBottomPanel::apply_collapsed() {
    auto* splitter = qobject_cast<QSplitter*>(parentWidget());
    const int header_h = tabs_->tabBar()->sizeHint().height();
    if (collapsed_) {
        if (splitter)
            saved_split_sizes_ = splitter->sizes();
        if (tab_content_)
            tab_content_->hide();
        setMaximumHeight(header_h);
        collapse_btn_->setText(QStringLiteral("▴")); // ▴ (click to expand)
        collapse_btn_->setToolTip(tr("Expand panel"));
    } else {
        setMaximumHeight(QWIDGETSIZE_MAX);
        if (tab_content_)
            tab_content_->show();
        collapse_btn_->setText(QStringLiteral("▾")); // ▾
        collapse_btn_->setToolTip(tr("Collapse panel"));
        if (splitter && !saved_split_sizes_.isEmpty())
            splitter->setSizes(saved_split_sizes_);
    }
}

void EquityBottomPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityBottomPanel::retranslateUi() {
    // Tab labels
    if (tabs_) {
        if (positions_tab_idx_ >= 0)
            tabs_->setTabText(positions_tab_idx_, tr("POSITIONS"));
        if (holdings_tab_idx_ >= 0)
            tabs_->setTabText(holdings_tab_idx_, tr("HOLDINGS"));
        if (orders_tab_idx_ >= 0)
            tabs_->setTabText(orders_tab_idx_, tr("ORDERS"));
        if (funds_tab_idx_ >= 0)
            tabs_->setTabText(funds_tab_idx_, tr("FUNDS"));
        if (stats_tab_idx_ >= 0)
            tabs_->setTabText(stats_tab_idx_, tr("STATS"));
        if (time_sales_tab_idx_ >= 0)
            tabs_->setTabText(time_sales_tab_idx_, fincept::ui::literal_ampersands(tr("TIME & SALES")));
        if (auctions_tab_idx_ >= 0)
            tabs_->setTabText(auctions_tab_idx_, tr("AUCTIONS"));
        if (calendar_tab_idx_ >= 0)
            tabs_->setTabText(calendar_tab_idx_, tr("CALENDAR"));
        if (fills_tab_idx_ >= 0)
            tabs_->setTabText(fills_tab_idx_, tr("FILLS"));
    }
    if (fills_table_)
        fills_table_->setHorizontalHeaderLabels(
            {tr("Time"), tr("Symbol"), tr("Side"), tr("Qty"), tr("Price"), tr("Fee"), tr("Realized P&L"), tr("Note")});

    // Table headers. Column 2 shows the open time in paper mode and the exchange
    // in live mode — the data setters own it, so mirror the current mode here.
    if (positions_table_)
        positions_table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Product"),
                                                     is_paper_ ? tr("Opened") : tr("Exchange"), tr("Side"), tr("Qty"),
                                                     tr("Avg Price"), tr("LTP"), tr("P&L"), tr("P&L %"), tr("Action")});
    if (holdings_table_) {
        holdings_table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Qty"), tr("Avg Price"), tr("LTP"), tr("Invested"),
                                                    tr("Current"), tr("P&L"), tr("P&L %"), tr("Day P&L"),
                                                    tr("Action")});
        apply_money_headers(); // + the account currency on the money columns
    }
    if (orders_table_)
        orders_table_->setHorizontalHeaderLabels({tr("Time"), tr("Symbol"), tr("Side"), tr("Type"), tr("Product"),
                                                  tr("Qty"), tr("Filled"), tr("Price"), tr("Trigger"), tr("Avg Fill"),
                                                  tr("Status"), tr("Order ID"), tr("Action")});
    if (auctions_table_)
        auctions_table_->setHorizontalHeaderLabels(
            {tr("Date"), tr("Type"), tr("Time"), tr("Price"), tr("Size"), tr("Exchange")});
    if (time_sales_table_)
        time_sales_table_->setHorizontalHeaderLabels(
            {tr("Time"), tr("Price"), tr("Size"), tr("Exchange"), tr("Conditions"), tr("Tape")});
    if (calendar_table_)
        calendar_table_->setHorizontalHeaderLabels(
            {tr("Date"), tr("Open (ET)"), tr("Close (ET)"), tr("Pre-Market"), tr("After-Hours")});

    // Action buttons
    if (close_all_btn_)
        close_all_btn_->setText(tr("SQUARE OFF ALL"));
    if (cancel_all_btn_)
        cancel_all_btn_->setText(tr("CANCEL ALL ORDERS"));
    if (holdings_import_btn_)
        holdings_import_btn_->setText(tr("IMPORT TO PORTFOLIO"));
    if (holdings_square_off_btn_)
        holdings_square_off_btn_->setText(tr("SQUARE OFF ALL"));

    // Holdings summary-strip captions
    if (holdings_count_caption_)
        holdings_count_caption_->setText(tr("HOLDINGS"));
    if (holdings_invested_caption_)
        holdings_invested_caption_->setText(tr("INVESTED"));
    if (holdings_current_caption_)
        holdings_current_caption_->setText(tr("CURRENT"));
    if (holdings_pnl_caption_)
        holdings_pnl_caption_->setText(tr("TOTAL P&L"));
    if (holdings_day_pnl_caption_)
        holdings_day_pnl_caption_->setText(tr("TODAY'S P&L"));
    if (holdings_pnl_pct_caption_)
        holdings_pnl_pct_caption_->setText(tr("RETURN %"));

    // Funds card captions
    const QString fund_caps[kFundCards] = {tr("AVAILABLE BALANCE"), tr("USED MARGIN"),    tr("TOTAL EQUITY"),
                                           tr("REALIZED P&L"),      tr("UNREALIZED P&L"), tr("HOLDINGS VALUE"),
                                           tr("OPENING BALANCE"),   tr("MARGIN USED %"),  tr("COLLATERAL")};
    for (int i = 0; i < kFundCards; ++i)
        if (fund_card_cap_[i])
            fund_card_cap_[i]->setText(fund_caps[i]);

    // Stats card captions
    const QString stat_caps[kStatCards] = {
        tr("NET P&L"), tr("TODAY'S P&L"), tr("RETURN %"), tr("WIN RATE"),    tr("PROFIT FACTOR"), tr("TOTAL TRADES"),
        tr("AVG WIN"), tr("AVG LOSS"),    tr("TURNOVER"), tr("LARGEST WIN"), tr("LARGEST LOSS"),  tr("TOTAL CHARGES")};
    for (int i = 0; i < kStatCards; ++i)
        if (stat_card_cap_[i])
            stat_card_cap_[i]->setText(stat_caps[i]);

    // The calendar clock banner ("● MARKET OPEN/CLOSED/--") is live data driven
    // by set_clock(); it re-renders in the new language on the next clock tick,
    // so it is intentionally not reapplied here.
}

QTableWidgetItem* EquityBottomPanel::ensure_item(QTableWidget* table, int row, int col) {
    auto* item = table->item(row, col);
    if (!item) {
        item = new QTableWidgetItem;
        item->setForeground(QColor(fincept::ui::colors::TEXT_PRIMARY()));
        table->setItem(row, col, item);
    }
    return item;
}

// ── Positions Tab ──────────────────────────────────────────────────────────

void EquityBottomPanel::setup_positions_tab() {
    auto* container = new QWidget;
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);

    // Action bar with Square Off All button
    auto* action_bar = new QWidget;
    action_bar->setObjectName("eqPositionsActionBar");
    action_bar->setStyleSheet(QString("#eqPositionsActionBar{background:%1;border-bottom:1px solid %2;}")
                                  .arg(fincept::ui::colors::PANEL(), fincept::ui::colors::BORDER_MED()));
    auto* bar_layout = new QHBoxLayout(action_bar);
    bar_layout->setContentsMargins(12, 4, 12, 4);
    bar_layout->setSpacing(10);

    // Net total P&L across the open (intraday) positions.
    positions_total_pnl_label_ = new QLabel(tr("Total P&L  --"));
    positions_total_pnl_label_->setStyleSheet(
        QString("font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;color:%1;").arg(fincept::ui::colors::TEXT_PRIMARY()));
    bar_layout->addWidget(positions_total_pnl_label_);

    // Small square-off button for a P&L group (winners / losers).
    auto make_group_btn = [](const QString& color, const QString& tip) {
        auto* b = new QPushButton(QString::fromUtf8("✕"));
        b->setCursor(Qt::PointingHandCursor);
        b->setEnabled(false);
        b->setMinimumHeight(18);
        b->setToolTip(tip);
        b->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                                 "padding:0 6px;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;border-radius:2px;}"
                                 "QPushButton:hover:enabled{background:%1;color:%3;}"
                                 "QPushButton:disabled{color:%2;border-color:%2;}")
                             .arg(color, fincept::ui::colors::TEXT_SECONDARY(), fincept::ui::colors::TEXT_ON_ACCENT()));
        return b;
    };

    // Winners group: count + sum, with a square-off-profits button.
    positions_win_label_ = new QLabel(QString::fromUtf8("▲ 0"));
    positions_win_label_->setStyleSheet(
        QString("font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:600;color:%1;").arg(fincept::ui::colors::POSITIVE()));
    bar_layout->addWidget(positions_win_label_);
    win_squareoff_btn_ = make_group_btn(fincept::ui::colors::POSITIVE(), tr("Square off all winning positions"));
    win_squareoff_btn_->setProperty("fincept_agent_risk", "order");
    connect(win_squareoff_btn_, &QPushButton::clicked, this, [this]() { request_square_off_group(1); });
    bar_layout->addWidget(win_squareoff_btn_);

    // Losers group: count + sum, with a cut-losses button.
    positions_loss_label_ = new QLabel(QString::fromUtf8("▼ 0"));
    positions_loss_label_->setStyleSheet(
        QString("font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:600;color:%1;").arg(fincept::ui::colors::NEGATIVE()));
    bar_layout->addWidget(positions_loss_label_);
    loss_squareoff_btn_ = make_group_btn(fincept::ui::colors::NEGATIVE(), tr("Square off all losing positions"));
    loss_squareoff_btn_->setProperty("fincept_agent_risk", "order");
    connect(loss_squareoff_btn_, &QPushButton::clicked, this, [this]() { request_square_off_group(-1); });
    bar_layout->addWidget(loss_squareoff_btn_);

    bar_layout->addStretch(1);

    close_all_btn_ = new QPushButton(tr("SQUARE OFF ALL"));
    close_all_btn_->setProperty("fincept_agent_risk", "order");
    close_all_btn_->setCursor(Qt::PointingHandCursor);
    close_all_btn_->setStyleSheet(
        QString("QPushButton{background:%3;color:%1;border:1px solid %2;"
                "padding:4px 14px;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;letter-spacing:0.5px;border-radius:2px;}"
                "QPushButton:hover{background:%4;}")
            .arg(fincept::ui::colors::NEGATIVE(), fincept::ui::colors::NEGATIVE_DIM(),
                 fincept::ui::colors::alpha(fincept::ui::colors::NEGATIVE, 0.12),
                 fincept::ui::colors::alpha(fincept::ui::colors::NEGATIVE, 0.25)));
    connect(close_all_btn_, &QPushButton::clicked, this, [this]() {
        if (account_id_.isEmpty())
            return;
        auto answer = QMessageBox::warning(this, tr("Square Off All Positions"),
                                           tr("This will close ALL open positions.\n\nAre you sure?"),
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        emit close_all_positions_requested(account_id_);
    });
    bar_layout->addWidget(close_all_btn_);
    vlay->addWidget(action_bar);

    positions_table_ = new QTableWidget;
    positions_table_->setObjectName("eqTable");
    positions_table_->setColumnCount(10);
    positions_table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Product"), tr("Opened"), tr("Side"), tr("Qty"),
                                                 tr("Avg Price"), tr("LTP"), tr("P&L"), tr("P&L %"), tr("Action")});
    positions_table_->verticalHeader()->setVisible(false);
    positions_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    positions_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    positions_table_->setShowGrid(false);
    positions_table_->horizontalHeader()->setStretchLastSection(true);
    positions_table_->verticalHeader()->setDefaultSectionSize(table_row_height(-2, 22));

    // Right-click a row → Buy (add) / Sell (reduce) for that symbol. Symbol +
    // product are read from the row items so it stays correct regardless of order.
    positions_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(positions_table_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& p) {
        const int row = positions_table_->rowAt(p.y());
        if (row < 0)
            return;
        auto* sym_item = positions_table_->item(row, 0);
        if (!sym_item || sym_item->text().isEmpty())
            return;
        const QString symbol = sym_item->text();
        auto* prod_item = positions_table_->item(row, 1);
        QString product = prod_item && !prod_item->text().isEmpty() ? prod_item->text() : QStringLiteral("MIS");
        QMenu menu(this);
        // Direction-aware exit: paper shorts render side "SHORT" with positive qty,
        // live broker shorts a signed qty and/or side "SHORT"/"SELL" (col 3).
        auto* side_item = positions_table_->item(row, 3); // Side column
        const QString side_txt = side_item ? side_item->text() : QString();
        auto* qty_item = positions_table_->item(row, 4); // Qty column
        double raw_qty = qty_item ? qty_item->text().toDouble() : 0.0;
        // The row's own exchange / product / unrounded quantity from the row-aligned
        // backing data (this table is not sortable). The cells are display forms: "--"
        // for a missing product, the quantity rounded for display.
        QString row_exchange;
        if (!is_paper_ && row < last_positions_.size() && last_positions_[row].symbol == symbol) {
            const auto& bp = last_positions_[row];
            product = bp.product_type; // broker wire spelling; the ticket maps it per broker
            row_exchange = bp.exchange;
            raw_qty = bp.quantity;
        } else if (is_paper_ && row < last_paper_positions_.size() && last_paper_positions_[row].symbol == symbol) {
            product = last_paper_positions_[row].product;
            raw_qty = last_paper_positions_[row].quantity;
        }
        const bool is_short = raw_qty < 0 || side_txt.contains(QStringLiteral("SHORT"), Qt::CaseInsensitive) ||
                              side_txt.compare(QStringLiteral("SELL"), Qt::CaseInsensitive) == 0;
        const double held = qAbs(raw_qty);
        QAction* buy = menu.addAction(tr("Buy / Add  %1").arg(symbol));
        QAction* sell =
            menu.addAction(is_short ? tr("Buy / Cover  %1").arg(symbol) : tr("Sell / Reduce  %1").arg(symbol));
        // Hand the ticker to Equity Research / News (derivative contracts have neither → no entry).
        const QString exchange = eqbp_row_exchange(last_positions_, last_holdings_, symbol, account_id_);
        const QString ticker = research_ticker_for(symbol, exchange);
        QAction* research = nullptr;
        QAction* news = nullptr;
        if (!ticker.isEmpty()) {
            menu.addSeparator();
            research = menu.addAction(tr("Open %1 in Equity Research").arg(symbol));
            news = menu.addAction(tr("News for %1").arg(symbol));
        }
        const QString trade_exchange = row_exchange.isEmpty() ? exchange : row_exchange;
        QAction* chosen = menu.exec(positions_table_->viewport()->mapToGlobal(p));
        if (!chosen)
            return;
        if (chosen == buy)
            emit trade_symbol_requested(symbol, trade_exchange, product, true, 0.0); // add → ticket defaults to 1
        else if (chosen == sell)
            emit trade_symbol_requested(symbol, trade_exchange, product, is_short, held); // exit → held qty
        else if (chosen == research)
            eqbp_open_symbol(QStringLiteral("equity_research"), ticker, exchange);
        else if (chosen == news)
            eqbp_open_symbol(QStringLiteral("news"), symbol, exchange);
    });

    // Left-click a position row → load that symbol's chart (parity with the
    // watchlist). Clicks on the Action cell-widget (col 9) go to its buttons, not
    // here, so the SELL button keeps working.
    connect(positions_table_, &QTableWidget::cellClicked, this, [this](int row, int col) {
        auto* sym_item = positions_table_->item(row, 0);
        LOG_DEBUG("EquityTrading", QString("positions cellClicked row=%1 col=%2 sym='%3'")
                               .arg(row)
                               .arg(col)
                               .arg(sym_item ? sym_item->text() : QStringLiteral("<null>")));
        if (sym_item && !sym_item->text().isEmpty())
            emit chart_symbol_requested(sym_item->text());
    });

    vlay->addWidget(positions_table_, 1);

    positions_tab_idx_ = tabs_->addTab(container, tr("POSITIONS"));
}

// ── Holdings Tab ───────────────────────────────────────────────────────────

void EquityBottomPanel::setup_holdings_tab() {
    // Only the page shell here. Its contents — summary strip, three action
    // buttons, the table — were the single largest cost of opening Equity
    // Trading (0.5 s and more), for a tab most opens never look at. They are
    // built in place, inside this page, the first time the tab becomes current.
    holdings_page_ = new QWidget;
    auto* v = new QVBoxLayout(holdings_page_);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    holdings_tab_idx_ = tabs_->addTab(holdings_page_, tr("HOLDINGS"));
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == holdings_tab_idx_)
            build_holdings_tab();
    });
}

void EquityBottomPanel::build_holdings_tab() {
    if (holdings_table_ || !holdings_page_)
        return;
    auto* v = qobject_cast<QVBoxLayout*>(holdings_page_->layout());
    if (!v)
        return;
    // A page shown for the first time while being filled would paint half-built.
    holdings_page_->setUpdatesEnabled(false);

    // Summary strip across the top of the Holdings tab.
    auto* strip = new QWidget;
    strip->setObjectName("eqHoldingsStrip");
    strip->setStyleSheet(QString("#eqHoldingsStrip{background:%1;border-bottom:1px solid %2;}")
                             .arg(fincept::ui::colors::PANEL(), fincept::ui::colors::BORDER_MED()));
    auto* strip_layout = new QHBoxLayout(strip);
    strip_layout->setContentsMargins(12, 6, 12, 6);
    strip_layout->setSpacing(24);

    // The six summary cells scroll sideways when the column is narrower than
    // them (EqbpCellsScroller); the action buttons stay in view on the right.
    auto* cells = new QWidget;
    auto* cells_layout = new QHBoxLayout(cells);
    cells_layout->setContentsMargins(0, 0, 0, 0);
    cells_layout->setSpacing(24);

    auto make_stat = [&](const QString& caption, QLabel*& caption_out) -> QLabel* {
        auto* cell = new QWidget;
        auto* cv = new QVBoxLayout(cell);
        cv->setContentsMargins(0, 0, 0, 0);
        cv->setSpacing(1);
        auto* cap = new QLabel(caption);
        cap->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;letter-spacing:0.5px;").arg(fincept::ui::colors::TEXT_SECONDARY()));
        caption_out = cap;
        auto* val = new QLabel("--");
        val->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-1) + "px;font-weight:700;").arg(fincept::ui::colors::TEXT_PRIMARY()));
        cv->addWidget(cap);
        cv->addWidget(val);
        cells_layout->addWidget(cell);
        return val;
    };
    holdings_count_label_ = make_stat(tr("HOLDINGS"), holdings_count_caption_);
    holdings_invested_label_ = make_stat(tr("INVESTED"), holdings_invested_caption_);
    holdings_current_label_ = make_stat(tr("CURRENT"), holdings_current_caption_);
    holdings_pnl_label_ = make_stat(tr("TOTAL P&L"), holdings_pnl_caption_);
    holdings_day_pnl_label_ = make_stat(tr("TODAY'S P&L"), holdings_day_pnl_caption_);
    holdings_pnl_pct_label_ = make_stat(tr("RETURN %"), holdings_pnl_pct_caption_);
    cells_layout->addStretch(1);
    strip_layout->addWidget(new EqbpCellsScroller(cells, QStringLiteral("eqHoldingsStats")), 1);

    holdings_import_btn_ = new QPushButton(tr("IMPORT TO PORTFOLIO"));
    holdings_import_btn_->setCursor(Qt::PointingHandCursor);
    holdings_import_btn_->setEnabled(false);
    holdings_import_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:4px 12px;"
                "font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;letter-spacing:0.5px;}"
                "QPushButton:hover{background:%4;color:%6;}"
                "QPushButton:disabled{color:%5;border-color:%3;}")
            .arg(fincept::ui::colors::PANEL(), fincept::ui::colors::ORANGE(), fincept::ui::colors::BORDER_MED(),
                 fincept::ui::colors::ORANGE(), fincept::ui::colors::TEXT_SECONDARY(),
                 fincept::ui::colors::TEXT_ON_ACCENT()));
    connect(holdings_import_btn_, &QPushButton::clicked, this, [this]() {
        if (!last_holdings_.isEmpty())
            emit import_holdings_requested(last_holdings_);
    });
    strip_layout->addWidget(holdings_import_btn_);

    holdings_replicate_btn_ = new QPushButton(tr("REPLICATE → PAPER"));
    holdings_replicate_btn_->setCursor(Qt::PointingHandCursor);
    holdings_replicate_btn_->setStyleSheet(holdings_import_btn_->styleSheet());
    connect(holdings_replicate_btn_, &QPushButton::clicked, this, [this]() { emit replicate_portfolio_requested(); });
    strip_layout->addWidget(holdings_replicate_btn_);

    // SQUARE OFF ALL — sells every holding (delivery/CNC) at market. Mirrors the
    // Positions tab button, but acts ONLY on holdings; positions are untouched.
    holdings_square_off_btn_ = new QPushButton(tr("SQUARE OFF ALL"));
    holdings_square_off_btn_->setProperty("fincept_agent_risk", "order");
    holdings_square_off_btn_->setCursor(Qt::PointingHandCursor);
    holdings_square_off_btn_->setEnabled(false);
    holdings_square_off_btn_->setStyleSheet(
        QString("QPushButton{background:%4;color:%1;border:1px solid %2;"
                "padding:4px 14px;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;letter-spacing:0.5px;border-radius:2px;}"
                "QPushButton:hover{background:%5;}"
                "QPushButton:disabled{color:%3;border-color:%2;}")
            .arg(fincept::ui::colors::NEGATIVE(), fincept::ui::colors::NEGATIVE_DIM(),
                 fincept::ui::colors::TEXT_SECONDARY(), fincept::ui::colors::alpha(fincept::ui::colors::NEGATIVE, 0.12),
                 fincept::ui::colors::alpha(fincept::ui::colors::NEGATIVE, 0.25)));
    connect(holdings_square_off_btn_, &QPushButton::clicked, this, [this]() {
        // The screen logs the request (Holdings square-off requested) and each exit is audited.
        LOG_DEBUG("EquityTrading", QString("[panel] SQUARE OFF ALL clicked: holdings=%1").arg(last_holdings_.size()));
        if (account_id_.isEmpty() || last_holdings_.isEmpty())
            return;
        // Snapshot what the confirmation lists: the modal loop keeps delivering holdings
        // pushes, and the orders must be exactly the ones the user agreed to.
        const QVector<trading::BrokerHolding> snapshot = last_holdings_;
        constexpr int kMaxListed = 20;
        QStringList lines;
        int sellable = 0;
        for (const auto& h : snapshot) {
            if (!(h.quantity > 0.0))
                continue;
            if (sellable < kMaxListed)
                lines << QString::fromUtf8("• SELL %1 × %2 (%3)")
                             .arg(format_quantity(h.quantity), h.symbol,
                                  h.exchange.isEmpty() ? tr("broker default exchange") : h.exchange);
            ++sellable;
        }
        if (sellable == 0)
            return;
        const QString more = sellable > kMaxListed ? tr("\n… and %1 more").arg(sellable - kMaxListed) : QString();
        auto answer = QMessageBox::warning(this, tr("Square Off All Holdings"),
                                           tr("This will place delivery (CNC) MARKET SELL orders to exit ALL %1 "
                                              "holding(s):\n\n%2%3\n\nPositions are NOT affected. Are you sure?")
                                               .arg(QString::number(sellable), lines.join(QLatin1Char('\n')), more),
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        emit square_off_all_holdings_requested(snapshot);
    });
    strip_layout->addWidget(holdings_square_off_btn_);

    v->addWidget(strip);

    holdings_table_ = new QTableWidget;
    holdings_table_->setObjectName("eqTable");
    holdings_table_->setColumnCount(10);
    holdings_table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Qty"), tr("Avg Price"), tr("LTP"), tr("Invested"),
                                                tr("Current"), tr("P&L"), tr("P&L %"), tr("Day P&L"), tr("Action")});
    apply_money_headers();
    holdings_table_->verticalHeader()->setVisible(false);
    holdings_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    holdings_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    holdings_table_->setShowGrid(false);
    holdings_table_->horizontalHeader()->setStretchLastSection(true);
    holdings_table_->verticalHeader()->setDefaultSectionSize(table_row_height(-2, 22));
    holdings_table_->setAlternatingRowColors(true);
    holdings_table_->setSortingEnabled(true);
    // Smooth pixel-based scrolling (default is per-item, which feels janky).
    holdings_table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    holdings_table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    holdings_table_->verticalScrollBar()->setSingleStep(8);
    holdings_table_->horizontalScrollBar()->setSingleStep(16);

    // Right-click a holding → Buy (add) / Sell (reduce). Holdings are delivery, so
    // the resulting order uses the CNC product.
    holdings_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(holdings_table_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& p) {
        const int row = holdings_table_->rowAt(p.y());
        if (row < 0)
            return;
        auto* sym_item = holdings_table_->item(row, 0);
        if (!sym_item || sym_item->text().isEmpty())
            return;
        const QString symbol = sym_item->text();
        QMenu menu(this);
        QAction* buy = menu.addAction(tr("Buy / Add  %1").arg(symbol));
        QAction* sell = menu.addAction(tr("Sell / Reduce  %1").arg(symbol));
        auto* qty_item = holdings_table_->item(row, 1); // Qty column
        // Raw quantity (UserRole) — the text is a display form; a fractional holding
        // must pre-fill the ticket unrounded.
        const double held = qty_item ? qAbs(qty_item->data(Qt::UserRole).toDouble()) : 0.0;
        const QString exchange = eqbp_row_exchange(last_positions_, last_holdings_, symbol, account_id_);
        // Delivery sell goes to the HOLDING's exchange (a same-symbol position on another
        // exchange must not redirect it); the broker default when the adapter reports none.
        const QString hold_exchange = eqbp_row_exchange({}, last_holdings_, symbol, account_id_);
        const QString ticker = research_ticker_for(symbol, exchange);
        QAction* research = nullptr;
        QAction* news = nullptr;
        if (!ticker.isEmpty()) {
            menu.addSeparator();
            research = menu.addAction(tr("Open %1 in Equity Research").arg(symbol));
            news = menu.addAction(tr("News for %1").arg(symbol));
        }
        QAction* chosen = menu.exec(holdings_table_->viewport()->mapToGlobal(p));
        if (!chosen)
            return;
        if (chosen == buy)
            emit trade_symbol_requested(symbol, hold_exchange, QStringLiteral("CNC"), true, 0.0);
        else if (chosen == sell)
            emit trade_symbol_requested(symbol, hold_exchange, QStringLiteral("CNC"), false, held);
        else if (chosen == research)
            eqbp_open_symbol(QStringLiteral("equity_research"), ticker, exchange);
        else if (chosen == news)
            eqbp_open_symbol(QStringLiteral("news"), symbol, exchange);
    });

    v->addWidget(holdings_table_, 1);

    // Empty-state note (the notice may have been set before the tab existed).
    install_empty_state(holdings_table_, false);
    if (!empty_states_.isEmpty() && empty_states_.constLast().table == holdings_table_)
        refresh_empty_state(empty_states_.constLast());

    // Data that arrived while the tab was not built yet.
    holdings_import_btn_->setEnabled(!last_holdings_.isEmpty());
    holdings_square_off_btn_->setEnabled(!last_holdings_.isEmpty());
    if (!last_holdings_.isEmpty())
        populate_holdings_view();

    holdings_page_->setUpdatesEnabled(true);
}

// ── Orders Tab ─────────────────────────────────────────────────────────────

void EquityBottomPanel::setup_orders_tab() {
    auto* container = new QWidget;
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);

    // Action bar with Cancel All button
    auto* action_bar = new QWidget;
    action_bar->setObjectName("eqOrdersActionBar");
    action_bar->setStyleSheet(QString("#eqOrdersActionBar{background:%1;border-bottom:1px solid %2;}")
                                  .arg(fincept::ui::colors::PANEL(), fincept::ui::colors::BORDER_MED()));
    auto* bar_layout = new QHBoxLayout(action_bar);
    bar_layout->setContentsMargins(12, 4, 12, 4);
    bar_layout->setSpacing(8);

    // Per-day order book: pick a date to view that IST day's orders (default today,
    // so each session starts empty instead of accumulating every past order).
    auto* date_lbl = new QLabel(tr("DATE"));
    date_lbl->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:600;letter-spacing:0.5px;")
                                .arg(fincept::ui::colors::TEXT_SECONDARY()));
    orders_date_edit_ = new QDateEdit(QDate::currentDate());
    orders_date_edit_->setObjectName("eqOrdersDate");
    orders_date_edit_->setCalendarPopup(true);
    orders_date_edit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    orders_date_edit_->setMaximumDate(QDate::currentDate());
    orders_date_edit_->setStyleSheet(
        QString(
            "QDateEdit{background:%1;color:%2;border:1px solid %3;padding:2px 6px;font-size:" + fincept::ui::fonts::px(-3) + "px;border-radius:2px;}")
            .arg(fincept::ui::colors::BG_BASE(), fincept::ui::colors::TEXT_PRIMARY(), fincept::ui::colors::BORDER_MED()));
    connect(orders_date_edit_, &QDateEdit::dateChanged, this, [this](const QDate& d) {
        if (!suppress_orders_date_signal_)
            emit orders_day_changed(d);
    });
    bar_layout->addWidget(date_lbl);
    bar_layout->addWidget(orders_date_edit_);
    bar_layout->addStretch(1);

    cancel_all_btn_ = new QPushButton(tr("CANCEL ALL ORDERS"));
    cancel_all_btn_->setProperty("fincept_agent_risk", "order");
    cancel_all_btn_->setCursor(Qt::PointingHandCursor);
    cancel_all_btn_->setStyleSheet(
        QString("QPushButton{background:%3;color:%1;border:1px solid %2;"
                "padding:4px 14px;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;letter-spacing:0.5px;border-radius:2px;}"
                "QPushButton:hover{background:%4;}")
            .arg(fincept::ui::colors::AMBER(), fincept::ui::colors::AMBER_DIM(),
                 fincept::ui::colors::alpha(fincept::ui::colors::AMBER, 0.12),
                 fincept::ui::colors::alpha(fincept::ui::colors::AMBER, 0.25)));
    connect(cancel_all_btn_, &QPushButton::clicked, this, [this]() {
        if (account_id_.isEmpty())
            return;
        auto answer = QMessageBox::warning(this, tr("Cancel All Orders"),
                                           tr("This will cancel ALL pending orders.\n\nAre you sure?"),
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        emit cancel_all_orders_requested(account_id_);
    });
    bar_layout->addWidget(cancel_all_btn_);
    vlay->addWidget(action_bar);

    orders_table_ = new QTableWidget;
    orders_table_->setObjectName("eqTable");
    orders_table_->setColumnCount(kOrdCols);
    orders_table_->setHorizontalHeaderLabels({tr("Time"), tr("Symbol"), tr("Side"), tr("Type"), tr("Product"),
                                              tr("Qty"), tr("Filled"), tr("Price"), tr("Trigger"), tr("Avg Fill"),
                                              tr("Status"), tr("Order ID"), tr("Action")});
    orders_table_->verticalHeader()->setVisible(false);
    orders_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    orders_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    orders_table_->setShowGrid(false);
    orders_table_->horizontalHeader()->setStretchLastSection(true);
    orders_table_->verticalHeader()->setDefaultSectionSize(table_row_height(-2, 22));
    // Header-click sorting, numeric columns by value (NumItem). Default: Time,
    // newest first. The setters turn sorting off while they write rows by index
    // (the Action column holds per-row cell widgets, which move with their row).
    orders_table_->horizontalHeader()->setSortIndicator(OC_Time, Qt::DescendingOrder);
    orders_table_->setSortingEnabled(true);
    vlay->addWidget(orders_table_, 1);

    orders_tab_idx_ = tabs_->addTab(container, tr("ORDERS"));
}

// ── Funds Tab ──────────────────────────────────────────────────────────────

// Build one metric card (caption + value) into a grid; stash the value/caption
// labels so the data setters and retranslateUi can reach them by slot index.
static QFrame* make_metric_card(const QString& caption, QLabel*& cap_out, QLabel*& val_out) {
    auto* card = new QFrame;
    card->setObjectName("eqMetricCard");
    card->setStyleSheet(QString("#eqMetricCard{background:%1;border:1px solid %2;border-radius:4px;}")
                            .arg(fincept::ui::colors::PANEL(), fincept::ui::colors::BORDER()));
    auto* cv = new QVBoxLayout(card);
    cv->setContentsMargins(12, 8, 12, 8);
    cv->setSpacing(3);
    auto* cap = new QLabel(caption);
    cap->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;letter-spacing:0.5px;font-weight:600;")
                           .arg(fincept::ui::colors::TEXT_SECONDARY()));
    auto* val = new QLabel(QStringLiteral("--"));
    val->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(1) + "px;font-weight:700;").arg(fincept::ui::colors::TEXT_PRIMARY()));
    cv->addWidget(cap);
    cv->addWidget(val);
    cap_out = cap;
    val_out = val;
    return card;
}

void EquityBottomPanel::setup_funds_tab() {
    auto* scroll = new QScrollArea;
    scroll->setObjectName("eqFundsScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* widget = new QWidget;
    auto* outer = new QVBoxLayout(widget);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(10);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    const QString caps[kFundCards] = {tr("AVAILABLE BALANCE"), tr("USED MARGIN"),    tr("TOTAL EQUITY"),
                                      tr("REALIZED P&L"),      tr("UNREALIZED P&L"), tr("HOLDINGS VALUE"),
                                      tr("OPENING BALANCE"),   tr("MARGIN USED %"),  tr("COLLATERAL")};
    for (int i = 0; i < kFundCards; ++i)
        grid->addWidget(make_metric_card(caps[i], fund_card_cap_[i], fund_card_val_[i]), i / 3, i % 3);
    for (int c = 0; c < 3; ++c)
        grid->setColumnStretch(c, 1);

    outer->addLayout(grid);
    outer->addStretch(1);
    scroll->setWidget(widget);
    funds_tab_idx_ = tabs_->addTab(scroll, tr("FUNDS"));
}

// ── Stats Tab ──────────────────────────────────────────────────────────────

void EquityBottomPanel::setup_stats_tab() {
    auto* scroll = new QScrollArea;
    scroll->setObjectName("eqStatsScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* widget = new QWidget;
    auto* outer = new QVBoxLayout(widget);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(10);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    const QString caps[kStatCards] = {tr("NET P&L"),       tr("TODAY'S P&L"),  tr("RETURN %"),     tr("WIN RATE"),
                                      tr("PROFIT FACTOR"), tr("TOTAL TRADES"), tr("AVG WIN"),      tr("AVG LOSS"),
                                      tr("TURNOVER"),      tr("LARGEST WIN"),  tr("LARGEST LOSS"), tr("TOTAL CHARGES")};
    for (int i = 0; i < kStatCards; ++i)
        grid->addWidget(make_metric_card(caps[i], stat_card_cap_[i], stat_card_val_[i]), i / 3, i % 3);
    for (int c = 0; c < 3; ++c)
        grid->setColumnStretch(c, 1);

    outer->addLayout(grid);
    outer->addStretch(1);
    scroll->setWidget(widget);
    stats_tab_idx_ = tabs_->addTab(scroll, tr("STATS"));
}

// ── Data Setters ───────────────────────────────────────────────────────────

void EquityBottomPanel::clear_blotter_tables() {
    if (positions_table_)
        positions_table_->setRowCount(0);
    if (holdings_table_)
        holdings_table_->setRowCount(0);
    if (orders_table_)
        orders_table_->setRowCount(0);
    last_positions_.clear();
    last_paper_positions_.clear();
    last_holdings_.clear();
    // The re-render caches belong to the previous account / mode as well: a format
    // change must never paint them back under the new one.
    last_orders_.clear();
    last_paper_orders_.clear();
    last_paper_trades_.clear();
    have_paper_trades_ = false;
    update_positions_summary(); // zero the net-P&L / winners-losers strip
}

void EquityBottomPanel::set_mode(bool is_paper) {
    if (fills_tab_idx_ >= 0)
        tabs_->setTabVisible(fills_tab_idx_, is_paper); // simulated fills: local paper only
    if (is_paper_ == is_paper)
        return;
    is_paper_ = is_paper;
    // Paper and live share the same tables — clear so the previous mode's rows
    // can't linger under the new one until fresh data arrives.
    clear_blotter_tables();
    if (fills_table_)
        fills_table_->setRowCount(0);
}

void EquityBottomPanel::set_symbol_meta(const QHash<QString, SymbolMeta>& meta) {
    meta_ = meta;
}

double EquityBottomPanel::meta_fx(const QString& symbol) const {
    const auto it = meta_.constFind(symbol);
    if (it == meta_.constEnd())
        return 1.0; // no entry: single-currency book, money already in the account currency
    // An entry without a usable factor has no rate yet: NaN, so the row's money
    // shows "—" and the totals leave it out — never counted at 1:1.
    return (std::isfinite(it->fx) && it->fx > 0.0) ? it->fx : std::numeric_limits<double>::quiet_NaN();
}

QString EquityBottomPanel::fx_missing_tip(const QString& symbol) const {
    const QString ccy = meta_.value(symbol).currency;
    return tr("No %1 to %2 exchange rate yet (it is being fetched): this row's money is not shown and is left out of "
              "the totals.")
        .arg(ccy.isEmpty() ? tr("price currency") : ccy, currency_.isEmpty() ? tr("account currency") : currency_);
}

QString EquityBottomPanel::px_text(const QString& symbol, double v) const {
    const auto it = meta_.constFind(symbol);
    fincept::ui::formatting::InstrumentFmt inst;
    if (it == meta_.constEnd() || it->currency.isEmpty()) {
        // Single-currency book: a bare price in the account currency's decimals.
        inst.currency = currency_;
        return fincept::ui::formatting::price(v, inst);
    }
    inst.currency = it->currency;
    inst.decimals = it->digits;
    return fincept::ui::formatting::price_with_symbol(v, inst);
}

// ── Fills (paper) ──────────────────────────────────────────────────────────

void EquityBottomPanel::setup_fills_tab() {
    fills_table_ = new QTableWidget;
    fills_table_->setObjectName("eqTable");
    fills_table_->setColumnCount(8);
    fills_table_->setHorizontalHeaderLabels(
        {tr("Time"), tr("Symbol"), tr("Side"), tr("Qty"), tr("Price"), tr("Fee"), tr("Realized P&L"), tr("Note")});
    fills_table_->verticalHeader()->setVisible(false);
    fills_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    fills_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    fills_table_->setShowGrid(false);
    fills_table_->horizontalHeader()->setStretchLastSection(true);
    fills_table_->verticalHeader()->setDefaultSectionSize(table_row_height(-2, 22));
    fills_tab_idx_ = tabs_->addTab(fills_table_, tr("FILLS"));
    tabs_->setTabToolTip(fills_tab_idx_, tr("Simulated fills of the paper account"));
}

void EquityBottomPanel::set_paper_trades(const QVector<trading::PtTrade>& trades, const QString& note) {
    last_paper_trades_ = trades; // re-rendered on a format change
    last_trades_note_ = note;
    have_paper_trades_ = true;
    if (!fills_table_)
        return;
    fills_table_->setUpdatesEnabled(false);
    fills_table_->setRowCount(trades.size());
    for (int i = 0; i < trades.size(); ++i) {
        const auto& t = trades[i];
        eqbp_set_time_cell(ensure_num_item(fills_table_, i, 0), t.timestamp, QTimeZone(QTimeZone::UTC), QTimeZone());
        ensure_item(fills_table_, i, 1)->setText(t.symbol);
        auto* side_item = ensure_item(fills_table_, i, 2);
        side_item->setText(t.side.toUpper());
        side_item->setForeground(side_qcolor(t.side));
        set_right_cell(ensure_item(fills_table_, i, 3), format_quantity(t.quantity));
        set_right_cell(ensure_item(fills_table_, i, 4), px_text(t.symbol, t.price));
        // Fee and realised P&L are cash: booked by the engine in the account currency.
        set_right_cell(ensure_item(fills_table_, i, 5), eqbp_money(t.fee, currency_));
        set_right_cell(ensure_item(fills_table_, i, 6), t.pnl != 0.0 ? eqbp_money(t.pnl, currency_, true)
                                                                     : fincept::ui::formatting::placeholder(),
                       pnl_qcolor(t.pnl));
        auto* note_item = ensure_item(fills_table_, i, 7);
        note_item->setText(note);
        note_item->setForeground(QColor(fincept::ui::colors::TEXT_SECONDARY()));
    }
    fills_table_->setUpdatesEnabled(true);
}

void EquityBottomPanel::set_account_id(const QString& account_id) {
    if (account_id_ == account_id)
        return;
    account_id_ = account_id;
    // Different account → drop the previous account's positions/holdings/orders.
    clear_blotter_tables();
}

void EquityBottomPanel::set_currency(const QString& currency_code) {
    const QString c = currency_code.trimmed();
    if (c.isEmpty() || c == currency_)
        return;
    currency_ = c;
    apply_money_headers();
}

void EquityBottomPanel::set_us_market_tabs_visible(bool visible) {
    us_tabs_visible_ = visible;
    if (time_sales_tab_idx_ >= 0)
        tabs_->setTabVisible(time_sales_tab_idx_, visible);
    // AUCTIONS only earns a tab once set_auctions() has delivered rows (nothing in the data
    // streams feeds it yet — a permanently empty tab reads as broken).
    if (auctions_tab_idx_ >= 0)
        tabs_->setTabVisible(auctions_tab_idx_, visible && auctions_table_ && auctions_table_->rowCount() > 0);
    if (calendar_tab_idx_ >= 0)
        tabs_->setTabVisible(calendar_tab_idx_, visible);
}

void EquityBottomPanel::update_positions_summary() {
    double total = 0.0, win_sum = 0.0, loss_sum = 0.0;
    int win_n = 0, loss_n = 0, no_fx = 0;
    auto accum = [&](double pnl) {
        if (!std::isfinite(pnl)) {
            ++no_fx; // no rate to the account currency yet: left out, said below
            return;
        }
        total += pnl;
        if (pnl > 0.0) {
            win_sum += pnl;
            ++win_n;
        } else if (pnl < 0.0) {
            loss_sum += pnl;
            ++loss_n;
        }
    };
    if (is_paper_)
        for (const auto& p : last_paper_positions_)
            accum(p.unrealized_pnl * meta_fx(p.symbol)); // account currency
    else
        for (const auto& p : last_positions_)
            accum(p.pnl);

    const QString pos = fincept::ui::colors::POSITIVE();
    const QString neg = fincept::ui::colors::NEGATIVE();
    const QString pri = fincept::ui::colors::TEXT_PRIMARY();
    if (positions_total_pnl_label_) {
        positions_total_pnl_label_->setText(tr("Total P&L  %1").arg(eqbp_money(total, currency_, true)) +
                                            eqbp_partial_note(no_fx));
        positions_total_pnl_label_->setToolTip(no_fx > 0 ? tr("Rows whose price currency has no exchange rate to %1 "
                                                              "yet are not added in (never counted at 1:1).")
                                                               .arg(currency_)
                                                         : QString());
        set_style_if_changed(
            positions_total_pnl_label_,
            QString("font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;color:%1;").arg(total > 0 ? pos : (total < 0 ? neg : pri)));
    }
    if (positions_win_label_)
        positions_win_label_->setText(
            QString::fromUtf8("▲ %1  %2").arg(win_n).arg(eqbp_money(win_sum, currency_, true)));
    if (positions_loss_label_)
        positions_loss_label_->setText(
            QString::fromUtf8("▼ %1  %2").arg(loss_n).arg(eqbp_money(loss_sum, currency_, true)));
    if (win_squareoff_btn_)
        win_squareoff_btn_->setEnabled(win_n > 0);
    if (loss_squareoff_btn_)
        loss_squareoff_btn_->setEnabled(loss_n > 0);
}

QVector<trading::BrokerPosition> EquityBottomPanel::square_off_group_targets(int sign) const {
    // Picked from the SAME tick-patched rows update_positions_summary() counts, so what
    // gets closed is what the ▲/▼ chips show — not an older REST snapshot.
    QVector<trading::BrokerPosition> out;
    auto in_group = [sign](double pnl) { return (sign > 0 && pnl > 0.0) || (sign < 0 && pnl < 0.0); };
    if (is_paper_) {
        for (const auto& p : last_paper_positions_) {
            if (p.quantity == 0.0 || trading::product_is_delivery(p.product) || !in_group(p.unrealized_pnl))
                continue; // CNC lives in Holdings — never squared by the positions buttons
            trading::BrokerPosition bp;
            bp.symbol = p.symbol;
            bp.product_type = p.product;
            bp.quantity = p.quantity;
            bp.side = p.side;
            bp.avg_price = p.entry_price;
            bp.ltp = p.current_price;
            bp.pnl = p.unrealized_pnl * meta_fx(p.symbol); // account currency, as the chips show
            out.append(bp);
        }
    } else {
        for (const auto& p : last_positions_) {
            // A squared-off row (qty 0) carries realised P&L — it is not an open position.
            if (p.quantity == 0.0 || !in_group(p.pnl))
                continue;
            out.append(p);
        }
    }
    return out;
}

void EquityBottomPanel::request_square_off_group(int sign) {
    if (account_id_.isEmpty() || sign == 0)
        return;
    const QString title = sign > 0 ? tr("Square Off Winning Positions") : tr("Square Off Losing Positions");
    const auto targets = square_off_group_targets(sign);
    if (targets.isEmpty()) {
        QMessageBox::information(this, title,
                                 sign > 0 ? tr("No open position is in profit right now — nothing to square off.")
                                          : tr("No open position is in loss right now — nothing to square off."));
        return;
    }
    // List exactly what will be closed (the confirmation used to say only "ALL positions
    // currently in profit", while the closes were picked from a different snapshot).
    constexpr int kMaxListed = 20;
    QStringList lines;
    for (int i = 0; i < targets.size() && i < kMaxListed; ++i) {
        const auto& t = targets[i];
        lines << QString::fromUtf8("• %1 · %2 · %3 · %4 %5 · P&L %6")
                     .arg(t.symbol, t.exchange.isEmpty() ? QStringLiteral("--") : t.exchange,
                          t.product_type.isEmpty() ? QStringLiteral("--") : t.product_type.toUpper(), t.side.toUpper(),
                          format_quantity(t.quantity), eqbp_money(t.pnl, currency_, true));
    }
    const QString more =
        targets.size() > kMaxListed ? tr("\n… and %1 more").arg(targets.size() - kMaxListed) : QString();
    const QString body = tr("This will place MARKET orders to close these %1 position(s) currently in %2:\n\n%3%4\n\n"
                            "Are you sure?")
                             .arg(QString::number(targets.size()), sign > 0 ? tr("profit") : tr("loss"),
                                  lines.join(QLatin1Char('\n')), more);
    const auto answer = QMessageBox::warning(this, title, body, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes)
        emit square_off_group_requested(account_id_, sign, targets);
}

void EquityBottomPanel::set_paper_positions(const QVector<trading::PtPosition>& positions) {
    last_paper_positions_ = positions; // keep row-aligned for live-quote patching
    const auto view = capture_view(positions_table_, 0);
    positions_table_->setUpdatesEnabled(false);
    // Column 2 holds the open timestamp in paper mode (see the live setter, which
    // puts the exchange there) — keep the header honest about what it shows.
    if (auto* h2 = positions_table_->horizontalHeaderItem(2); !h2 || h2->text() != tr("Opened"))
        positions_table_->setHorizontalHeaderItem(2, new QTableWidgetItem(tr("Opened")));
    positions_table_->setRowCount(positions.size());
    for (int i = 0; i < positions.size(); ++i) {
        const auto& p = positions[i];
        ensure_item(positions_table_, i, 0)->setText(p.symbol);
        ensure_item(positions_table_, i, 1)->setText(p.product.isEmpty() ? QStringLiteral("MIS") : p.product.toUpper());

        // Col 2 — "Opened": parse ISO timestamp, convert to local time, fall back to raw string
        {
            const QDateTime dt = QDateTime::fromString(p.opened_at, Qt::ISODate).toLocalTime();
            ensure_item(positions_table_, i, 2)->setText(dt.isValid() ? dt.toString("yyyy-MM-dd HH:mm") : p.opened_at);
        }

        auto* side_item = ensure_item(positions_table_, i, 3);
        side_item->setText(p.side.toUpper());
        side_item->setForeground(side_qcolor(p.side));
        set_right_cell(ensure_item(positions_table_, i, 4), format_quantity(p.quantity));
        // Prices in the instrument's own currency; money in the account currency.
        set_right_cell(ensure_item(positions_table_, i, 5), px_text(p.symbol, p.entry_price));
        set_right_cell(ensure_item(positions_table_, i, 6), px_text(p.symbol, p.current_price));

        // Col 7 — P&L in the account currency (signed, with its unit; flat = neutral).
        // No rate to the account currency yet: "—", explained in the tooltip.
        const double pnl_acct = p.unrealized_pnl * meta_fx(p.symbol);
        auto* pnl_cell = ensure_item(positions_table_, i, 7);
        set_right_cell(pnl_cell, eqbp_money(pnl_acct, currency_, true), pnl_qcolor(pnl_acct));
        pnl_cell->setToolTip(std::isfinite(pnl_acct) ? QString() : fx_missing_tip(p.symbol));

        // Col 8 — P&L %. Use notional as the divisor so both entry_price == 0 and
        // quantity == 0 are handled by a single guard (avoids divide-by-zero).
        const double notional = p.entry_price * p.quantity;
        const double pct = notional != 0.0 ? (p.unrealized_pnl / notional) * 100.0 : 0.0;
        set_right_cell(ensure_item(positions_table_, i, 8), fincept::ui::formatting::pct(pct, 2, true),
                       pnl_qcolor(pct));

        // Col 9 — Action: SELL (exit, pre-filled qty) + for intraday (MIS), a
        // "→ CNC" convert-to-delivery button. NRML/CNC rows get SELL only.
        // Paper rows carry no exchange — the screen falls back to its own market.
        positions_table_->setCellWidget(i, 9,
                                        make_positions_action_cell(p.symbol, QString(), p.product, p.quantity, p.side,
                                                                   trading::product_is_intraday(p.product), p.id));
    }
    positions_table_->setUpdatesEnabled(true);
    restore_view(positions_table_, 0, view);
    update_positions_summary();
}

void EquityBottomPanel::set_paper_orders(const QVector<trading::PtOrder>& orders) {
    last_paper_orders_ = orders; // re-rendered on a format change
    orders_are_paper_ = true;
    const auto view = capture_view(orders_table_, OC_OrderId);
    // Rows are written by index: sorting off while filling, restored (re-sorted) after.
    const bool was_sorting = orders_table_->isSortingEnabled();
    orders_table_->setSortingEnabled(false);
    orders_table_->setUpdatesEnabled(false);
    orders_table_->setRowCount(orders.size());
    const QString dash = fincept::ui::formatting::placeholder();
    for (int i = 0; i < orders.size(); ++i) {
        const auto& o = orders[i];
        // The paper engine stores UTC ISO stamps (naive = UTC).
        eqbp_set_time_cell(ensure_num_item(orders_table_, i, OC_Time), o.created_at, QTimeZone(QTimeZone::UTC),
                           QTimeZone());
        ensure_item(orders_table_, i, OC_Symbol)->setText(o.symbol);
        auto* side_item = ensure_item(orders_table_, i, OC_Side);
        side_item->setText(o.side.toUpper());
        side_item->setForeground(side_qcolor(o.side));
        ensure_item(orders_table_, i, OC_Type)->setText(o.order_type.toUpper());
        ensure_item(orders_table_, i, OC_Product)
            ->setText(o.product.isEmpty() ? QStringLiteral("MIS") : o.product.toUpper());

        auto num = [&](int col, double value, const QString& text, const QColor& fg = QColor()) {
            auto* it = ensure_num_item(orders_table_, i, col);
            set_num_cell(it, value, text);
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            it->setForeground(fg.isValid() ? fg : QColor(fincept::ui::colors::TEXT_PRIMARY()));
            return it;
        };
        num(OC_Qty, o.quantity, format_quantity(o.quantity));
        const bool partial = o.filled_qty > 0.0 && o.filled_qty < o.quantity;
        num(OC_Filled, o.filled_qty, format_quantity(o.filled_qty) + QStringLiteral("/") + format_quantity(o.quantity),
            partial ? QColor(fincept::ui::colors::AMBER()) : QColor());
        // A MARKET order's stored price is only its margin reference: show "MKT".
        const bool is_mkt = o.order_type == QLatin1String("market");
        num(OC_Price, o.price && !is_mkt ? *o.price : 0.0,
            o.price && !is_mkt ? px_text(o.symbol, *o.price) : tr("MKT"));
        const double trig = o.stop_price && *o.stop_price > 0.0 ? *o.stop_price : 0.0;
        num(OC_Trigger, trig, trig > 0.0 ? px_text(o.symbol, trig) : dash);
        const double avg = o.avg_price && *o.avg_price > 0.0 ? *o.avg_price : 0.0;
        num(OC_AvgFill, avg, avg > 0.0 ? px_text(o.symbol, avg) : dash);

        auto* status_item = ensure_item(orders_table_, i, OC_Status);
        status_item->setText(o.status.toUpper());
        status_item->setForeground(order_status_color(o.status));
        auto* id_item = ensure_item(orders_table_, i, OC_OrderId);
        id_item->setText(o.id.left(8));
        id_item->setToolTip(o.id);

        // A resting paper order (still with the matcher) gets EDIT + CANCEL. EDIT
        // goes through the screen's paper modify (cancel + re-place, pt_modify_order).
        // The cell shows a short id; the signals carry the FULL id.
        ensure_item(orders_table_, i, OC_Action)->setText("");
        const QString st = o.status.trimmed().toLower();
        if (st == QLatin1String("pending") || st == QLatin1String("partial")) {
            auto* cancel_btn = new QPushButton(tr("CANCEL"));
            cancel_btn->setObjectName("eqTableBtn");
            cancel_btn->setProperty("act", "cancel");
            cancel_btn->setProperty("fincept_agent_risk", "order");
            cancel_btn->setMinimumHeight(18);
            cancel_btn->setAccessibleName(tr("Cancel paper order %1").arg(o.id.left(8)));
            cancel_btn->setCursor(Qt::PointingHandCursor);
            const QString full_id = o.id;
            const QString short_id = o.id.left(8);
            connect(cancel_btn, &QPushButton::clicked, this, [this, full_id, short_id]() {
                if (QMessageBox::question(this, tr("Cancel Order"), tr("Cancel paper order %1?").arg(short_id),
                                          QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
                    emit cancel_order_requested(full_id);
            });
            // EDIT — paper modify = cancel + re-place with the new quantity / price
            // (pt_modify_order); a queued MARKET order changes quantity only.
            auto* edit_btn = new QPushButton(tr("EDIT"));
            edit_btn->setObjectName("eqTableBtn");
            edit_btn->setProperty("act", "edit");
            edit_btn->setProperty("fincept_agent_risk", "order");
            edit_btn->setMinimumHeight(18);
            edit_btn->setAccessibleName(tr("Modify paper order %1").arg(short_id));
            edit_btn->setCursor(Qt::PointingHandCursor);
            const QString type = o.order_type;
            const double qty = o.quantity;
            const double px = type == QLatin1String("stop") ? o.stop_price.value_or(0.0) : o.price.value_or(0.0);
            const QString caption = type == QLatin1String("market") ? QString()
                                    : type == QLatin1String("stop") ? tr("TRIGGER PRICE")
                                                                    : tr("LIMIT PRICE");
            connect(edit_btn, &QPushButton::clicked, this,
                    [this, full_id, qty, px, caption]() { open_modify_dialog(full_id, qty, px, caption); });
            auto* action_cell = new QWidget;
            auto* action_lay = new QHBoxLayout(action_cell);
            action_lay->setContentsMargins(2, 0, 2, 0);
            action_lay->setSpacing(4);
            action_lay->addWidget(edit_btn);
            action_lay->addWidget(cancel_btn);
            action_lay->addStretch();
            orders_table_->setCellWidget(i, OC_Action, action_cell);
        } else {
            orders_table_->setCellWidget(i, OC_Action, nullptr);
        }
    }
    orders_table_->setSortingEnabled(was_sorting);
    orders_table_->setUpdatesEnabled(true);
    restore_view(orders_table_, OC_OrderId, view);
}

void EquityBottomPanel::open_modify_dialog(const QString& oid, double qty, double prc, const QString& price_caption) {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Modify Order"));
    dlg->setFixedWidth(280);
    dlg->setStyleSheet(QString("QDialog { background: %1; color: %2; }"
                               "QLabel { color: %3; font-size: " +
                               fincept::ui::fonts::px(-3) +
                               "px; }"
                               "QLineEdit { background: %4; border: 1px solid %5; "
                               "  color: %2; padding: 5px; border-radius: 2px; }"
                               "QLineEdit:focus { border-color: %6; }"
                               "QPushButton { padding: 6px 14px; font-weight: 700; border-radius: 2px; }")
                           .arg(fincept::ui::colors::BG_SURFACE())
                           .arg(fincept::ui::colors::TEXT_PRIMARY())
                           .arg(fincept::ui::colors::TEXT_SECONDARY())
                           .arg(fincept::ui::colors::BG_BASE())
                           .arg(fincept::ui::colors::BORDER_MED())
                           .arg(fincept::ui::colors::AMBER()));
    auto* vlay = new QVBoxLayout(dlg);
    vlay->setSpacing(6);
    vlay->setContentsMargins(14, 14, 14, 14);

    auto* qty_lbl = new QLabel(tr("QTY"));
    auto* qty_edit = new QLineEdit(format_quantity(qty));
    auto* prc_lbl = new QLabel(price_caption);
    auto* prc_edit = new QLineEdit(prc > 0.0 ? QString::number(prc, 'f', 2) : QString());
    const bool has_price = !price_caption.isEmpty();
    prc_lbl->setVisible(has_price);
    prc_edit->setVisible(has_price);

    auto* btn_row = new QHBoxLayout;
    auto* ok_btn = new QPushButton(tr("MODIFY"));
    ok_btn->setProperty("fincept_agent_risk", "order");
    ok_btn->setStyleSheet(QString("background: %3; color: %1; border: 1px solid %2;")
                              .arg(fincept::ui::colors::AMBER())
                              .arg(fincept::ui::colors::AMBER_DIM())
                              .arg(fincept::ui::colors::alpha(fincept::ui::colors::AMBER, 0.15)));
    auto* cancel_btn = new QPushButton(tr("CANCEL"));
    cancel_btn->setStyleSheet(QString("background: %3; color: %1; border: 1px solid %2;")
                                  .arg(fincept::ui::colors::NEGATIVE())
                                  .arg(fincept::ui::colors::NEGATIVE_DIM())
                                  .arg(fincept::ui::colors::alpha(fincept::ui::colors::NEGATIVE, 0.1)));

    connect(ok_btn, &QPushButton::clicked, dlg, [dlg, this, oid, qty_edit, prc_edit, has_price]() {
        // A garbage quantity used to close the dialog with no order change and no
        // message — the trader believed the modify had gone through. Refuse instead.
        bool qty_ok = false, prc_ok = !has_price;
        const double new_qty = qty_edit->text().trimmed().toDouble(&qty_ok);
        const double new_prc = has_price ? prc_edit->text().trimmed().toDouble(&prc_ok) : 0.0;
        if (!qty_ok || new_qty <= 0.0) {
            QMessageBox::warning(dlg, tr("Modify Order"), tr("Enter a valid quantity."));
            return;
        }
        if (!prc_ok || new_prc < 0.0) {
            QMessageBox::warning(dlg, tr("Modify Order"), tr("Enter a valid price."));
            return;
        }
        emit modify_order_requested(oid, new_qty, new_prc);
        dlg->accept();
    });
    connect(cancel_btn, &QPushButton::clicked, dlg, &QDialog::reject);

    vlay->addWidget(qty_lbl);
    vlay->addWidget(qty_edit);
    vlay->addWidget(prc_lbl);
    vlay->addWidget(prc_edit);
    btn_row->addWidget(ok_btn);
    btn_row->addWidget(cancel_btn);
    vlay->addLayout(btn_row);
    dlg->exec();
    dlg->deleteLater();
}

void EquityBottomPanel::set_funds_view(const EquityFundsView& v) {
    last_funds_view_ = v; // re-rendered on a format change
    have_funds_view_ = true;
    const QString ccy = v.currency; // ISO code of every amount in the view
    const QString pos = fincept::ui::colors::POSITIVE();
    const QString neg = fincept::ui::colors::NEGATIVE();
    const QString pri = fincept::ui::colors::TEXT_PRIMARY();

    auto set_plain = [&](int idx, const QString& text) {
        if (!fund_card_val_[idx])
            return;
        fund_card_val_[idx]->setText(text);
        fund_card_val_[idx]->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(1) + "px;font-weight:700;").arg(pri));
    };
    // Totals that leave out positions without an FX rate say so on the card.
    const QString partial = eqbp_partial_note(v.fx_missing);
    auto set_pnl = [&](int idx, double val, const QString& suffix = QString()) {
        if (!fund_card_val_[idx])
            return;
        fund_card_val_[idx]->setText(eqbp_money(val, ccy, true) + suffix);
        fund_card_val_[idx]->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(1) + "px;font-weight:700;").arg(val >= 0 ? pos : neg));
    };

    set_plain(FC_Available, eqbp_money(v.available, ccy));
    set_plain(FC_UsedMargin, eqbp_money(v.used_margin, ccy));
    set_plain(FC_TotalEquity, eqbp_money(v.total_equity, ccy) + partial);
    set_pnl(FC_RealizedPnl, v.realized_pnl);
    set_pnl(FC_UnrealizedPnl, v.unrealized_pnl, partial);
    set_plain(FC_HoldingsValue, eqbp_money(v.holdings_value, ccy) + partial);
    set_plain(FC_OpeningBalance, eqbp_money(v.opening_balance, ccy));
    set_plain(FC_MarginUtil, fincept::ui::formatting::pct(v.margin_util_pct, 1, false));
    set_plain(FC_Collateral, eqbp_money(v.collateral, ccy));
}

void EquityBottomPanel::set_stats_view(const EquityStatsView& v) {
    last_stats_view_ = v; // re-rendered on a format change
    have_stats_view_ = true;
    const QString ccy = v.currency; // ISO code of the money fields
    const QString pos = fincept::ui::colors::POSITIVE();
    const QString neg = fincept::ui::colors::NEGATIVE();
    const QString pri = fincept::ui::colors::TEXT_PRIMARY();

    auto set_card = [&](int idx, const QString& text, const QString& color) {
        if (!stat_card_val_[idx])
            return;
        stat_card_val_[idx]->setText(text);
        stat_card_val_[idx]->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(1) + "px;font-weight:700;").arg(color));
    };
    auto pnl_color = [&](double x) { return x >= 0 ? pos : neg; };

    namespace uf = fincept::ui::formatting;
    set_card(SC_NetPnl, eqbp_money(v.net_pnl, ccy, true) + eqbp_partial_note(v.fx_missing), pnl_color(v.net_pnl));
    set_card(SC_TodayPnl, eqbp_money(v.today_pnl, ccy, true), pnl_color(v.today_pnl));
    set_card(SC_ReturnPct, uf::pct(v.return_pct, 2, true), pnl_color(v.return_pct));
    set_card(SC_WinRate,
             QString("%1 (%2/%3)")
                 .arg(uf::fraction_pct(v.win_rate, 1, false))
                 .arg(v.winning_trades)
                 .arg(v.winning_trades + v.losing_trades),
             pri);
    set_card(SC_ProfitFactor, v.profit_factor > 0 ? uf::number(v.profit_factor, 2) : uf::placeholder(), pri);
    set_card(SC_TotalTrades, uf::quantity(static_cast<double>(v.total_trades)), pri);
    set_card(SC_AvgWin, eqbp_money(v.avg_win, ccy, true), pos);
    set_card(SC_AvgLoss, eqbp_money(v.avg_loss, ccy, true), neg);
    set_card(SC_Turnover, eqbp_money(v.turnover, ccy), pri);
    set_card(SC_LargestWin, eqbp_money(v.largest_win, ccy, true), pos);
    set_card(SC_LargestLoss, eqbp_money(v.largest_loss, ccy, true), neg);
    set_card(SC_TotalFees, eqbp_money(v.total_fees, ccy), pri);
}

void EquityBottomPanel::set_orders_date(const QDate& day) {
    if (!orders_date_edit_)
        return;
    suppress_orders_date_signal_ = true;
    orders_date_edit_->setDate(day);
    suppress_orders_date_signal_ = false;
}

QWidget* EquityBottomPanel::make_positions_action_cell(const QString& symbol, const QString& exchange,
                                                       const QString& product, double qty, const QString& side,
                                                       bool show_convert, const QString& paper_pid) {
    auto* cell = new QWidget;
    auto* lay = new QHBoxLayout(cell);
    lay->setContentsMargins(2, 0, 2, 0);
    lay->setSpacing(4);

    // Exit → opens the order ticket pre-filled with the held quantity (parity with
    // the FNO tab's per-strike order button, instead of right-click-only). For a
    // SHORT position the exit side is BUY (cover): paper rows carry a positive qty
    // with side "short"; live broker rows carry a signed qty and/or side
    // "SHORT"/"SELL" — detect either.
    const bool exit_is_buy = qty < 0 || side.contains(QStringLiteral("short"), Qt::CaseInsensitive) ||
                             side.compare(QStringLiteral("sell"), Qt::CaseInsensitive) == 0;
    auto* sell = new QPushButton(exit_is_buy ? tr("BUY") : tr("SELL"));
    sell->setObjectName("eqTableBtn");
    // Direction is colour-coded (green = buy/cover, red = sell) so a wrong-way
    // click on an exit button is visually obvious before it is pressed.
    sell->setProperty("act", exit_is_buy ? "cover" : "sell");
    sell->setProperty("fincept_agent_risk", "order");
    sell->setMinimumHeight(18);
    sell->setCursor(Qt::PointingHandCursor);
    sell->setAccessibleName(exit_is_buy ? tr("Buy to cover %1").arg(symbol) : tr("Sell to exit %1").arg(symbol));
    sell->setToolTip(exit_is_buy
                         ? tr("Buy / cover %1 — opens an order ticket pre-filled with the held quantity").arg(symbol)
                         : tr("Sell / exit %1 — opens an order ticket pre-filled with the held quantity").arg(symbol));
    const double held = qAbs(qty);
    const QString prod = product.isEmpty() ? QStringLiteral("MIS") : product;
    connect(sell, &QPushButton::clicked, this, [this, symbol, exchange, prod, held, exit_is_buy]() {
        LOG_DEBUG("EquityTrading", QString("[panel] per-row %1 clicked: sym='%2' exch='%3' prod='%4' qty=%5")
                              .arg(exit_is_buy ? QStringLiteral("BUY(cover)") : QStringLiteral("SELL"), symbol,
                                   exchange, prod)
                              .arg(held));
        emit trade_symbol_requested(symbol, exchange, prod, exit_is_buy, held);
    });
    lay->addWidget(sell);

    // Paper intraday only: convert MIS → CNC (carry overnight).
    if (show_convert && !paper_pid.isEmpty()) {
        auto* cnc = new QPushButton(tr("→ CNC"));
        cnc->setObjectName("eqTableBtn");
        cnc->setProperty("act", "cnc");
        cnc->setProperty("fincept_agent_risk", "order"); // converts the position's product
        cnc->setMinimumHeight(18);
        cnc->setCursor(Qt::PointingHandCursor);
        cnc->setAccessibleName(tr("Convert %1 to delivery").arg(symbol));
        cnc->setToolTip(tr("Convert to CNC delivery (carry overnight, locks full cash)"));
        connect(cnc, &QPushButton::clicked, this, [this, paper_pid, symbol]() {
            emit convert_position_requested(paper_pid, symbol, QStringLiteral("CNC"));
        });
        lay->addWidget(cnc);
    }
    lay->addStretch();
    return cell;
}

void EquityBottomPanel::set_positions(const QVector<trading::BrokerPosition>& positions) {
    last_positions_ = positions; // keep row-aligned for live-quote patching
    const auto view = capture_view(positions_table_, 0);
    positions_table_->setUpdatesEnabled(false);
    // Live rows put the EXCHANGE in column 2 (brokers don't return an open time),
    // so the shared "Opened" header was simply wrong in live mode.
    if (auto* h2 = positions_table_->horizontalHeaderItem(2); !h2 || h2->text() != tr("Exchange"))
        positions_table_->setHorizontalHeaderItem(2, new QTableWidgetItem(tr("Exchange")));
    positions_table_->setRowCount(positions.size());
    for (int i = 0; i < positions.size(); ++i) {
        const auto& p = positions[i];
        ensure_item(positions_table_, i, 0)->setText(p.symbol);
        ensure_item(positions_table_, i, 1)->setText(p.product_type.isEmpty() ? "--" : p.product_type.toUpper());
        ensure_item(positions_table_, i, 2)->setText(p.exchange.isEmpty() ? "--" : p.exchange);
        auto* side_item = ensure_item(positions_table_, i, 3);
        side_item->setText(p.side.toUpper());
        // Signed-quantity brokers (net qty < 0 = short) may send no side word.
        side_item->setForeground(p.quantity < 0 ? QColor(fincept::ui::colors::NEGATIVE()) : side_qcolor(p.side));
        set_right_cell(ensure_item(positions_table_, i, 4), format_quantity(p.quantity));
        set_right_cell(ensure_item(positions_table_, i, 5), px_text(p.symbol, p.avg_price));
        set_right_cell(ensure_item(positions_table_, i, 6), px_text(p.symbol, p.ltp));

        // Broker P&L: reported in the account currency (documented fallback — the
        // broker position carries no currency of its own).
        set_right_cell(ensure_item(positions_table_, i, 7), eqbp_money(p.pnl, currency_, true), pnl_qcolor(p.pnl));
        set_right_cell(ensure_item(positions_table_, i, 8), fincept::ui::formatting::pct(p.pnl_pct, 2, true),
                       pnl_qcolor(p.pnl_pct));

        // SELL (exit) action; no MIS→CNC convert in live mode (paper-only).
        positions_table_->setCellWidget(i, 9,
                                        make_positions_action_cell(p.symbol, p.exchange, p.product_type, p.quantity,
                                                                   p.side, false, QString()));
    }
    positions_table_->setUpdatesEnabled(true);
    restore_view(positions_table_, 0, view);
    update_positions_summary();
}

void EquityBottomPanel::update_holding_quote(const QString& symbol, double ltp, double prev_close) {
    // Find this symbol's holding (one per symbol) and re-mark it to the live price.
    trading::BrokerHolding* h = nullptr;
    for (auto& it : last_holdings_)
        if (it.symbol == symbol) {
            h = &it;
            break;
        }
    if (!h)
        return;
    h->ltp = ltp;
    if (prev_close > 0.0)
        h->prev_close = prev_close; // capture today's prev close from the live quote
    // Money in the account currency (SymbolMeta factor; 1 for single-currency books;
    // NaN while the rate is missing — the row then shows "—" and is left out of totals).
    const double fx = meta_fx(symbol);
    h->current_value = h->quantity * ltp * fx;
    h->pnl = h->current_value - h->invested_value;
    h->pnl_pct = !std::isfinite(h->pnl)       ? std::numeric_limits<double>::quiet_NaN()
                 : h->invested_value > 0.0 ? (h->pnl / h->invested_value) * 100.0
                                           : 0.0;
    const double day_pnl = h->prev_close > 0.0 ? h->quantity * (ltp - h->prev_close) * fx : 0.0;
    if (!holdings_table_)
        return; // tab not built yet — the data above is painted when it is

    const QColor pos_color(fincept::ui::colors::POSITIVE());
    const QColor neg_color(fincept::ui::colors::NEGATIVE());

    // Patch the row in place. Sorting may reorder rows, so match on the symbol cell
    // rather than assuming index alignment. Disable sorting during the writes.
    const bool was_sorting = holdings_table_->isSortingEnabled();
    holdings_table_->setSortingEnabled(false);
    for (int r = 0; r < holdings_table_->rowCount(); ++r) {
        auto* sym_item = holdings_table_->item(r, 0);
        if (!sym_item || sym_item->text() != symbol)
            continue;
        set_num_cell(ensure_num_item(holdings_table_, r, 3), ltp, px_text(symbol, ltp)); // LTP
        set_num_cell(ensure_num_item(holdings_table_, r, 5), h->current_value,
                     eqbp_amount(h->current_value, currency_)); // Current
        auto* pnl_item = ensure_num_item(holdings_table_, r, 6);
        set_num_cell(pnl_item, h->pnl, eqbp_amount(h->pnl, currency_));
        pnl_item->setForeground(h->pnl >= 0 ? pos_color : neg_color);
        pnl_item->setToolTip(std::isfinite(h->pnl) ? QString() : fx_missing_tip(symbol));
        auto* pct_item = ensure_num_item(holdings_table_, r, 7);
        set_num_cell(pct_item, h->pnl_pct, fincept::ui::formatting::pct(h->pnl_pct, 2, false));
        pct_item->setForeground(h->pnl_pct >= 0 ? pos_color : neg_color);
        auto* day_item = ensure_num_item(holdings_table_, r, 8); // Today's P&L
        set_num_cell(day_item, day_pnl, eqbp_amount(day_pnl, currency_));
        day_item->setForeground(day_pnl >= 0 ? pos_color : neg_color);
        break; // one row per symbol
    }
    holdings_table_->setSortingEnabled(was_sorting);

    // Refresh the summary strip (CURRENT / TOTAL P&L / TODAY'S P&L / RETURN %); invested is fixed.
    // A holding without a rate to the account currency is left out and counted.
    double total_invested = 0.0, total_current = 0.0, total_pnl = 0.0, total_day_pnl = 0.0;
    int no_fx = 0;
    for (const auto& it : last_holdings_) {
        if (!std::isfinite(it.current_value) || !std::isfinite(it.invested_value) || !std::isfinite(it.pnl)) {
            ++no_fx;
            continue;
        }
        total_invested += it.invested_value;
        total_current += it.current_value;
        total_pnl += it.pnl;
        if (it.prev_close > 0.0)
            total_day_pnl += it.quantity * (it.ltp - it.prev_close) * meta_fx(it.symbol);
    }
    const double total_pct = total_invested > 0.0 ? (total_pnl / total_invested) * 100.0 : 0.0;
    // Same currency + digit grouping as the table cells and the Positions strip.
    if (holdings_current_label_)
        holdings_current_label_->setText(eqbp_money(total_current, currency_) + eqbp_partial_note(no_fx));
    if (holdings_pnl_label_) {
        holdings_pnl_label_->setText(eqbp_money(total_pnl, currency_, true));
        set_style_if_changed(holdings_pnl_label_,
                             QString("color:%1;font-size:" + fincept::ui::fonts::px(-1) + "px;font-weight:700;")
                                 .arg(total_pnl >= 0 ? fincept::ui::colors::POSITIVE()
                                                     : fincept::ui::colors::NEGATIVE()));
    }
    if (holdings_day_pnl_label_) {
        holdings_day_pnl_label_->setText(eqbp_money(total_day_pnl, currency_, true));
        set_style_if_changed(holdings_day_pnl_label_,
                             QString("color:%1;font-size:" + fincept::ui::fonts::px(-1) + "px;font-weight:700;")
                                 .arg(total_day_pnl >= 0 ? fincept::ui::colors::POSITIVE()
                                                         : fincept::ui::colors::NEGATIVE()));
    }
    if (holdings_pnl_pct_label_) {
        holdings_pnl_pct_label_->setText(fincept::ui::formatting::pct(total_pct, 2, true));
        set_style_if_changed(holdings_pnl_pct_label_,
                             QString("color:%1;font-size:" + fincept::ui::fonts::px(-1) + "px;font-weight:700;")
                                 .arg(total_pct >= 0 ? fincept::ui::colors::POSITIVE()
                                                     : fincept::ui::colors::NEGATIVE()));
    }
}

void EquityBottomPanel::update_quote(const QString& symbol, const trading::BrokerQuote& quote) {
    const double ltp = quote.ltp;
    if (ltp <= 0.0 || symbol.isEmpty())
        return; // ignore empty / zero-price ticks — keep the last good values

    {
        // [posdbg] throttled — which quote symbols arrive vs the position symbols.
        static qint64 s_last = 0;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - s_last > 3000) {
            s_last = now;
            QStringList ps;
            if (is_paper_)
                for (const auto& p : last_paper_positions_)
                    ps << p.symbol;
            else
                for (const auto& p : last_positions_)
                    ps << p.symbol;
            LOG_DEBUG("EquityTrading", QString("update_quote sym='%1' ltp=%2 (%3) positions=[%4]")
                                   .arg(symbol)
                                   .arg(ltp, 0, 'f', 2)
                                   .arg(is_paper_ ? "paper" : "live", ps.join(',')));
        }
    }

    // Holdings (CNC) live the same quote stream as positions — patch them too.
    // quote.close is today's previous-day close → drives the "Today's P&L" column.
    update_holding_quote(symbol, ltp, quote.close);

    // Columns shift by +1 vs the pre-Product layout: LTP=6, P&L=7, P&L%=8.
    auto paint = [&](int row, double pnl, double pnl_pct) {
        set_right_cell(ensure_item(positions_table_, row, 6), px_text(symbol, ltp));
        set_right_cell(ensure_item(positions_table_, row, 7), eqbp_money(pnl, currency_, true), pnl_qcolor(pnl));
        set_right_cell(ensure_item(positions_table_, row, 8), fincept::ui::formatting::pct(pnl_pct, 2, true),
                       pnl_qcolor(pnl_pct));
    };

    if (is_paper_) {
        // Paper engine convention: positive quantity + side ("long"/"short").
        // Mirror PaperTradingRepository::update_position_price exactly so the row
        // and the engine never diverge. P&L shown in the account currency.
        const double fx = meta_fx(symbol);
        for (int i = 0; i < last_paper_positions_.size(); ++i) {
            auto& p = last_paper_positions_[i];
            if (p.symbol != symbol)
                continue;
            p.current_price = ltp;
            p.unrealized_pnl = (p.side == QLatin1String("long")) ? (ltp - p.entry_price) * p.quantity
                                                                 : (p.entry_price - ltp) * p.quantity;
            const double notional = p.entry_price * p.quantity;
            const double pct = notional != 0.0 ? (p.unrealized_pnl / notional) * 100.0 : 0.0;
            paint(i, p.unrealized_pnl * fx, pct);
            set_right_cell(ensure_item(positions_table_, i, 6), px_text(symbol, ltp));
        }
        update_positions_summary();
        return;
    }

    // Live broker positions. Quantity may be signed (e.g. Fyers netQty < 0 for
    // shorts) or positive-with-side; honor the side flag so direction is correct
    // regardless of broker convention.
    for (int i = 0; i < last_positions_.size(); ++i) {
        auto& p = last_positions_[i];
        if (p.symbol != symbol)
            continue;
        if (p.quantity == 0.0) {
            // A squared-off position (Zerodha & co. keep it in the net list with qty 0)
            // carries its REALISED P&L in `pnl`. Re-marking it as (ltp - avg) * 0 would
            // wipe that figure — and the net-P&L total — on every tick; only the LTP moves.
            p.ltp = ltp;
            set_right_cell(ensure_item(positions_table_, i, 6), px_text(symbol, ltp));
            continue;
        }
        double signed_qty = p.quantity;
        if (p.quantity > 0 && p.side.startsWith(QLatin1Char('s'), Qt::CaseInsensitive))
            signed_qty = -p.quantity; // broker reports a short as +qty with "sell"/"short"
        p.ltp = ltp;
        p.pnl = (ltp - p.avg_price) * signed_qty;
        const double notional = p.avg_price * qAbs(p.quantity);
        p.pnl_pct = notional > 0.0 ? (p.pnl / notional) * 100.0 : 0.0;
        paint(i, p.pnl, p.pnl_pct);
    }
    update_positions_summary();
}

void EquityBottomPanel::set_holdings(const QVector<trading::BrokerHolding>& holdings) {
    // Carry forward prev_close (today's close, learned from the live quote feed)
    // across REST holdings refreshes so the "Today's P&L" column/total don't blink
    // to 0 on every poll — the REST payload doesn't carry prev_close.
    QHash<QString, double> prev_close_by_sym;
    for (const auto& it : last_holdings_)
        if (it.prev_close > 0.0)
            prev_close_by_sym.insert(it.symbol, it.prev_close);
    last_holdings_ = holdings;
    for (auto& it : last_holdings_) {
        const auto pc = prev_close_by_sym.constFind(it.symbol);
        if (pc != prev_close_by_sym.constEnd())
            it.prev_close = pc.value();
    }
    if (holdings_import_btn_)
        holdings_import_btn_->setEnabled(!last_holdings_.isEmpty());
    if (holdings_square_off_btn_)
        holdings_square_off_btn_->setEnabled(!last_holdings_.isEmpty());
    populate_holdings_view();
}

void EquityBottomPanel::populate_holdings_view() {
    if (!holdings_table_)
        return; // tab not built yet — build_holdings_tab() paints last_holdings_
    const auto& holdings = last_holdings_;

    // Disable sorting during population so setItem assignments stay at intended rows.
    const auto view = capture_view(holdings_table_, 0);
    holdings_table_->setUpdatesEnabled(false);
    const bool was_sorting = holdings_table_->isSortingEnabled();
    holdings_table_->setSortingEnabled(false);
    holdings_table_->setRowCount(holdings.size());

    double total_invested = 0.0;
    double total_current = 0.0;
    double total_pnl = 0.0;
    double total_day_pnl = 0.0;
    int no_fx = 0; // holdings without a rate to the account currency: left out of the totals

    const QColor pos_color(fincept::ui::colors::POSITIVE());
    const QColor neg_color(fincept::ui::colors::NEGATIVE());

    for (int i = 0; i < last_holdings_.size(); ++i) {
        const auto& h = last_holdings_[i]; // carries prev_close across refreshes
        ensure_item(holdings_table_, i, 0)->setText(h.symbol);
        set_num_cell(ensure_num_item(holdings_table_, i, 1), h.quantity, format_quantity(h.quantity));
        set_num_cell(ensure_num_item(holdings_table_, i, 2), h.avg_price, px_text(h.symbol, h.avg_price));
        set_num_cell(ensure_num_item(holdings_table_, i, 3), h.ltp, px_text(h.symbol, h.ltp));
        // Money columns are in the account currency (named in their headers).
        set_num_cell(ensure_num_item(holdings_table_, i, 4), h.invested_value, eqbp_amount(h.invested_value, currency_));
        set_num_cell(ensure_num_item(holdings_table_, i, 5), h.current_value, eqbp_amount(h.current_value, currency_));

        auto* pnl_item = ensure_num_item(holdings_table_, i, 6);
        set_num_cell(pnl_item, h.pnl, eqbp_amount(h.pnl, currency_));
        pnl_item->setForeground(h.pnl >= 0 ? pos_color : neg_color);
        const bool valued = std::isfinite(h.pnl) && std::isfinite(h.current_value) && std::isfinite(h.invested_value);
        pnl_item->setToolTip(valued ? QString() : fx_missing_tip(h.symbol));

        auto* pct_item = ensure_num_item(holdings_table_, i, 7);
        set_num_cell(pct_item, h.pnl_pct, fincept::ui::formatting::pct(h.pnl_pct, 2, false));
        pct_item->setForeground(h.pnl_pct >= 0 ? pos_color : neg_color);

        // Today's P&L = qty * (LTP - prev close). prev_close starts 0 (shown as 0)
        // and is filled from the live quote feed in update_holding_quote — generic
        // across brokers, no per-broker holdings parser needed.
        const double day_pnl = h.prev_close > 0.0 ? h.quantity * (h.ltp - h.prev_close) * meta_fx(h.symbol) : 0.0;
        auto* day_item = ensure_num_item(holdings_table_, i, 8);
        set_num_cell(day_item, day_pnl, eqbp_amount(day_pnl, currency_));
        day_item->setForeground(day_pnl >= 0 ? pos_color : neg_color);

        // Per-row SELL → square off just this holding (market, CNC). Routed through
        // the screen's close_position path (same as SQUARE OFF ALL) so it reliably
        // reduces the holding instead of opening a counter-position.
        // Reuse the row's existing SELL button; only its bound symbol/exchange
        // change. Previously this allocated a QWidget + QHBoxLayout + QPushButton
        // on EVERY holdings push, and setCellWidget() destroyed the previous
        // occupant — so each broker refresh churned N button subtrees (each with
        // a connect, a tooltip and a tr() accessible name) and dropped keyboard
        // focus out of the action cell. The rest of this function was already
        // careful (updates disabled, sorting off, item reuse via ensure_item);
        // only the action cell was missed. Same shape as PaperBlotterPanel and
        // CryptoBottomPanel: the handler reads dynamic properties, so the
        // connection is made exactly once.
        auto* sell_btn = qobject_cast<QPushButton*>(holdings_table_->cellWidget(i, 9));
        if (!sell_btn) {
            sell_btn = new QPushButton(tr("SELL"));
            sell_btn->setObjectName("eqTableBtn");
            sell_btn->setProperty("act", "sell");
            sell_btn->setProperty("fincept_agent_risk", "order");
            sell_btn->setFixedHeight(cell_button_height());
            sell_btn->setCursor(Qt::PointingHandCursor);
            connect(sell_btn, &QPushButton::clicked, this, [this, sell_btn]() {
                const QString sym = sell_btn->property("fnSymbol").toString();
                const QString exch = sell_btn->property("fnExchange").toString();
                const double qty = sell_btn->property("fnQty").toDouble();
                LOG_DEBUG("EquityTrading",
                          QString("[panel] per-row SELL clicked: sym='%1' exch='%2' qty=%3").arg(sym, exch).arg(qty));
                emit square_off_holding_requested(sym, exch, qty);
            });
            holdings_table_->setCellWidget(i, 9, sell_btn);
        }
        sell_btn->setProperty("fnSymbol", h.symbol);
        sell_btn->setProperty("fnExchange", h.exchange);
        sell_btn->setProperty("fnQty", h.quantity);
        sell_btn->setAccessibleName(tr("Square off holding %1").arg(h.symbol));
        sell_btn->setToolTip(tr("Square off %1 — sells the full holding at market").arg(h.symbol));

        if (!valued) {
            ++no_fx;
            continue;
        }
        total_invested += h.invested_value;
        total_current += h.current_value;
        total_pnl += h.pnl;
        if (std::isfinite(day_pnl))
            total_day_pnl += day_pnl;
    }

    const double total_pct = total_invested > 0 ? (total_pnl / total_invested) * 100.0 : 0.0;

    if (holdings_count_label_)
        holdings_count_label_->setText(QString::number(holdings.size()));
    // Same currency + digit grouping as the table cells and the Positions strip.
    if (holdings_invested_label_)
        holdings_invested_label_->setText(eqbp_money(total_invested, currency_) + eqbp_partial_note(no_fx));
    if (holdings_current_label_)
        holdings_current_label_->setText(eqbp_money(total_current, currency_) + eqbp_partial_note(no_fx));
    if (holdings_pnl_label_) {
        holdings_pnl_label_->setText(eqbp_money(total_pnl, currency_, true));
        set_style_if_changed(holdings_pnl_label_,
                             QString("color:%1;font-size:" + fincept::ui::fonts::px(-1) + "px;font-weight:700;")
                                 .arg(total_pnl >= 0 ? fincept::ui::colors::POSITIVE()
                                                     : fincept::ui::colors::NEGATIVE()));
    }
    if (holdings_day_pnl_label_) {
        holdings_day_pnl_label_->setText(eqbp_money(total_day_pnl, currency_, true));
        set_style_if_changed(holdings_day_pnl_label_,
                             QString("color:%1;font-size:" + fincept::ui::fonts::px(-1) + "px;font-weight:700;")
                                 .arg(total_day_pnl >= 0 ? fincept::ui::colors::POSITIVE()
                                                         : fincept::ui::colors::NEGATIVE()));
    }
    if (holdings_pnl_pct_label_) {
        holdings_pnl_pct_label_->setText(fincept::ui::formatting::pct(total_pct, 2, true));
        set_style_if_changed(holdings_pnl_pct_label_,
                             QString("color:%1;font-size:" + fincept::ui::fonts::px(-1) + "px;font-weight:700;")
                                 .arg(total_pct >= 0 ? fincept::ui::colors::POSITIVE()
                                                     : fincept::ui::colors::NEGATIVE()));
    }

    holdings_table_->setSortingEnabled(was_sorting);
    holdings_table_->setUpdatesEnabled(true);
    restore_view(holdings_table_, 0, view);
}

void EquityBottomPanel::set_orders(const QVector<trading::BrokerOrderInfo>& orders) {
    last_orders_ = orders; // re-rendered on a format change
    orders_are_paper_ = false;
    const auto view = capture_view(orders_table_, OC_OrderId);
    // Rows are written by index: sorting off while filling, restored (re-sorted) after.
    const bool was_sorting = orders_table_->isSortingEnabled();
    orders_table_->setSortingEnabled(false);
    orders_table_->setUpdatesEnabled(false);
    orders_table_->setRowCount(orders.size());
    const QString dash = fincept::ui::formatting::placeholder();
    for (int i = 0; i < orders.size(); ++i) {
        const auto& o = orders[i];
        const QTimeZone naive_zone = eqbp_naive_order_zone(o.exchange);
        eqbp_set_time_cell(ensure_num_item(orders_table_, i, OC_Time), o.timestamp, naive_zone, naive_zone);
        ensure_item(orders_table_, i, OC_Symbol)->setText(o.symbol);
        auto* side_item = ensure_item(orders_table_, i, OC_Side);
        side_item->setText(o.side.toUpper());
        side_item->setForeground(side_qcolor(o.side));
        ensure_item(orders_table_, i, OC_Type)->setText(o.order_type.toUpper());
        ensure_item(orders_table_, i, OC_Product)
            ->setText(o.product_type.isEmpty() ? "--" : o.product_type.toUpper());

        auto num = [&](int col, double value, const QString& text, const QColor& fg = QColor()) {
            auto* it = ensure_num_item(orders_table_, i, col);
            set_num_cell(it, value, text);
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            it->setForeground(fg.isValid() ? fg : QColor(fincept::ui::colors::TEXT_PRIMARY()));
            return it;
        };
        num(OC_Qty, o.quantity, format_quantity(o.quantity));
        // Filled / ordered. A broker that does not report fills leaves 0 — on an
        // order it says is filled that is "unknown", not "nothing filled".
        const QString st = o.status.trimmed().toLower();
        const bool filled_status = st == QLatin1String("filled") || st == QLatin1String("complete") ||
                                   st == QLatin1String("completed") || st == QLatin1String("traded") ||
                                   st == QLatin1String("executed");
        const bool fill_unknown = o.filled_qty <= 0.0 && filled_status;
        const bool partial = o.filled_qty > 0.0 && o.filled_qty < o.quantity;
        num(OC_Filled, fill_unknown ? -1.0 : o.filled_qty,
            fill_unknown ? dash : format_quantity(o.filled_qty) + QStringLiteral("/") + format_quantity(o.quantity),
            partial ? QColor(fincept::ui::colors::AMBER()) : QColor());
        // A market order has no price; "0.00" reads as a real (absurd) limit. Prices
        // are as quoted (the symbol's own currency when known, else the account's).
        num(OC_Price, o.price > 0.0 ? o.price : 0.0, o.price > 0.0 ? px_text(o.symbol, o.price) : tr("MKT"));
        const double trig = o.trigger_price > 0.0 ? o.trigger_price : (o.stop_price > 0.0 ? o.stop_price : 0.0);
        num(OC_Trigger, trig, trig > 0.0 ? px_text(o.symbol, trig) : dash);
        num(OC_AvgFill, o.avg_price > 0.0 ? o.avg_price : 0.0,
            o.avg_price > 0.0 ? px_text(o.symbol, o.avg_price) : dash);

        // Status keeps its colour; the broker's message (reject / cancel reason)
        // rides in the tooltip, flagged with "ⓘ" so there is a reason to hover.
        auto* status_item = ensure_item(orders_table_, i, OC_Status);
        const QString msg = o.message.trimmed();
        const bool explain = !msg.isEmpty() && (st == QLatin1String("rejected") || st == QLatin1String("cancelled") ||
                                                st == QLatin1String("canceled"));
        status_item->setText(explain ? o.status.toUpper() + QStringLiteral(" ⓘ") : o.status.toUpper());
        status_item->setForeground(order_status_color(o.status));
        status_item->setToolTip(msg);
        auto* id_item = ensure_item(orders_table_, i, OC_OrderId);
        id_item->setText(o.order_id.left(12));
        id_item->setToolTip(o.order_id);

        // Action column — EDIT + CANCEL for every order that is still working.
        const bool modifiable = !order_is_terminal(o.status);
        if (modifiable) {
            auto* btn = new QPushButton(tr("EDIT"));
            btn->setObjectName("eqTableBtn");
            btn->setProperty("act", "edit");
            btn->setProperty("fincept_agent_risk", "order");
            btn->setMinimumHeight(18);
            btn->setAccessibleName(tr("Modify order %1").arg(o.order_id.left(12)));
            btn->setCursor(Qt::PointingHandCursor);
            const QString oid = o.order_id;
            const double qty = o.quantity;
            const double prc = o.price;
            connect(btn, &QPushButton::clicked, this,
                    [this, oid, qty, prc]() { open_modify_dialog(oid, qty, prc, tr("LIMIT PRICE")); });
            // CANCEL — kills the open order. Confirmed here because on_cancel_order
            // cancels immediately with no downstream prompt and equity orders are live.
            auto* cancel_row_btn = new QPushButton(tr("CANCEL"));
            cancel_row_btn->setObjectName("eqTableBtn");
            cancel_row_btn->setProperty("act", "cancel");
            cancel_row_btn->setProperty("fincept_agent_risk", "order");
            cancel_row_btn->setMinimumHeight(18);
            cancel_row_btn->setAccessibleName(tr("Cancel order %1").arg(o.order_id.left(12)));
            cancel_row_btn->setCursor(Qt::PointingHandCursor);
            const QString cancel_oid = o.order_id;
            connect(cancel_row_btn, &QPushButton::clicked, this, [this, cancel_oid]() {
                if (QMessageBox::question(this, tr("Cancel Order"), tr("Cancel order %1?").arg(cancel_oid.left(12)),
                                          QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
                    emit cancel_order_requested(cancel_oid);
            });

            auto* action_cell = new QWidget;
            auto* action_lay = new QHBoxLayout(action_cell);
            action_lay->setContentsMargins(2, 0, 2, 0);
            action_lay->setSpacing(4);
            action_lay->addWidget(btn);
            action_lay->addWidget(cancel_row_btn);
            action_lay->addStretch();
            orders_table_->setCellWidget(i, OC_Action, action_cell);
        } else {
            orders_table_->setCellWidget(i, OC_Action, nullptr);
        }
    }
    orders_table_->setSortingEnabled(was_sorting);
    orders_table_->setUpdatesEnabled(true);
    restore_view(orders_table_, OC_OrderId, view);
}

// ── Auctions Tab ────────────────────────────────────────────────────────────

void EquityBottomPanel::setup_auctions_tab() {
    auctions_table_ = new QTableWidget;
    auctions_table_->setObjectName("eqTable");
    auctions_table_->setColumnCount(6);
    auctions_table_->setHorizontalHeaderLabels(
        {tr("Date"), tr("Type"), tr("Time"), tr("Price"), tr("Size"), tr("Exchange")});
    auctions_table_->verticalHeader()->setVisible(false);
    auctions_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    auctions_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auctions_table_->setShowGrid(false);
    auctions_table_->horizontalHeader()->setStretchLastSection(true);
    auctions_table_->verticalHeader()->setDefaultSectionSize(table_row_height(-2, 20));
    auctions_table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    auctions_tab_idx_ = tabs_->addTab(auctions_table_, tr("AUCTIONS"));
}

void EquityBottomPanel::set_auctions(const QVector<trading::BrokerAuction>& auctions) {
    auctions_table_->setUpdatesEnabled(false);
    auctions_table_->setRowCount(0);
    for (const auto& auction : auctions) {
        for (const auto& entry : auction.entries) {
            const int row = auctions_table_->rowCount();
            auctions_table_->insertRow(row);
            const bool is_open = (entry.auction_type == "O");
            const QColor type_color =
                is_open ? QColor(fincept::ui::colors::AMBER()) : QColor(fincept::ui::colors::INFO());

            auto set = [&](int col, const QString& text,
                           const QColor& fg = QColor(fincept::ui::colors::TEXT_PRIMARY())) {
                auto* item = new QTableWidgetItem(text);
                item->setForeground(fg);
                item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                auctions_table_->setItem(row, col, item);
            };

            const QString time_str =
                QDateTime::fromString(entry.timestamp, Qt::ISODateWithMs).toLocalTime().toString("hh:mm:ss");
            set(0, auction.date, QColor(fincept::ui::colors::TEXT_SECONDARY()));
            set(1, is_open ? tr("OPEN") : tr("CLOSE"), type_color);
            set(2, time_str, QColor(fincept::ui::colors::TEXT_SECONDARY()));
            set(3, px_text(auction.symbol, entry.price));
            set(4, format_quantity(entry.size), QColor(fincept::ui::colors::TEXT_SECONDARY()));
            set(5, entry.exchange, QColor(fincept::ui::colors::TEXT_SECONDARY()));
            for (int col = 3; col <= 4; ++col)
                if (auto* it = auctions_table_->item(row, col))
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    auctions_table_->setUpdatesEnabled(true);
    if (auctions_tab_idx_ >= 0)
        tabs_->setTabVisible(auctions_tab_idx_, us_tabs_visible_ && auctions_table_->rowCount() > 0);
}

void EquityBottomPanel::set_condition_codes(const QMap<QString, QString>& codes) {
    condition_codes_ = codes;
}

// ── Time & Sales Tab ────────────────────────────────────────────────────────

void EquityBottomPanel::setup_time_sales_tab() {
    time_sales_table_ = new QTableWidget;
    time_sales_table_->setObjectName("eqTable");
    time_sales_table_->setColumnCount(6);
    time_sales_table_->setHorizontalHeaderLabels(
        {tr("Time"), tr("Price"), tr("Size"), tr("Exchange"), tr("Conditions"), tr("Tape")});
    time_sales_table_->verticalHeader()->setVisible(false);
    time_sales_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    time_sales_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    time_sales_table_->setShowGrid(false);
    time_sales_table_->horizontalHeader()->setStretchLastSection(true);
    time_sales_table_->verticalHeader()->setDefaultSectionSize(table_row_height(-2, 20));
    time_sales_table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    time_sales_table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    time_sales_tab_idx_ = tabs_->addTab(time_sales_table_, fincept::ui::literal_ampersands(tr("TIME & SALES")));
}

static void fill_trade_row(QTableWidget* table, int row, const trading::BrokerTrade& t,
                           const QMap<QString, QString>& codes = {}) {
    // Parse ISO timestamp → time only
    const QString time_str =
        QDateTime::fromString(t.timestamp, Qt::ISODateWithMs).toLocalTime().toString("hh:mm:ss.zzz");

    auto set = [&](int col, const QString& text, const QColor& fg = QColor(fincept::ui::colors::TEXT_PRIMARY())) {
        auto* item = new QTableWidgetItem(text);
        item->setForeground(fg);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table->setItem(row, col, item);
    };

    set(0, time_str.isEmpty() ? t.timestamp.right(12) : time_str, QColor(fincept::ui::colors::TEXT_SECONDARY()));
    // Price as quoted (the tape is the account's US market; no unit in a price
    // column), size with its fraction (Alpaca prints fractional shares); both right-aligned.
    set(1, fincept::ui::formatting::price(t.price),
        t.price > 0 ? QColor(fincept::ui::colors::TEXT_PRIMARY()) : QColor(fincept::ui::colors::TEXT_SECONDARY()));
    set(2, format_quantity(t.size), QColor(fincept::ui::colors::TEXT_SECONDARY()));
    for (int col = 1; col <= 2; ++col)
        if (auto* it = table->item(row, col))
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    set(3, t.exchange, QColor(fincept::ui::colors::TEXT_SECONDARY()));
    QStringList decoded;
    for (const auto& c : t.conditions)
        decoded.append(codes.contains(c) ? codes[c] : c);
    set(4, decoded.join(", "), QColor(fincept::ui::colors::TEXT_TERTIARY()));
    set(5, t.tape, QColor(fincept::ui::colors::TEXT_TERTIARY()));
}

void EquityBottomPanel::set_time_sales(const QVector<trading::BrokerTrade>& trades) {
    time_sales_table_->setUpdatesEnabled(false);
    time_sales_table_->setRowCount(trades.size());
    for (int i = 0; i < trades.size(); ++i)
        fill_trade_row(time_sales_table_, i, trades[i], condition_codes_);
    time_sales_table_->setUpdatesEnabled(true);
    // Scroll to most recent (bottom — trades arrive oldest-first)
    if (!trades.isEmpty())
        time_sales_table_->scrollToBottom();
}

void EquityBottomPanel::prepend_trade(const trading::BrokerTrade& trade) {
    // Insert at row 0 (newest at top), cap at 500 rows to avoid unbounded growth
    time_sales_table_->insertRow(0);
    fill_trade_row(time_sales_table_, 0, trade, condition_codes_);
    if (time_sales_table_->rowCount() > 500)
        time_sales_table_->removeRow(500);
}

// ── Calendar Tab ───────────────────────────────────────────────────────────

void EquityBottomPanel::setup_calendar_tab() {
    auto* container = new QWidget(this);
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);

    // Clock status banner
    auto* banner = new QWidget(this);
    banner->setObjectName("calClockBanner");
    banner->setMinimumHeight(28);
    banner->setStyleSheet(QString("QWidget#calClockBanner { background: %1; border-bottom: 1px solid %2; }")
                              .arg(fincept::ui::colors::BG_BASE())
                              .arg(fincept::ui::colors::BORDER_MED()));
    auto* banner_lay = new QHBoxLayout(banner);
    banner_lay->setContentsMargins(10, 0, 10, 0);
    banner_lay->setSpacing(16);

    clock_status_label_ = new QLabel(tr("● MARKET --"));
    clock_status_label_->setObjectName("calClockStatus");
    clock_status_label_->setStyleSheet(
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700;").arg(fincept::ui::colors::TEXT_TERTIARY()));

    clock_next_label_ = new QLabel("");
    clock_next_label_->setObjectName("calClockNext");
    clock_next_label_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px;").arg(fincept::ui::colors::TEXT_TERTIARY()));

    banner_lay->addWidget(clock_status_label_);
    banner_lay->addWidget(clock_next_label_);
    banner_lay->addStretch();
    vlay->addWidget(banner);

    // Calendar table
    calendar_table_ = new QTableWidget;
    calendar_table_->setObjectName("eqTable");
    calendar_table_->setColumnCount(5);
    calendar_table_->setHorizontalHeaderLabels(
        {tr("Date"), tr("Open (ET)"), tr("Close (ET)"), tr("Pre-Market"), tr("After-Hours")});
    calendar_table_->verticalHeader()->setVisible(false);
    calendar_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    calendar_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    calendar_table_->setShowGrid(false);
    calendar_table_->horizontalHeader()->setStretchLastSection(true);
    calendar_table_->verticalHeader()->setDefaultSectionSize(table_row_height(-2, 22));
    // Pixel-level smooth scrolling
    calendar_table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    calendar_table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    vlay->addWidget(calendar_table_);

    calendar_tab_idx_ = tabs_->addTab(container, tr("CALENDAR"));
}

void EquityBottomPanel::set_calendar(const QVector<trading::MarketCalendarDay>& days) {
    calendar_table_->setRowCount(days.size());
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");
    for (int i = 0; i < days.size(); ++i) {
        const auto& d = days[i];
        const bool is_today = (d.date == today);

        auto set = [&](int col, const QString& text) {
            auto* item = ensure_item(calendar_table_, i, col);
            item->setText(text);
            // Items are reused across reloads, so a row that was "today" in the previous
            // load must have its highlight cleared (calendar refetched after midnight).
            item->setForeground(is_today ? QColor(fincept::ui::colors::AMBER())
                                         : QColor(fincept::ui::colors::TEXT_PRIMARY()));
            item->setBackground(is_today ? QBrush(QColor(fincept::ui::colors::ACCENT_BG())) : QBrush());
        };
        set(0, d.date);
        set(1, d.open);
        set(2, d.close);
        set(3, d.session_open.isEmpty() ? "--" : d.session_open);
        set(4, d.session_close.isEmpty() ? "--" : d.session_close);
    }
    // Scroll to today (or nearest future day) centred in the view
    for (int i = 0; i < days.size(); ++i) {
        if (days[i].date >= today) {
            calendar_table_->scrollToItem(calendar_table_->item(i, 0), QAbstractItemView::PositionAtCenter);
            break;
        }
    }
}

void EquityBottomPanel::set_clock(const trading::MarketClock& clock) {
    if (!clock_status_label_)
        return;

    if (clock.is_open) {
        clock_status_label_->setText(tr("● MARKET OPEN"));
        clock_status_label_->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700;").arg(fincept::ui::colors::POSITIVE()));
    } else {
        clock_status_label_->setText(tr("● MARKET CLOSED"));
        clock_status_label_->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700;").arg(fincept::ui::colors::NEGATIVE()));
    }

    // Parse ISO timestamps and show local-friendly next event
    if (!clock.next_open.isEmpty() || !clock.next_close.isEmpty()) {
        const QString next_event = clock.is_open ? tr("Closes %1")
                                                       .arg(QDateTime::fromString(clock.next_close, Qt::ISODateWithMs)
                                                                .toLocalTime()
                                                                .toString("MMM d h:mm ap"))
                                                 : tr("Opens %1")
                                                       .arg(QDateTime::fromString(clock.next_open, Qt::ISODateWithMs)
                                                                .toLocalTime()
                                                                .toString("MMM d h:mm ap"));
        clock_next_label_->setText(next_event);
        clock_next_label_->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px;").arg(fincept::ui::colors::TEXT_SECONDARY()));
    }
}

} // namespace fincept::screens::equity
